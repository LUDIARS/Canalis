#include "canalis/crawl/discord/client.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "canalis/base/env.hpp"
#include "canalis/base/strings.hpp"
#include "canalis/base/url.hpp"

namespace canalis::crawl::discord {
namespace {

constexpr std::string_view kBaseUrl = "https://discord.com/api/v10";
constexpr int kMaxRateLimitRetries = 3;

std::vector<Json> to_vector(const Json& array) {
    std::vector<Json> out;
    if (array.is_array())
        for (const auto& v : array) out.push_back(v);
    return out;
}

} // namespace

DiscordApiClient::DiscordApiClient(std::string token, FetchFn fetch, SleepFn sleep)
    : token_(std::move(token)), fetch_(std::move(fetch)), sleep_(std::move(sleep)) {
    if (token_.empty()) throw std::invalid_argument("DiscordApiClient: token is required");
    if (!fetch_) fetch_ = default_fetch;
    if (!sleep_) sleep_ = default_sleep;
}

Json DiscordApiClient::request(const std::string& url) {
    for (int attempt = 0;; ++attempt) {
        http::ClientRequest req;
        req.url = url;
        req.headers.set("Authorization", "Bot " + token_);
        req.max_redirects = 0;  // token を別の場所へ持っていかない
        const auto res = fetch_(req);
        if (!res.transport_ok) throw std::runtime_error("DiscordApi: request failed — " + url + ": " + res.error);
        if (res.status == 429) {
            // Rate limited — Retry-After は秒 (小数あり)
            const auto retry_after = str::js_number(res.headers.get("Retry-After").value_or("1"));
            if (attempt >= kMaxRateLimitRetries) throw std::runtime_error("DiscordApi: rate limit retries exhausted (" + url + ")");
            const double seconds = retry_after && std::isfinite(*retry_after) && *retry_after >= 0 ? std::min(*retry_after, 600.0) : 1.0;
            sleep_(std::chrono::milliseconds(static_cast<std::int64_t>(std::ceil(seconds * 1000)) + 100));
            continue;
        }
        if (!res.ok())
            throw std::runtime_error("DiscordApi: " + std::to_string(res.status) + " " + res.reason + " \xE2\x80\x94 " + url + "\n" + res.body);
        return parse_response_json(res, "DiscordApi");
    }
}

std::vector<Json> DiscordApiClient::get_guild_channels(const std::string& guild_id) {
    return to_vector(request(std::string(kBaseUrl) + "/guilds/" + url::encode_component(guild_id) + "/channels"));
}

std::vector<Json> DiscordApiClient::get_active_threads(const std::string& guild_id) {
    return to_vector(json_array(request(std::string(kBaseUrl) + "/guilds/" + url::encode_component(guild_id) + "/threads/active"), "threads"));
}

ArchivedThreads DiscordApiClient::get_archived_threads(const std::string& channel_id, const std::optional<std::string>& before) {
    std::vector<std::pair<std::string, std::string>> q{{"limit", "100"}};
    if (before && !before->empty()) q.emplace_back("before", *before);
    const Json res = request(std::string(kBaseUrl) + "/channels/" + url::encode_component(channel_id) +
                             "/threads/archived/public?" + url::build_query(q));
    return {to_vector(json_array(res, "threads")), json_bool(res, "has_more", false)};
}

std::vector<Json> DiscordApiClient::get_messages(const std::string& channel_id, const MessagesQuery& query) {
    std::vector<std::pair<std::string, std::string>> q{{"limit", std::to_string(query.limit)}};
    if (query.before && !query.before->empty()) q.emplace_back("before", *query.before);
    if (query.after && !query.after->empty()) q.emplace_back("after", *query.after);
    return to_vector(request(std::string(kBaseUrl) + "/channels/" + url::encode_component(channel_id) + "/messages?" + url::build_query(q)));
}

std::unique_ptr<DiscordApi> create_discord_client(const std::optional<std::string>& token) {
    std::optional<std::string> t = token;
    if (!t) t = env::get("DISCORD_BOT_TOKEN");
    if (!t || t->empty())
        throw std::runtime_error("DiscordApiClient: token is required (config.token or env DISCORD_BOT_TOKEN)");
    return std::make_unique<DiscordApiClient>(*t);
}

} // namespace canalis::crawl::discord
