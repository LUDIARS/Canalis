#pragma once
// ステージ ②Clean → ③Save の受け渡し契約 = sink ネイティブの「最終形」エンベロープ (src/core/envelope.ts の移植)。
// 整形の知識はすべて ② (サービス固有) が持ち、② が保存先のネイティブ形まで作り切る。③ は verbatim で書くだけ。
// ② が組む JSON をそのまま受け渡すので、C++ でも JSON の object として持つ (形は TS の型のとおり):
//   RdbBatch: { kind: "rdb", upserts: [{ table, rows: [{...}], conflictKey: [..] }] }
//   KgBatch : { kind: "kg", nodes: [{ label, key, props? }], edges: [{ label, from: {label, key}, to: {label, key}, props? }] }
//   FtBatch : { kind: "ft", task: "classification" | "causal-lm", examples: [..], dataset?, model? }
#include <string>
#include <string_view>

#include "canalis/base/json.hpp"

namespace canalis::core {

using SinkBatch = Json;

inline constexpr std::string_view kKindRaw = "raw";
inline constexpr std::string_view kKindRdb = "rdb";
inline constexpr std::string_view kKindKg = "kg";
inline constexpr std::string_view kKindFt = "ft";

/** バッチの kind (無ければ空文字)。 */
inline std::string batch_kind(const SinkBatch& batch) { return json_string(batch, "kind"); }

} // namespace canalis::core
