#pragma once
// パイプラインを宣言する「フォーマット」(src/core/{manifest,manifest-loader}.ts の移植)。
// D3 (spec/plan/cpp-port.md): C++ 版の manifest は JSON だけ。.yaml / .yml は変換を案内して失敗にする。
#include <optional>
#include <string>
#include <vector>

#include "canalis/base/json.hpp"

namespace canalis::core {

/** 取得元 1 つ: adapter 名 + adapter 固有の config。 */
struct SourceSpec {
    std::string adapter;
    Json config;
};

/** 保存先 1 つ: adapter 名 + 受け付ける種別 ("raw" | "rdb" | "kg" | "ft") + adapter 固有の config。 */
struct SinkSpec {
    std::string adapter;
    std::string accepts;
    Json config;
};

struct Manifest {
    std::string pipeline;
    std::vector<SourceSpec> sources;
    /** 任意: ① の出力をそのまま保存する raw sink。 */
    std::optional<SinkSpec> raw_save;
    /** ② の adapter 名。無ければ取得 (+ raw 保存) だけ。 */
    std::optional<std::string> transform;
    std::vector<SinkSpec> save;
    /** 任意: 再処理。crawl の代わりに raw store から読み直して ②→③ を焼き直す。 */
    std::optional<SinkSpec> replay_from;
};

/** JSON の値から読む。pipeline (文字列) と crawl (object) が無い・形が違うときは std::runtime_error (label を付ける)。 */
Manifest manifest_from_json(const Json& value, const std::string& label);

/** ファイルから読む。.yaml / .yml・読めない・壊れた JSON は std::runtime_error。 */
Manifest load_manifest(const std::string& path);

} // namespace canalis::core
