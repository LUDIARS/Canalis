#include "cli/adapters.hpp"

#include <cmath>

#include "canalis/crawl/discord/source.hpp"
#include "canalis/crawl/notion/crawl.hpp"
#include "canalis/crawl/notion_public/source.hpp"
#include "canalis/crawl/reddit/source.hpp"
#include "canalis/crawl/steam/source.hpp"
#include "canalis/crawl/website/source.hpp"
#include "canalis/crawl/youtube/source.hpp"

namespace canalis::cli {
namespace {

std::optional<std::int64_t> opt_int(const Json& obj, std::string_view key) {
    const auto n = json_opt_number(obj, key);
    if (!n || !std::isfinite(*n) || std::fabs(*n) > 9.0e15) return std::nullopt;
    return static_cast<std::int64_t>(*n);
}

std::optional<int> opt_small(const Json& obj, std::string_view key) {
    const auto n = opt_int(obj, key);
    if (!n || *n < -1'000'000'000 || *n > 1'000'000'000) return std::nullopt;
    return static_cast<int>(*n);
}

std::vector<std::string> string_list(const Json& obj, std::string_view key) {
    std::vector<std::string> out;
    for (const auto& v : json_array(obj, key))
        if (v.is_string()) out.push_back(v.get<std::string>());
    return out;
}

std::vector<core::RawRecord> website(const Json& c) {
    crawl::website::WebsiteSourceConfig config;
    config.url = json_string(c, "url");
    config.user_agent = json_opt_string(c, "userAgent");
    config.timeout_ms = opt_int(c, "timeoutMs");
    return crawl::website::WebsiteSource().crawl(config);
}

std::vector<core::RawRecord> notion_public(const Json& c) {
    const Json& o = json_object(c, "options");
    crawl::notion_public::NotionPublicSourceConfig config;
    config.url = json_string(c, "url");
    config.options.timeout = opt_int(o, "timeout");
    config.options.scroll_delay = opt_int(o, "scrollDelay");
    config.options.max_scrolls = opt_small(o, "maxScrolls");
    return crawl::notion_public::NotionPublicSource().crawl(config);
}

std::vector<core::RawRecord> notion(const Json& c) {
    const Json& o = json_object(c, "crawl");
    crawl::notion::NotionSourceConfig config;
    config.database_id = json_string(c, "databaseId");
    config.token = json_opt_string(c, "token");
    config.crawl.max_depth = opt_small(o, "maxDepth");
    config.crawl.max_pages = opt_small(o, "maxPages");
    config.crawl.include_child_databases = json_opt_bool(o, "includeChildDatabases");
    return crawl::notion::NotionSource().crawl(config);
}

std::vector<core::RawRecord> youtube(const Json& c) {
    return crawl::youtube::YouTubeSource().crawl(
        {json_string(c, "videoId"), json_string(c, "apiKey"), opt_small(c, "maxResults"), json_opt_string(c, "textFormat")});
}

std::vector<core::RawRecord> reddit(const Json& c) {
    crawl::reddit::RedditSourceConfig r;
    r.subreddit = json_opt_string(c, "subreddit");
    r.post_id = json_opt_string(c, "postId");
    r.subreddit_for_post = json_opt_string(c, "subredditForPost");
    r.limit = opt_small(c, "limit");
    r.sort = json_opt_string(c, "sort");
    r.mode = json_opt_string(c, "mode");
    return crawl::reddit::RedditSource().crawl(r);
}

crawl::discord::DiscordCrawlOptions discord_options(const Json& o) {
    crawl::discord::DiscordCrawlOptions out;
    out.max_messages = opt_small(o, "maxMessages");
    out.before = json_opt_string(o, "before");
    out.after = json_opt_string(o, "after");
    out.oldest_first = json_opt_bool(o, "oldestFirst");
    return out;
}

std::vector<core::RawRecord> discord(const Json& c) {
    crawl::discord::DiscordSourceConfig d;
    d.channel_id = json_string(c, "channelId");
    d.token = json_opt_string(c, "token");
    d.guild_id = json_opt_string(c, "guildId");
    d.options = discord_options(json_object(c, "options"));
    return crawl::discord::DiscordSource().crawl(d);
}

std::vector<core::RawRecord> discord_guild(const Json& c) {
    crawl::discord::DiscordGuildSourceConfig d;
    d.guild_id = json_string(c, "guildId");
    d.token = json_opt_string(c, "token");
    if (c.contains("channelTypes")) {
        std::vector<int> types;
        for (const auto& v : json_array(c, "channelTypes"))
            if (v.is_number_integer()) types.push_back(v.get<int>());
        d.channel_types = types;
    }
    d.exclude_channel_ids = string_list(c, "excludeChannelIds");
    d.options = discord_options(json_object(c, "options"));
    return crawl::discord::DiscordGuildSource().crawl(d);
}

crawl::steam::SteamHttpOptions steam_http(const Json& c) {
    crawl::steam::SteamHttpOptions out;
    out.polite_delay_ms = opt_int(c, "politeDelayMs");
    return out;
}

std::vector<core::RawRecord> steam_new_releases(const Json& c) {
    crawl::steam::FetchNewReleasesOptions s;
    s.http = steam_http(c);
    s.max_apps = opt_small(c, "maxApps");
    s.country_code = json_opt_string(c, "countryCode");
    s.language = json_opt_string(c, "language");
    return crawl::steam::SteamNewReleasesSource().crawl(s);
}

std::vector<core::RawRecord> steam_app_reviews(const Json& c) {
    crawl::steam::FetchAppReviewsOptions s;
    s.http = steam_http(c);
    s.app_id = opt_int(c, "appId").value_or(0);
    s.languages = string_list(c, "languages");
    s.filter = json_opt_string(c, "filter");
    s.max_reviews = opt_int(c, "maxReviews");
    s.stop_at_recommendation_id = json_opt_string(c, "stopAtRecommendationId");
    return crawl::steam::SteamAppReviewsSource().crawl(s);
}

std::vector<core::RawRecord> steam_user_reviews(const Json& c) {
    crawl::steam::FetchUserReviewsOptions s;
    s.http = steam_http(c);
    s.steam_id = json_string(c, "steamId");
    s.max_pages = opt_small(c, "maxPages");
    s.language = json_opt_string(c, "language");
    return crawl::steam::SteamUserReviewsSource().crawl(s);
}

using Crawl = std::vector<core::RawRecord> (*)(const Json&);
struct Entry {
    std::string_view name;
    Crawl crawl;
};
constexpr Entry kAdapters[] = {
    {"website", website},
    {"notion-public", notion_public},
    {"notion", notion},
    {"youtube", youtube},
    {"reddit", reddit},
    {"discord", discord},
    {"discord-guild", discord_guild},
    {"steam-new-releases", steam_new_releases},
    {"steam-app-reviews", steam_app_reviews},
    {"steam-user-reviews", steam_user_reviews},
};

} // namespace

const std::vector<std::string_view>& adapter_names() {
    static const std::vector<std::string_view> names = [] {
        std::vector<std::string_view> out;
        for (const auto& e : kAdapters) out.push_back(e.name);
        return out;
    }();
    return names;
}

std::optional<std::vector<core::RawRecord>> crawl_adapter(std::string_view adapter, const Json& config) {
    for (const auto& e : kAdapters)
        if (e.name == adapter) return e.crawl(config);
    return std::nullopt;
}

} // namespace canalis::cli
