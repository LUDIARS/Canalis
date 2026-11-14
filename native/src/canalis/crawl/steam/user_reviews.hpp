#pragma once
// steamcommunity のプロフィールの recommended ページ (公開プロフィールのみ) から、1 人のレビューを全部取る
// (src/crawl/steam/user-reviews.ts の移植)。公式 API が無いので HTML をマーカーで切り出してタグを除く。
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "canalis/crawl/steam/http.hpp"
#include "canalis/crawl/steam/types.hpp"

namespace canalis::crawl::steam {

struct FetchUserReviewsOptions {
    SteamHttpOptions http;
    /** SteamID64 (17 桁)。違えば std::invalid_argument (値はエラー文に出さない)。 */
    std::string steam_id;
    /** ページの上限 (1 ページ約 10 件)。既定 50。負は std::invalid_argument。 */
    std::optional<int> max_pages;
    /** 表示言語。既定 "english" (Recommended / Not Recommended の判定語を安定させるため)。 */
    std::optional<std::string> language;
};

/** recommended ページの HTML からレビューを取り出す (TS と同じ規則。正規表現は使わない)。 */
std::vector<SteamUserReviewEntry> parse_user_reviews_html(std::string_view html, const std::string& steam_id);

/** 非公開・存在しないプロフィールなら空。 */
std::vector<SteamUserReviewEntry> fetch_user_reviews(const FetchUserReviewsOptions& options);

} // namespace canalis::crawl::steam
