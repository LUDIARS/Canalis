#pragma once
// ステージ ①Crawl → ②Clean、および ①Crawl → raw 保存の受け渡し契約 (src/core/raw-record.ts の移植)。
// JSON に出すときのキーと順序は TS の adapter が組むオブジェクトと同じ
// (source, sourceId, fetchedAt, url, title, text, raw, meta)。値の無い項目は出さない (JSON.stringify が undefined を落とすのと同じ)。
#include <optional>
#include <string>

#include "canalis/base/json.hpp"

namespace canalis::core {

struct RawRecord {
    /** 取得元の種別。例: "notion" | "notion-public" | "website" */
    std::string source;
    /** source 内で一意な ID (dedup キー)。 */
    std::string source_id;
    /** 取得時刻 (ISO8601 / UTC)。 */
    std::string fetched_at;
    std::optional<std::string> url;
    std::optional<std::string> title;
    /** 本文を正規化したテキスト (Markdown 等)。②の主な入力。 */
    std::optional<std::string> text;
    /** source ネイティブの原データ (replay / 監査用)。 */
    Json raw;
    /** 付帯メタ (source 固有の補助情報)。object のときだけ出す。 */
    std::optional<Json> meta;
};

Json to_json(const RawRecord& record);

/** JSON から読む。source / sourceId / fetchedAt が文字列でなければ std::nullopt。 */
std::optional<RawRecord> raw_record_from_json(const Json& value);

} // namespace canalis::core
