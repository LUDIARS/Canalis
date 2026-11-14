#pragma once
// Steam の adapter が返す取り出し結果の型 (src/crawl/steam/types.ts の移植)。
// appreviews API の応答 (レビュー・集計) は TS と同じく JSON のまま持つ。
#include <cstdint>
#include <optional>
#include <string>

#include "canalis/base/json.hpp"

namespace canalis::crawl::steam {

/** store 検索 (新作一覧) から取り出した 1 アプリ。 */
struct SteamSearchEntry {
    std::int64_t app_id = 0;
    std::string title;
    std::string url;
    /** 検索結果に出るリリース日の生の文字列 (ロケール依存)。 */
    std::optional<std::string> release_text;
};

/** steamcommunity の recommended ページから取り出した 1 レビュー。 */
struct SteamUserReviewEntry {
    std::string steam_id;
    std::int64_t app_id = 0;
    /** おすすめ (true) / おすすめしない (false)。判定できなければ nullopt。 */
    std::optional<bool> recommended;
    std::string text;
    std::optional<std::string> hours_text;
    std::optional<std::string> posted_text;
    std::string url;
};

/** TS のオブジェクトと同じキー順 (値の無い項目は出さない)。 */
Json to_json(const SteamSearchEntry& entry);
Json to_json(const SteamUserReviewEntry& entry);

} // namespace canalis::crawl::steam
