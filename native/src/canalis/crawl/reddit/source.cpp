#include "canalis/crawl/reddit/source.hpp"

#include <stdexcept>

#include "canalis/base/time.hpp"
#include "canalis/base/url.hpp"

namespace canalis::crawl::reddit {
namespace {

constexpr std::string_view kUserAgent = "Canalis-Crawler/1.0 (LUDIARS; +https://github.com/LUDIARS)";
constexpr std::string_view kBase = "https://www.reddit.com";
// 返信の入れ子の安全弁 (JSON の深さの上限とは別に、再帰を止める)。
constexpr int kMaxReplyDepth = 100;

void flatten_comments(const Json& items, const std::string& fetched_at, std::vector<core::RawRecord>& out, int depth) {
    if (depth > kMaxReplyDepth || !items.is_array()) return;
    for (const auto& item : items) {
        if (!item.is_object() || json_string(item, "kind") != "t1") continue;
        const Json& c = json_object(item, "data");
        core::RawRecord r;
        r.source = "reddit";
        r.source_id = json_string(c, "name");
        r.fetched_at = fetched_at;
        r.url = std::string(kBase) + json_string(c, "permalink");
        if (auto body = json_opt_string(c, "body")) r.text = *body;
        r.raw = c;
        Json meta = Json::object();
        for (const char* key : {"subreddit", "author", "score", "created_utc", "parent_id", "link_id"})
            if (auto v = c.find(key); v != c.end()) meta[key] = *v;
        r.meta = std::move(meta);
        out.push_back(std::move(r));
        // replies は返信が無いと "" (文字列) になる。object のときだけ辿る。
        const Json& replies = json_object(c, "replies");
        flatten_comments(json_array(json_object(replies, "data"), "children"), fetched_at, out, depth + 1);
    }
}

} // namespace

RedditSource::RedditSource(RedditSourceDeps deps) : deps_(std::move(deps)) {
    if (!deps_.fetch) deps_.fetch = default_fetch;
    if (!deps_.now) deps_.now = [] { return time::now_iso(); };
}

Json RedditSource::get(const std::string& url) const {
    http::ClientRequest req;
    req.url = url;
    req.headers.set("User-Agent", std::string(kUserAgent));
    const auto res = deps_.fetch(req);
    if (!res.transport_ok) throw std::runtime_error("RedditSource: request failed " + url + ": " + res.error);
    if (!res.ok()) throw std::runtime_error("RedditSource: API error " + std::to_string(res.status) + " " + url + ": " + res.body);
    return parse_response_json(res, "RedditSource");
}

std::vector<core::RawRecord> RedditSource::crawl(const RedditSourceConfig& config) {
    const std::string mode = config.mode.value_or("posts");
    const std::string fetched_at = deps_.now();
    std::vector<core::RawRecord> out;

    if (mode == "comments") {
        if (!config.post_id || config.post_id->empty())
            throw std::runtime_error("RedditSource: postId is required for mode=comments");
        const std::string sub = config.subreddit_for_post.value_or("unknown");
        const std::string url = std::string(kBase) + "/r/" + url::encode_component(sub) + "/comments/" +
                                url::encode_component(*config.post_id) +
                                ".json?limit=" + std::to_string(config.limit.value_or(500)) + "&raw_json=1";
        const Json res = get(url);
        // Reddit は [投稿の一覧, コメントの一覧] を返す。
        const Json& comments = res.is_array() && res.size() > 1 ? json_array(json_object(res[1], "data"), "children") : Json::array();
        flatten_comments(comments, fetched_at, out, 0);
        return out;
    }

    if (!config.subreddit || config.subreddit->empty())
        throw std::runtime_error("RedditSource: subreddit is required for mode=posts");
    const std::string url = std::string(kBase) + "/r/" + url::encode_component(*config.subreddit) + "/" +
                            url::encode_component(config.sort.value_or("hot")) +
                            ".json?limit=" + std::to_string(config.limit.value_or(100)) + "&raw_json=1";
    const Json res = get(url);
    for (const auto& child : json_array(json_object(res, "data"), "children")) {
        const Json& p = json_object(child, "data");
        core::RawRecord r;
        r.source = "reddit";
        r.source_id = json_string(p, "name");
        r.fetched_at = fetched_at;
        r.url = std::string(kBase) + json_string(p, "permalink");
        if (auto title = json_opt_string(p, "title")) r.title = *title;
        const std::string selftext = json_string(p, "selftext");
        if (!selftext.empty()) r.text = selftext;
        else if (r.title) r.text = *r.title;
        r.raw = p;
        Json meta = Json::object();
        for (const char* key : {"subreddit", "author", "score", "upvote_ratio", "num_comments", "created_utc"})
            if (auto v = p.find(key); v != p.end()) meta[key] = *v;
        r.meta = std::move(meta);
        out.push_back(std::move(r));
    }
    return out;
}

} // namespace canalis::crawl::reddit
