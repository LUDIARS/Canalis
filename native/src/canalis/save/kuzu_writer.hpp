#pragma once
// ③Save の KG sink: KgBatch を Kuzu (グラフ DB) へ MERGE する (src/save/kuzu-writer.ts の移植)。
// ドライバは注入する。ノード / エッジのテーブル定義は事前に作成済みであること (サービスの責務)。
// TS との違い: プロパティ名も label と同じ規則 ([A-Za-z_][A-Za-z0-9_]*) で検査する (クエリに直接入るため)。
#include <functional>
#include <optional>
#include <string>

#include "canalis/core/pipeline.hpp"

namespace canalis::save {

/** Cypher の実行関数 (パラメタは名前つきの object)。利用者がドライバで実装して注入する。失敗は例外。 */
using CypherExecutor = std::function<void(const std::string& query, const Json& params)>;

struct KuzuSinkConfig {
    /** ノードの一意キーを入れるプロパティ名 (MERGE キー)。既定 "key"。 */
    std::optional<std::string> key_prop;
};

class KuzuSink final : public core::Sink {
public:
    KuzuSink(CypherExecutor exec, KuzuSinkConfig config = {}, std::string name = "kuzu");
    std::string_view name() const override { return name_; }
    std::string_view accepts() const override { return core::kKindKg; }
    void write_batch(const core::SinkBatch& batch) override;

private:
    void merge_node(const Json& node);
    void merge_edge(const Json& edge);
    CypherExecutor exec_;
    std::string key_prop_;
    std::string name_;
};

} // namespace canalis::save
