#include "canalis/runner/runner.hpp"

#include <stdexcept>

namespace canalis::runner {
namespace {

core::Sink& require_sink(const PipelineDeps& deps, const core::SinkSpec& spec) {
    const auto it = deps.sinks.find(spec.adapter);
    if (it == deps.sinks.end() || !it->second) throw std::runtime_error("unknown sink adapter: " + spec.adapter);
    core::Sink& sink = *it->second;
    if (sink.accepts() != spec.accepts) {
        throw std::runtime_error("sink '" + spec.adapter + "' accepts '" + std::string(sink.accepts()) +
                                 "' but manifest declares '" + spec.accepts + "'");
    }
    return sink;
}

std::vector<core::RawRecord> crawl_all(const core::Manifest& manifest, const PipelineDeps& deps, std::vector<StageError>& errors) {
    std::vector<core::RawRecord> records;
    for (const auto& spec : manifest.sources) {
        const auto it = deps.sources.find(spec.adapter);
        if (it == deps.sources.end() || !it->second) throw std::runtime_error("unknown source adapter: " + spec.adapter);
        try {
            auto got = it->second->crawl(spec.config);
            records.insert(records.end(), std::make_move_iterator(got.begin()), std::make_move_iterator(got.end()));
        } catch (const std::exception& e) {
            errors.push_back({"crawl:" + spec.adapter, e.what()});
        }
    }
    return records;
}

std::vector<Written> save_all(const core::Manifest& manifest, const PipelineDeps& deps, const std::vector<core::SinkBatch>& batches,
                              std::vector<StageError>& errors) {
    std::vector<Written> written;
    for (const auto& spec : manifest.save) {
        core::Sink& sink = require_sink(deps, spec);
        int count = 0;
        for (const auto& batch : batches) {
            if (core::batch_kind(batch) != spec.accepts) continue;
            try {
                sink.write_batch(batch);
                ++count;
            } catch (const std::exception& e) {
                errors.push_back({"save:" + spec.adapter, e.what()});
            }
        }
        written.push_back({std::string(sink.name()), spec.accepts, count});
    }
    return written;
}

} // namespace

RunReport run_pipeline(const core::Manifest& manifest, const PipelineDeps& deps) {
    RunReport report;
    report.pipeline = manifest.pipeline;
    const bool replay = manifest.replay_from.has_value() && static_cast<bool>(deps.replay_loader);
    report.mode = replay ? "replay" : "crawl";

    // ① 取得 (または replay の読み込み)
    const std::vector<core::RawRecord> records =
        replay ? deps.replay_loader(*manifest.replay_from) : crawl_all(manifest, deps, report.errors);
    report.record_count = records.size();

    // raw 保存 (crawl のときだけ。replay 元を上書きしない)
    if (!replay && manifest.raw_save) {
        core::Sink& raw_sink = require_sink(deps, *manifest.raw_save);
        try {
            raw_sink.write_raw(records);
            report.raw_saved = true;
        } catch (const std::exception& e) {
            report.errors.push_back({"rawSave:" + std::string(raw_sink.name()), e.what()});
        }
    }

    // ② 整形
    std::vector<core::SinkBatch> batches;
    if (manifest.transform) {
        const auto it = deps.transforms.find(*manifest.transform);
        if (it == deps.transforms.end() || !it->second) throw std::runtime_error("unknown transform: " + *manifest.transform);
        try {
            batches = it->second->run(records);
        } catch (const std::exception& e) {
            report.errors.push_back({"transform:" + *manifest.transform, e.what()});
        }
    }

    // ③ 保存
    report.written = save_all(manifest, deps, batches, report.errors);
    return report;
}

Json to_json(const RunReport& r) {
    Json written = Json::array();
    for (const auto& w : r.written) {
        Json o = Json::object();
        o["sink"] = w.sink;
        o["kind"] = w.kind;
        o["batches"] = w.batches;
        written.push_back(std::move(o));
    }
    Json errors = Json::array();
    for (const auto& e : r.errors) {
        Json o = Json::object();
        o["stage"] = e.stage;
        o["detail"] = e.detail;
        errors.push_back(std::move(o));
    }
    Json out = Json::object();
    out["pipeline"] = r.pipeline;
    out["mode"] = r.mode;
    out["recordCount"] = r.record_count;
    out["rawSaved"] = r.raw_saved;
    out["written"] = std::move(written);
    out["errors"] = std::move(errors);
    return out;
}

} // namespace canalis::runner
