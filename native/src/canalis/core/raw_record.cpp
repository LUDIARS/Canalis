#include "canalis/core/raw_record.hpp"

namespace canalis::core {

Json to_json(const RawRecord& record) {
    Json out = Json::object();
    out["source"] = record.source;
    out["sourceId"] = record.source_id;
    out["fetchedAt"] = record.fetched_at;
    if (record.url) out["url"] = *record.url;
    if (record.title) out["title"] = *record.title;
    if (record.text) out["text"] = *record.text;
    out["raw"] = record.raw;
    if (record.meta && record.meta->is_object()) out["meta"] = *record.meta;
    return out;
}

std::optional<RawRecord> raw_record_from_json(const Json& value) {
    if (!value.is_object()) return std::nullopt;
    const auto source = json_opt_string(value, "source");
    const auto source_id = json_opt_string(value, "sourceId");
    const auto fetched_at = json_opt_string(value, "fetchedAt");
    if (!source || !source_id || !fetched_at) return std::nullopt;
    RawRecord r;
    r.source = *source;
    r.source_id = *source_id;
    r.fetched_at = *fetched_at;
    r.url = json_opt_string(value, "url");
    r.title = json_opt_string(value, "title");
    r.text = json_opt_string(value, "text");
    if (const auto raw = value.find("raw"); raw != value.end()) r.raw = *raw;
    if (const auto meta = value.find("meta"); meta != value.end() && meta->is_object()) r.meta = *meta;
    return r;
}

} // namespace canalis::core
