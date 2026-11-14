#pragma once
// ③Save の FT sink: FtBatch を「学習データセット (JSONL) + ジョブ manifest」に書き出し、任意で外部の FT runner を起動する
// (src/save/ft-writer.ts の移植)。Canalis core は学習を持たない (LLM / ML 依存ゼロ)。実際の学習は core の外の runner が行う。
// TS との違い: dataset 名を出力先の 1 区間として検査する ("/" や ".." で出力先の外へ出さない)。
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "canalis/core/pipeline.hpp"

namespace canalis::save {

/** runner へ渡すジョブの記述 (job.json の中身)。 */
struct FtJob {
    /** "classification" | "causal-lm" */
    std::string task;
    std::string dataset;
    std::size_t count = 0;
    std::string data_path;
    std::string job_path;
    Json model = Json::object();
    std::string created_at;
};

/** TS と同じキー (task, dataset, count, dataPath, jobPath, model, createdAt)。 */
Json to_json(const FtJob& job);

/** FT の runner (core の外の学習の実体を起動する)。失敗は例外。 */
using FtRunner = std::function<void(const FtJob& job)>;

struct FtSinkConfig {
    /** 出力のルート。<dir>/<dataset>/<stamp>/ に data.jsonl と job.json を書く。 */
    std::string dir;
    /** 無ければデータセットを書くだけ (起動しない)。 */
    FtRunner runner;
    /** stamp の時刻源 (既定は現在時刻の ISO8601)。 */
    std::function<std::string()> now;
};

class FtSink final : public core::Sink {
public:
    explicit FtSink(FtSinkConfig config, std::string name = "ft");
    std::string_view name() const override { return name_; }
    std::string_view accepts() const override { return core::kKindFt; }
    /** task に合わない例・危ない dataset 名は std::invalid_argument。 */
    void write_batch(const core::SinkBatch& batch) override;

private:
    FtSinkConfig config_;
    std::string name_;
};

/**
 * 外部コマンドを FT runner として起動する。`<command> [...args] --job <jobPath>` を引数の配列で起動する (シェルを通さない)。
 * 終了コードが 0 でなければ std::runtime_error (標準エラーの末尾を添える)。期限の既定は 24 時間。
 */
FtRunner command_runner(std::string command, std::vector<std::string> args = {}, std::string cwd = {},
                        std::chrono::milliseconds timeout = std::chrono::hours(24));

} // namespace canalis::save
