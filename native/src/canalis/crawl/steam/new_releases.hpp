#pragma once
// store 検索 (リリース日の降順) から新作アプリの一覧を取る (src/crawl/steam/new-releases.ts の移植)。
// `search/results/?infinite=1` は { results_html, total_count } の JSON を返すので、results_html から appid / タイトル / リリース日を取り出す。
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "canalis/crawl/steam/http.hpp"
#include "canalis/crawl/steam/types.hpp"

namespace canalis::crawl::steam {

struct FetchNewReleasesOptions {
    SteamHttpOptions http;
    /** 取得件数の上限。既定 100。負は std::invalid_argument。 */
    std::optional<int> max_apps;
    /** store の国コード。既定 "jp"。 */
    std::optional<std::string> country_code;
    /** 表示言語。既定 "japanese"。 */
    std::optional<std::string> language;
};

/** results_html から検索エントリを取り出す (TS と同じ規則。正規表現は使わない)。 */
std::vector<SteamSearchEntry> parse_search_results_html(std::string_view html);

std::vector<SteamSearchEntry> fetch_new_releases(const FetchNewReleasesOptions& options = {});

} // namespace canalis::crawl::steam
