#include "canalis/crawl/notion/client.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <thread>

#include "canalis/base/env.hpp"
#include "canalis/base/strings.hpp"
#include "canalis/base/time.hpp"
#include "canalis/base/url.hpp"

namespace canalis::crawl::notion {
namespace {

// Notion の応答 1 件の上限 (ページネーションは 100 件ずつなので十分に大きい)。
constexpr std::size_t kMaxResponseBytes = 16u << 20;

Paged to_paged(const Json& json) {
    Paged out;
    for (const auto& r : json_array(json, "results")) out.results.push_back(r);
    out.next_cursor = json_opt_string(json, "next_cursor");
    out.has_more = json_bool(json, "has_more", false);
    return out;
}

/** id をパスの 1 区間として入れる (TS はそのまま連結するが、C++ は / や ? を含む id でパスを壊さないよう符号化する)。 */
std::string segment(const std::string& id) { return url::encode_component(id); }

} // namespace

NotionApiClient::NotionApiClient(NotionClientConfig config) : config_(std::move(config)) {
    if (config_.token.empty()) throw std::invalid_argument("Notion token is required");
    version_ = config_.notion_version.value_or("2022-06-28");
    base_url_ = config_.base_url.value_or("https://api.notion.com/v1");
    if (!base_url_.empty() && base_url_.back() == '/') base_url_.pop_back();
    min_interval_ms_ = config_.min_interval_ms.value_or(350);
    max_retries_ = config_.max_retries.value_or(4);
    if (!config_.fetch) config_.fetch = [](const http::ClientRequest& r) { return http::fetch(r); };
    if (!config_.sleep) config_.sleep = [](std::chrono::milliseconds d) { std::this_thread::sleep_for(d); };
}

Paged NotionApiClient::query_database(const std::string& database_id, const std::optional<std::string>& cursor) {
    Json body = Json::object();
    body["page_size"] = 100;
    if (cursor && !cursor->empty()) body["start_cursor"] = *cursor;
    return to_paged(request("POST", "/databases/" + segment(database_id) + "/query", body));
}

Paged NotionApiClient::get_block_children(const std::string& block_id, const std::optional<std::string>& cursor) {
    std::vector<std::pair<std::string, std::string>> qs{{"page_size", "100"}};
    if (cursor && !cursor->empty()) qs.emplace_back("start_cursor", *cursor);
    return to_paged(request("GET", "/blocks/" + segment(block_id) + "/children?" + url::build_query(qs), std::nullopt));
}

Json NotionApiClient::retrieve_page(const std::string& page_id) {
    return request("GET", "/pages/" + segment(page_id), std::nullopt);
}

void NotionApiClient::throttle() {
    const std::int64_t wait = last_at_ + min_interval_ms_ - time::steady_ms();
    if (wait > 0) config_.sleep(std::chrono::milliseconds(wait));
    last_at_ = time::steady_ms();
}

Json NotionApiClient::request(const std::string& method, const std::string& path, const std::optional<Json>& body) {
    int attempt = 0;
    while (true) {
        throttle();
        http::ClientRequest req;
        req.method = method;
        req.url = base_url_ + path;
        req.headers.set("authorization", "Bearer " + config_.token);
        req.headers.set("notion-version", version_);
        req.headers.set("content-type", "application/json");
        if (body) req.body = json_stringify(*body);
        req.timeout = config_.timeout;
        req.max_response_bytes = kMaxResponseBytes;
        req.allow_private_network = config_.allow_private_network;
        req.max_redirects = 0;  // API は転送しない。Authorization を別の場所へ持っていかない

        const http::ClientResponse res = config_.fetch(req);
        if (!res.transport_ok) throw std::runtime_error("notion " + method + " " + path + " failed: " + res.error);
        if (res.ok()) {
            JsonLimits limits;
            limits.max_bytes = kMaxResponseBytes;
            limits.max_elements = 2'000'000;
            std::string error;
            auto json = parse_json(res.body, limits, &error);
            if (!json) throw std::runtime_error("notion " + method + " " + path + " returned invalid JSON: " + error);
            return *json;
        }
        // 429 / 5xx はリトライ
        if ((res.status == 429 || res.status >= 500) && attempt < max_retries_) {
            const auto retry_after = str::js_number(res.headers.get("retry-after").value_or(""));
            const std::int64_t backoff =
                retry_after && std::isfinite(*retry_after) && *retry_after > 0
                    ? static_cast<std::int64_t>(*retry_after * 1000)
                    : std::min<std::int64_t>(1000LL << std::min(attempt, 20), 10'000);
            ++attempt;
            config_.sleep(std::chrono::milliseconds(backoff));
            continue;
        }
        throw std::runtime_error("notion " + method + " " + path + " failed: " + std::to_string(res.status) + " " +
                                 str::js_slice(res.body, 0, 200));
    }
}

std::unique_ptr<NotionApiClient> create_notion_client_from_env() {
    auto token = env::get("NOTION_TOKEN");
    if (!token || token->empty()) return nullptr;
    NotionClientConfig cfg;
    cfg.token = *token;
    if (auto v = env::get_nonempty("NOTION_VERSION")) cfg.notion_version = *v;
    if (auto ms = env::get_nonempty("NOTION_MIN_INTERVAL_MS")) {
        if (auto n = str::js_number(*ms); n && std::isfinite(*n)) cfg.min_interval_ms = static_cast<std::int64_t>(*n);
    }
    return std::make_unique<NotionApiClient>(std::move(cfg));
}

} // namespace canalis::crawl::notion
