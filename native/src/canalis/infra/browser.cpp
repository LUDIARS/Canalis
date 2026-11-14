#include "canalis/infra/browser.hpp"

#include <filesystem>
#include <stdexcept>
#include <thread>

#include "canalis/base/crypto.hpp"
#include "canalis/base/env.hpp"
#include "canalis/base/fsutil.hpp"
#include "canalis/infra/browser_process.hpp"

namespace canalis::cdp {
namespace {

std::vector<std::string> candidate_paths() {
    std::vector<std::string> out;
#ifdef _WIN32
    const char* roots[] = {"ProgramFiles(x86)", "ProgramFiles", "LOCALAPPDATA"};
    const char* rels[] = {"Microsoft/Edge/Application/msedge.exe", "Google/Chrome/Application/chrome.exe",
                          "Chromium/Application/chrome.exe"};
    for (const char* rel : rels)
        for (const char* root : roots)
            if (auto base = env::get_nonempty(root)) out.push_back(fs::join(*base, rel));
#elif defined(__APPLE__)
    out = {"/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge",
           "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
           "/Applications/Chromium.app/Contents/MacOS/Chromium"};
#else
    out = {"/usr/bin/microsoft-edge", "/usr/bin/microsoft-edge-stable", "/usr/bin/google-chrome",
           "/usr/bin/google-chrome-stable", "/usr/bin/chromium", "/usr/bin/chromium-browser", "/snap/bin/chromium"};
#endif
    return out;
}

void remove_profile(const std::string& dir) {
    if (dir.empty()) return;
    // ブラウザの終了直後はファイルを掴んでいることがあるので、少し待ちながら数回試す。
    for (int attempt = 0; attempt < 10; ++attempt) {
        std::error_code ec;
        std::filesystem::remove_all(fs::to_path(dir), ec);
        if (!ec && !fs::exists(dir)) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

} // namespace

std::optional<std::string> find_browser() {
    if (auto configured = env::get_nonempty("CANALIS_BROWSER")) {
        if (fs::is_file(*configured)) return configured;
        return std::nullopt;  // 指定されたのに無いときは、別のブラウザで黙って続けない
    }
    for (const auto& path : candidate_paths())
        if (fs::is_file(path)) return path;
    return std::nullopt;
}

std::vector<std::string> launch_args(const std::string& profile_dir, const std::vector<std::string>& extra_args) {
    std::vector<std::string> args = {
        "--headless=new",
        "--user-data-dir=" + profile_dir,
        "--no-first-run",
        "--no-default-browser-check",
        "--disable-extensions",
        "--disable-sync",
        "--disable-background-networking",
        "--disable-component-update",
        "--disable-default-apps",
        "--disable-domain-reliability",
        "--disable-client-side-phishing-detection",
        "--metrics-recording-only",
        "--mute-audio",
        "--hide-scrollbars",
    };
    args.insert(args.end(), extra_args.begin(), extra_args.end());
    args.emplace_back("about:blank");
    return args;
}

Browser::Browser(BrowserOptions options) {
    std::string exe = options.executable;
    if (exe.empty()) {
        auto found = find_browser();
        if (!found) {
            throw std::runtime_error(
                "browser: Edge / Chrome が見つかりません (CANALIS_BROWSER に実行ファイルのパスを指定してください)");
        }
        exe = *found;
    }
    profile_dir_ = fs::from_path(std::filesystem::temp_directory_path() /
                                 fs::to_path("canalis-browser-" + crypto::hex_encode(crypto::random_bytes(8))));
    if (!fs::ensure_dir(profile_dir_)) throw std::runtime_error("browser: failed to create profile dir");
    try {
        LaunchOptions launch;
        launch.executable = exe;
        launch.args = launch_args(profile_dir_, options.extra_args);
        transport_ = std::make_unique<BrowserProcess>(launch);
        client_ = std::make_unique<Client>(*transport_);
        client_->call("Browser.getVersion", Json::object(), options.startup_timeout);
    } catch (...) {
        close();
        throw;
    }
}

Browser::Browser(std::unique_ptr<Transport> transport) : transport_(std::move(transport)) {
    client_ = std::make_unique<Client>(*transport_);
}

Browser::~Browser() { close(); }

std::unique_ptr<Page> Browser::new_page(std::chrono::milliseconds timeout) {
    const Json target = client_->call("Target.createTarget", Json{{"url", "about:blank"}}, timeout);
    const std::string target_id = json_string(target, "targetId");
    if (target_id.empty()) throw CdpError("cdp: Target.createTarget returned no targetId");
    const Json attached =
        client_->call("Target.attachToTarget", Json{{"targetId", target_id}, {"flatten", true}}, timeout);
    const std::string session_id = json_string(attached, "sessionId");
    if (session_id.empty()) throw CdpError("cdp: Target.attachToTarget returned no sessionId");
    auto page = std::make_unique<Page>(Page::Key{}, *client_, target_id, session_id);
    client_->call("Page.enable", Json::object(), timeout, session_id);
    return page;
}

void Browser::close() {
    if (closed_) return;
    closed_ = true;
    if (client_ && transport_ && !transport_->closed()) {
        try {
            client_->call("Browser.close", Json::object(), std::chrono::milliseconds(3000));
        } catch (const std::exception&) {
            // 応答の前に切れることがある。下で必ず止める。
        }
    }
    if (auto* process = dynamic_cast<BrowserProcess*>(transport_.get())) process->close();
    client_.reset();
    transport_.reset();
    remove_profile(profile_dir_);
}

Page::Page(Key, Client& client, std::string target_id, std::string session_id)
    : client_(client), target_id_(std::move(target_id)), session_id_(std::move(session_id)) {}

Page::~Page() {
    try {
        close();
    } catch (...) {
        // デストラクタからは投げない
    }
}

void Page::navigate(const std::string& url, std::chrono::milliseconds timeout) {
    client_.drop_events(session_id_);
    const Json result = client_.call("Page.navigate", Json{{"url", url}}, timeout, session_id_);
    if (const auto error = json_opt_string(result, "errorText"); error && !error->empty())
        throw CdpError("navigation failed: " + *error + " " + url);
    if (!client_.wait_event("Page.domContentEventFired", session_id_, timeout))
        throw CdpError("navigation timed out (" + std::to_string(timeout.count()) + "ms): " + url);
}

Json Page::evaluate(const std::string& expression, std::chrono::milliseconds timeout) {
    const Json result = client_.call(
        "Runtime.evaluate", Json{{"expression", expression}, {"returnByValue", true}, {"awaitPromise", true}}, timeout,
        session_id_);
    if (const auto details = result.find("exceptionDetails"); details != result.end()) {
        const Json& exception = json_object(*details, "exception");
        std::string text = json_string(exception, "description");
        if (text.empty()) text = json_string(*details, "text", "exception");
        throw CdpError("evaluate failed: " + text);
    }
    const Json& value = json_object(result, "result");
    const auto v = value.find("value");
    return v != value.end() ? *v : Json(nullptr);
}

void Page::close() {
    if (closed_) return;
    closed_ = true;
    try {
        client_.call("Target.closeTarget", Json{{"targetId", target_id_}}, std::chrono::milliseconds(5000));
    } catch (const CdpError&) {
        // ブラウザが先に閉じていれば何もしない
    }
    client_.drop_events(session_id_);
}

} // namespace canalis::cdp
