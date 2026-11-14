#pragma once
// manifest を解釈して 3 ステージを実行するオーケストレータ (src/runner/runner.ts の移植)。
//   crawl (① sources) → [rawSave (raw sink)] → transform (②) → save (③ sinks)
// source / transform / sink の実体は呼び出し側が deps で注入する (DI)。サービス固有の ② は各リポに置き、ここでは名前で引くだけ。
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "canalis/core/manifest.hpp"
#include "canalis/core/pipeline.hpp"

namespace canalis::runner {

/** 注入する実体 (adapter 名 → インスタンス。所有は呼び出し側)。 */
struct PipelineDeps {
    std::map<std::string, core::JsonSource*> sources;
    std::map<std::string, core::Transform*> transforms;
    std::map<std::string, core::Sink*> sinks;
    /** manifest.replayFrom があるときに raw store から読む関数 (任意)。 */
    std::function<std::vector<core::RawRecord>(const core::SinkSpec&)> replay_loader;
};

struct StageError {
    std::string stage;
    std::string detail;
};

struct Written {
    std::string sink;
    std::string kind;
    int batches = 0;
};

struct RunReport {
    std::string pipeline;
    /** "crawl" | "replay" */
    std::string mode;
    std::size_t record_count = 0;
    bool raw_saved = false;
    std::vector<Written> written;
    std::vector<StageError> errors;
};

/**
 * manifest 1 本を実行する。各段の失敗 (source・raw 保存・transform・sink の書き込み) は errors に積んで続ける。
 * manifest の誤り (未知の adapter・accepts の食い違い) は std::runtime_error (TS と同じく続けない)。
 */
RunReport run_pipeline(const core::Manifest& manifest, const PipelineDeps& deps);

/** TS と同じキー (pipeline, mode, recordCount, rawSaved, written[{sink, kind, batches}], errors[{stage, detail}])。 */
Json to_json(const RunReport& report);

} // namespace canalis::runner
