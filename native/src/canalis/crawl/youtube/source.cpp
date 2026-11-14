#include "canalis/crawl/youtube/source.hpp"

#include <algorithm>
#include <stdexcept>

#include "canalis/base/time.hpp"
#include "canalis/base/url.hpp"

namespace canalis::crawl::youtube {
namespace {

constexpr std::string_view kBase = "https://www.googleapis.com/youtube/v3";
// ページ送りの安全弁 (nextPageToken を返し続けても止まる)。
constexpr int kMaxRequests = 10'000;

} // namespace

YouTubeSource::YouTubeSource(YouTubeSourceDeps deps) : deps_(std::move(deps)) {
    if (!deps_.fetch) deps_.fetch = default_fetch;
    if (!deps_.now) deps_.now = [] { return time::now_iso(); };
}

std::vector<core::RawRecord> YouTubeSource::crawl(const YouTubeSourceConfig& config) {
    if (config.video_id.empty()) throw std::runtime_error("YouTubeSource: videoId is required");
    if (config.api_key.empty()) throw std::runtime_error("YouTubeSource: apiKey is required");
    const int max_results = config.max_results.value_or(100);
    const std::string text_format = config.text_format.value_or("plainText");

    std::vector<Json> threads;
    std::optional<std::string> page_token;
    for (int request = 0; request < kMaxRequests; ++request) {
        std::vector<std::pair<std::string, std::string>> params{
            {"part", "snippet"},
            {"videoId", config.video_id},
            {"key", config.api_key},
            {"maxResults", std::to_string(std::min(max_results - static_cast<int>(threads.size()), 100))},
            {"textFormat", text_format},
        };
        if (page_token) params.emplace_back("pageToken", *page_token);
        http::ClientRequest req;
        req.url = std::string(kBase) + "/commentThreads?" + url::build_query(params);
        const auto res = deps_.fetch(req);
        // URL には API キーが入るので出さない。
        if (!res.transport_ok) throw std::runtime_error("YouTubeSource: request failed: " + res.error);
        if (!res.ok()) throw std::runtime_error("YouTubeSource: API error " + std::to_string(res.status) + ": " + res.body);
        const Json data = parse_response_json(res, "YouTubeSource");
        const Json& items = json_array(data, "items");
        for (const auto& t : items) threads.push_back(t);
        page_token = json_opt_string(data, "nextPageToken");
        if (!page_token || page_token->empty() || static_cast<int>(threads.size()) >= max_results || items.empty()) break;
    }

    const std::string fetched_at = deps_.now();
    const std::string video_url = "https://www.youtube.com/watch?v=" + config.video_id;
    std::vector<core::RawRecord> out;
    for (const auto& t : threads) {
        const Json& thread_snippet = json_object(t, "snippet");
        const Json& snippet = json_object(json_object(thread_snippet, "topLevelComment"), "snippet");
        core::RawRecord r;
        r.source = "youtube";
        r.source_id = json_string(t, "id");
        r.fetched_at = fetched_at;
        r.url = video_url;
        if (auto text = json_opt_string(snippet, "textDisplay")) r.text = *text;
        r.raw = t;
        Json meta = Json::object();
        meta["videoId"] = config.video_id;
        if (auto v = snippet.find("authorDisplayName"); v != snippet.end()) meta["authorDisplayName"] = *v;
        if (auto v = json_opt_string(json_object(snippet, "authorChannelId"), "value")) meta["authorChannelId"] = *v;
        if (auto v = snippet.find("likeCount"); v != snippet.end()) meta["likeCount"] = *v;
        if (auto v = thread_snippet.find("totalReplyCount"); v != thread_snippet.end()) meta["replyCount"] = *v;
        if (auto v = snippet.find("publishedAt"); v != snippet.end()) meta["publishedAt"] = *v;
        if (auto v = snippet.find("updatedAt"); v != snippet.end()) meta["updatedAt"] = *v;
        r.meta = std::move(meta);
        out.push_back(std::move(r));
    }
    return out;
}

} // namespace canalis::crawl::youtube
