#include "canalis/crawl/steam/app_reviews.hpp"

#include <stdexcept>
#include <unordered_set>

#include "canalis/base/strings.hpp"
#include "canalis/base/url.hpp"

namespace canalis::crawl::steam {
namespace {

// cursor のページ送りの安全弁 (1 ページ 100 件なので 1,000 万件まで)。
constexpr int kMaxReviewPages = 100'000;

void assert_app_id(std::int64_t app_id) {
    if (app_id <= 0) throw std::invalid_argument("steam appId must be a positive integer");
}

std::string filter_of(const std::optional<std::string>& filter) {
    const std::string f = filter.value_or("recent");
    if (f != "recent" && f != "all" && f != "updated") throw std::invalid_argument("steam filter must be recent, all, or updated");
    return f;
}

/** `success !== 1` なら失敗 (値は JS の文字列化と同じく、無ければ undefined)。 */
void assert_success(const Json& data, std::int64_t app_id) {
    const auto s = data.find("success");
    if (s != data.end() && s->is_number() && s->get<double>() == 1) return;
    const std::string shown = s == data.end() ? "undefined" : json_stringify(*s);
    throw std::runtime_error("steam appreviews success=" + shown + " appId=" + std::to_string(app_id));
}

} // namespace

std::string build_reviews_url(std::int64_t app_id, const std::string& filter, const std::vector<std::string>& languages,
                              const std::string& cursor, int per_page) {
    const std::vector<std::string> langs = languages.empty() ? std::vector<std::string>{"all"} : languages;
    return "https://store.steampowered.com/appreviews/" + std::to_string(app_id) + "?json=1&num_per_page=" +
           std::to_string(per_page) + "&filter=" + filter + "&language=" + url::encode_component(str::join(langs, ",")) +
           "&review_type=all&purchase_type=all&cursor=" + str::replace_all(url::encode_component(cursor), "*", "%2A");
}

Json fetch_app_review_summary(const SteamHttpOptions& http_options, std::int64_t app_id) {
    assert_app_id(app_id);
    const SteamHttp http(http_options);
    const Json data = http.get_json(build_reviews_url(app_id, "recent", {}, "*", 0));
    assert_success(data, app_id);
    return json_object(data, "query_summary");
}

std::vector<Json> fetch_app_reviews(const FetchAppReviewsOptions& options) {
    assert_app_id(options.app_id);
    if (options.max_reviews && *options.max_reviews < 0) throw std::invalid_argument("steam maxReviews must be a non-negative integer");
    const std::string filter = filter_of(options.filter);
    const SteamHttp http(options.http);
    const std::int64_t max_reviews = options.max_reviews.value_or(0);

    std::vector<Json> out;
    std::unordered_set<std::string> seen_cursors;
    std::string cursor = "*";
    for (int page = 0; page < kMaxReviewPages; ++page) {
        const Json data = http.get_json(build_reviews_url(options.app_id, filter, options.languages, cursor, 100));
        assert_success(data, options.app_id);
        const Json& reviews = json_array(data, "reviews");
        if (reviews.empty()) break;
        for (const auto& r : reviews) {
            if (options.stop_at_recommendation_id && !options.stop_at_recommendation_id->empty() &&
                json_string(r, "recommendationid") == *options.stop_at_recommendation_id)
                return out;
            out.push_back(r);
            if (max_reviews > 0 && static_cast<std::int64_t>(out.size()) >= max_reviews) return out;
        }
        if (options.on_page) options.on_page(out.size());
        const auto next = json_opt_string(data, "cursor");
        if (!next || next->empty() || !seen_cursors.insert(*next).second) break;  // 末尾 (cursor が進まない) で止める
        cursor = *next;
        http.polite_wait();
    }
    return out;
}

} // namespace canalis::crawl::steam
