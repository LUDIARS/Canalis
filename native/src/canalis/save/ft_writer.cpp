#include "canalis/save/ft_writer.hpp"

#include <stdexcept>

#include "canalis/base/fsutil.hpp"
#include "canalis/base/process.hpp"
#include "canalis/base/strings.hpp"
#include "canalis/base/time.hpp"

namespace canalis::save {
namespace {

/** task に応じて 1 例を JSONL の 1 行にする。形が合わなければ std::invalid_argument。 */
Json to_record(const std::string& task, const Json& ex) {
    if (!ex.is_object()) throw std::invalid_argument("FT example must be an object");
    if (task == "classification") {
        if (ex.contains("input") && ex.contains("label")) return Json{{"input", ex["input"]}, {"label", ex["label"]}};
        throw std::invalid_argument("classification example requires { input, label }");
    }
    // causal-lm: chat か prompt / completion のどちらか
    if (ex.contains("messages")) return Json{{"messages", ex["messages"]}};
    if (ex.contains("prompt") && ex.contains("completion")) return Json{{"prompt", ex["prompt"]}, {"completion", ex["completion"]}};
    throw std::invalid_argument("causal-lm example requires { messages } or { prompt, completion }");
}

} // namespace

Json to_json(const FtJob& job) {
    Json out = Json::object();
    out["task"] = job.task;
    out["dataset"] = job.dataset;
    out["count"] = job.count;
    out["dataPath"] = job.data_path;
    out["jobPath"] = job.job_path;
    out["model"] = job.model;
    out["createdAt"] = job.created_at;
    return out;
}

FtSink::FtSink(FtSinkConfig config, std::string name) : config_(std::move(config)), name_(std::move(name)) {
    if (!config_.now) config_.now = [] { return time::now_iso(); };
}

void FtSink::write_batch(const core::SinkBatch& batch) {
    const std::string created_at = config_.now();
    const std::string task = json_string(batch, "task");
    const std::string dataset = json_opt_string(batch, "dataset").value_or("default");
    const std::string stamp = str::replace_all(created_at, ":", "-");
    const auto dataset_dir = fs::safe_join(config_.dir, dataset);
    if (!dataset_dir || dataset.find('/') != std::string::npos || dataset.find('\\') != std::string::npos)
        throw std::invalid_argument("FT dataset name must be a single path segment");
    const std::string out_dir = fs::join(*dataset_dir, stamp);

    // 先に全部の例を検査する (途中まで書いたデータセットを残さない)。
    std::vector<std::string> lines;
    for (const auto& ex : json_array(batch, "examples")) lines.push_back(json_stringify(to_record(task, ex)));

    FtJob job;
    job.task = task;
    job.dataset = dataset;
    job.count = lines.size();
    job.data_path = fs::join(out_dir, "data.jsonl");
    job.job_path = fs::join(out_dir, "job.json");
    if (const auto m = batch.find("model"); m != batch.end() && m->is_object()) job.model = *m;
    job.created_at = created_at;

    std::string data = str::join(lines, "\n");
    if (!data.empty()) data += "\n";
    if (!fs::ensure_dir(out_dir) || !fs::write_file_atomic(job.data_path, data) ||
        !fs::write_file_atomic(job.job_path, json_stringify_pretty(to_json(job), 2)))
        throw std::runtime_error("FtSink: failed to write " + out_dir);

    // runner があれば学習を実行させる (core の外)。無ければ書き出して終わり。
    if (config_.runner) config_.runner(job);
}

FtRunner command_runner(std::string command, std::vector<std::string> args, std::string cwd, std::chrono::milliseconds timeout) {
    return [command = std::move(command), args = std::move(args), cwd = std::move(cwd), timeout](const FtJob& job) {
        proc::Options options;
        options.argv = {command};
        options.argv.insert(options.argv.end(), args.begin(), args.end());
        options.argv.push_back("--job");
        options.argv.push_back(job.job_path);
        options.cwd = cwd;
        options.timeout = timeout;
        const auto result = proc::run(options);
        if (!result.started) throw std::runtime_error("ft runner '" + command + "' failed to start: " + result.start_error);
        if (result.timed_out) throw std::runtime_error("ft runner '" + command + "' timed out");
        if (result.exit_code != 0) {
            throw std::runtime_error("ft runner '" + command + "' exited with code " + std::to_string(result.exit_code) + ": " +
                                     str::js_slice(result.err, -500));
        }
    };
}

} // namespace canalis::save
