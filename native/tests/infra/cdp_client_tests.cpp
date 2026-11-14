// CDP のクライアントと Browser / Page を、偽の transport (台本どおりに応答する偽のブラウザ) で確かめる。
// 本物のブラウザは起動しない (それは infra.browser_e2e、CANALIS_BROWSER_E2E=1 のときだけ)。
#include "check.hpp"

#include <algorithm>
#include <deque>
#include <functional>
#include <memory>

#include "canalis/infra/browser.hpp"
#include "canalis/infra/cdp_client.hpp"

using namespace canalis;
using std::chrono::milliseconds;

namespace {

/** 送られた要求ごとに handler を呼び、返したメッセージ (応答・イベント) を受信の列に積む。 */
class FakeBrowser final : public cdp::Transport {
public:
    using Handler = std::function<std::vector<Json>(const Json& request)>;
    explicit FakeBrowser(Handler handler) : handler_(std::move(handler)) {}
    std::vector<Json> requests;
    bool is_closed = false;

    bool send(std::string_view message) override {
        if (is_closed) return false;
        const Json req = parse_json_or_throw(message);
        requests.push_back(req);
        for (auto& m : handler_(req)) inbox_.push_back(json_stringify(m));
        return true;
    }
    std::optional<std::string> receive(milliseconds) override {
        if (inbox_.empty()) return std::nullopt;
        std::string m = std::move(inbox_.front());
        inbox_.pop_front();
        return m;
    }
    bool closed() const override { return is_closed && inbox_.empty(); }
    void push_raw(std::string m) { inbox_.push_back(std::move(m)); }

private:
    Handler handler_;
    std::deque<std::string> inbox_;
};

Json reply(const Json& req, Json result) { return Json{{"id", req["id"]}, {"result", std::move(result)}}; }

} // namespace

TEST_CASE("call は同じ id の応答を返し、間のイベントは溜めておく") {
    FakeBrowser fake([](const Json& req) {
        return std::vector<Json>{Json{{"method", "Page.loadEventFired"}, {"params", Json{{"t", 1}}}, {"sessionId", "S"}},
                                 Json{{"id", 999}, {"result", Json::object()}},  // 別の要求への応答 (読み捨てる)
                                 reply(req, Json{{"ok", true}})};
    });
    cdp::Client client(fake);
    const Json r = client.call("Browser.getVersion", Json::object(), milliseconds(1000));
    CHECK_EQ(r, (Json{{"ok", true}}));
    CHECK_EQ(json_string(fake.requests[0], "method"), std::string("Browser.getVersion"));
    CHECK(!fake.requests[0].contains("sessionId"));
    const auto event = client.wait_event("Page.loadEventFired", "S", milliseconds(0));
    CHECK(event.has_value());
    CHECK_EQ(json_int(*event, "t"), std::int64_t{1});
    CHECK(!client.wait_event("Page.loadEventFired", "S", milliseconds(0)).has_value());
}

TEST_CASE("応答の error・期限切れ・切断・壊れたメッセージは CdpError") {
    FakeBrowser errors([](const Json& req) {
        return std::vector<Json>{Json{{"id", req["id"]}, {"error", Json{{"code", -32000}, {"message", "No target"}}}}};
    });
    cdp::Client c1(errors);
    try {
        c1.call("Target.closeTarget", Json::object(), milliseconds(1000), "S1");
        CHECK(false);
    } catch (const cdp::CdpError& e) {
        CHECK(std::string(e.what()).find("No target") != std::string::npos);
    }
    CHECK_EQ(json_string(errors.requests[0], "sessionId"), std::string("S1"));

    FakeBrowser silent([](const Json&) { return std::vector<Json>{}; });
    cdp::Client c2(silent);
    CHECK_THROWS(c2.call("Page.enable", Json::object(), milliseconds(50)));

    FakeBrowser gone([](const Json&) { return std::vector<Json>{}; });
    gone.is_closed = true;
    cdp::Client c3(gone);
    CHECK_THROWS(c3.call("Page.enable", Json::object(), milliseconds(50)));

    FakeBrowser garbage([](const Json&) { return std::vector<Json>{}; });
    garbage.push_raw("{not json");
    cdp::Client c4(garbage);
    CHECK_THROWS(c4.call("Page.enable", Json::object(), milliseconds(50)));
}

TEST_CASE("上限を超える大きさのメッセージは受け取らない") {
    FakeBrowser big([](const Json& req) { return std::vector<Json>{reply(req, Json{{"v", std::string(2000, 'x')}})}; });
    cdp::Client client(big, JsonLimits{1000, 100, 8});
    CHECK_THROWS(client.call("Runtime.evaluate", Json::object(), milliseconds(100)));
}

TEST_CASE("Browser / Page: タブを開き、移動して評価し、閉じる") {
    auto fake = std::make_unique<FakeBrowser>([](const Json& req) -> std::vector<Json> {
        const std::string method = json_string(req, "method");
        if (method == "Target.createTarget") return {reply(req, Json{{"targetId", "T1"}})};
        if (method == "Target.attachToTarget") return {reply(req, Json{{"sessionId", "S1"}})};
        if (method == "Page.navigate") {
            const std::string url = json_string(json_object(req, "params"), "url");
            if (url == "https://bad.example/") return {reply(req, Json{{"errorText", "net::ERR_NAME_NOT_RESOLVED"}})};
            return {reply(req, Json{{"frameId", "F"}}),
                    Json{{"method", "Page.domContentEventFired"}, {"params", Json{{"timestamp", 1}}}, {"sessionId", "S1"}}};
        }
        if (method == "Runtime.evaluate") {
            const std::string expr = json_string(json_object(req, "params"), "expression");
            if (expr == "throw") {
                return {reply(req, Json{{"result", Json{{"type", "object"}}},
                                        {"exceptionDetails", Json{{"text", "Uncaught"},
                                                                  {"exception", Json{{"description", "Error: boom"}}}}}})};
            }
            return {reply(req, Json{{"result", Json{{"type", "number"}, {"value", 42}}}})};
        }
        return {reply(req, Json::object())};
    });
    FakeBrowser* raw = fake.get();
    cdp::Browser browser(std::move(fake));
    auto page = browser.new_page(milliseconds(1000));
    page->navigate("https://acme.notion.site/x", milliseconds(1000));
    CHECK_EQ(page->evaluate("1 + 41", milliseconds(1000)), Json(42));
    try {
        page->evaluate("throw", milliseconds(1000));
        CHECK(false);
    } catch (const cdp::CdpError& e) {
        CHECK(std::string(e.what()).find("Error: boom") != std::string::npos);
    }
    CHECK_THROWS(page->navigate("https://bad.example/", milliseconds(1000)));
    page->close();

    std::vector<std::string> methods;
    for (const auto& r : raw->requests) methods.push_back(json_string(r, "method"));
    CHECK_EQ(methods.at(0), std::string("Target.createTarget"));
    CHECK_EQ(methods.at(1), std::string("Target.attachToTarget"));
    CHECK_EQ(methods.at(2), std::string("Page.enable"));
    CHECK_EQ(json_string(raw->requests.at(2), "sessionId"), std::string("S1"));
    CHECK(json_bool(json_object(raw->requests.at(1), "params"), "flatten"));
    CHECK_EQ(methods.back(), std::string("Target.closeTarget"));
}

TEST_CASE("DOMContentLoaded が来なければ移動は期限切れ") {
    auto fake = std::make_unique<FakeBrowser>([](const Json& req) -> std::vector<Json> {
        const std::string method = json_string(req, "method");
        if (method == "Target.createTarget") return {reply(req, Json{{"targetId", "T"}})};
        if (method == "Target.attachToTarget") return {reply(req, Json{{"sessionId", "S"}})};
        return {reply(req, Json::object())};
    });
    cdp::Browser browser(std::move(fake));
    auto page = browser.new_page(milliseconds(500));
    CHECK_THROWS(page->navigate("https://acme.notion.site/x", milliseconds(50)));
}

TEST_CASE("起動引数: ヘッドレス・一時プロファイル・拡張と背景通信を止める") {
    const auto args = cdp::launch_args("/tmp/p", {"--proxy-server=http://127.0.0.1:9"});
    auto has = [&](const std::string& a) { return std::find(args.begin(), args.end(), a) != args.end(); };
    CHECK(has("--headless=new"));
    CHECK(has("--user-data-dir=/tmp/p"));
    CHECK(has("--disable-extensions"));
    CHECK(has("--disable-background-networking"));
    CHECK(has("--proxy-server=http://127.0.0.1:9"));
    CHECK_EQ(args.back(), std::string("about:blank"));
}

CI_TEST_MAIN()
