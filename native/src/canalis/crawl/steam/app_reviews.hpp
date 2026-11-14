#pragma once
// appreviews API (store.steampowered.com/appreviews/{appid}?json=1) の取得 (src/crawl/steam/app-reviews.ts の移植)。
// cursor でページを送ってレビューを集める。集計だけなら num_per_page=0 で query_summary を引く。API キー不要。
// レビューと集計は TS と同じく API の JSON のまま返す。
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "canalis/crawl/steam/http.hpp"

namespace canalis::crawl::steam {

struct FetchAppReviewsOptions {
    SteamHttpOptions http;
    std::int64_t app_id = 0;
    /** Steam の language フィルタ。空なら {"all"}。 */
    std::vector<std::string> languages;
    /** "recent" | "all" | "updated" (既定 "recent")。それ以外は std::invalid_argument。 */
    std::optional<std::string> filter;
    /** 取得の上限。0 / 無指定で無制限。負は std::invalid_argument。 */
    std::optional<std::int64_t> max_reviews;
    /** 既知の recommendationid に当たったら打ち切る (増分取得)。 */
    std::optional<std::string> stop_at_recommendation_id;
    /** 1 ページごとの進捗 (累計件数)。 */
    std::function<void(std::size_t)> on_page;
};

/** レビューの集計 (query_summary) を 1 回の要求で取る。無ければ空 object。 */
Json fetch_app_review_summary(const SteamHttpOptions& http, std::int64_t app_id);

/** cursor でレビューを取る。success != 1 は std::runtime_error (黙って続けない)。 */
std::vector<Json> fetch_app_reviews(const FetchAppReviewsOptions& options);

/** 要求の URL (テスト用に公開)。cursor の `*` は %2A にする (TS と同じ)。 */
std::string build_reviews_url(std::int64_t app_id, const std::string& filter, const std::vector<std::string>& languages,
                              const std::string& cursor, int per_page);

} // namespace canalis::crawl::steam
