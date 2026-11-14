#include "canalis/save/raw_writer.hpp"

#include <algorithm>
#include <stdexcept>

#include "canalis/base/fsutil.hpp"
#include "canalis/base/strings.hpp"
#include "canalis/base/time.hpp"

namespace canalis::save {
namespace {

// 読み戻す JSONL の上限 (1 本・1 行)。raw は本文を丸ごと持つので大きめにする。
constexpr std::size_t kMaxFileBytes = 1u << 30;
constexpr std::size_t kMaxLineBytes = 64u << 20;

} // namespace

JsonlRawSink::JsonlRawSink(JsonlRawSinkConfig config) : config_(std::move(config)) {}

std::string JsonlRawSink::write(const std::vector<core::RawRecord>& records) const {
    const std::string stamp = str::replace_all(config_.stamp.value_or(time::now_iso()), ":", "-");
    const std::string file = fs::join(config_.dir, stamp + ".jsonl");
    std::vector<std::string> lines;
    lines.reserve(records.size());
    for (const auto& r : records) lines.push_back(json_stringify(core::to_json(r)));
    std::string body = str::join(lines, "\n");
    if (!body.empty()) body += "\n";
    if (!fs::ensure_dir(fs::dirname(file)) || !fs::write_file_atomic(file, body))
        throw std::runtime_error("failed to write " + file);
    return file;
}

std::vector<core::RawRecord> load_raw_records_from_file(const std::string& file) {
    const auto text = fs::read_file(file, kMaxFileBytes);
    if (!text) throw std::runtime_error("failed to read " + file);
    JsonLimits limits;
    limits.max_bytes = kMaxLineBytes;
    limits.max_elements = 5'000'000;
    std::vector<core::RawRecord> out;
    std::size_t line_no = 0;
    for (const auto& line : str::split(*text, "\n")) {
        ++line_no;
        const std::string trimmed = str::trim(line);
        if (trimmed.empty()) continue;
        std::string error;
        const auto json = parse_json(trimmed, limits, &error);
        const auto record = json ? core::raw_record_from_json(*json) : std::nullopt;
        if (!record) throw std::runtime_error(file + ":" + std::to_string(line_no) + ": invalid RawRecord " + error);
        out.push_back(*record);
    }
    return out;
}

std::vector<core::RawRecord> load_raw_records_from_dir(const std::string& dir) {
    std::vector<std::string> files;
    for (const auto& name : fs::list_dir(dir))
        if (str::ends_with(name, ".jsonl")) files.push_back(name);
    std::sort(files.begin(), files.end());
    std::vector<core::RawRecord> out;
    for (const auto& f : files) {
        auto part = load_raw_records_from_file(fs::join(dir, f));
        out.insert(out.end(), std::make_move_iterator(part.begin()), std::make_move_iterator(part.end()));
    }
    return out;
}

} // namespace canalis::save
