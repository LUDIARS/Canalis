#pragma once
// ③Save の RDB sink: RdbBatch を Postgres へ upsert する (src/save/postgres-writer.ts の移植)。
// DB ドライバは注入する (共有 lib はドライバに直接依存しない)。エンベロープが「最終形」なので、
// table / rows / conflictKey をそのまま SQL (INSERT ... ON CONFLICT) にして書くだけ。値は必ずパラメタで渡す。
#include <functional>
#include <string>
#include <vector>

#include "canalis/core/pipeline.hpp"

namespace canalis::save {

/** パラメタ化クエリの実行関数 (`$1`, `$2` …)。利用者がドライバで実装して注入する。失敗は例外。 */
using SqlExecutor = std::function<void(const std::string& query, const std::vector<Json>& params)>;

struct SqlStatement {
    std::string query;
    std::vector<Json> params;
};

/** 1 つの upsert ({table, rows, conflictKey}) を行ごとの文にする (テスト用に公開)。識別子が危なければ std::invalid_argument。 */
std::vector<SqlStatement> build_upsert_statements(const Json& upsert);

class PostgresSink final : public core::Sink {
public:
    explicit PostgresSink(SqlExecutor exec, std::string name = "postgres");
    std::string_view name() const override { return name_; }
    std::string_view accepts() const override { return core::kKindRdb; }
    void write_batch(const core::SinkBatch& batch) override;

private:
    SqlExecutor exec_;
    std::string name_;
};

} // namespace canalis::save
