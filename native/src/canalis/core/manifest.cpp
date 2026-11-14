#include "canalis/core/manifest.hpp"

#include <stdexcept>

#include "canalis/base/fsutil.hpp"
#include "canalis/base/strings.hpp"

namespace canalis::core {
namespace {

constexpr std::size_t kMaxManifestBytes = 4u << 20;

SinkSpec sink_spec(const Json& v, const std::string& label, const std::string& where) {
    if (!v.is_object()) throw std::runtime_error("manifest-loader: " + label + ": " + where + " must be an object");
    SinkSpec s;
    s.adapter = json_string(v, "adapter");
    s.accepts = json_string(v, "accepts");
    if (s.adapter.empty() || s.accepts.empty())
        throw std::runtime_error("manifest-loader: " + label + ": " + where + " requires adapter and accepts");
    if (const auto c = v.find("config"); c != v.end()) s.config = *c;
    return s;
}

} // namespace

Manifest manifest_from_json(const Json& m, const std::string& label) {
    if (!m.is_object()) throw std::runtime_error("manifest-loader: " + label + " is not an object");
    const auto pipeline = json_opt_string(m, "pipeline");
    if (!pipeline) throw std::runtime_error("manifest-loader: " + label + ": pipeline (string) is required");
    const auto crawl = m.find("crawl");
    if (crawl == m.end() || !crawl->is_object()) throw std::runtime_error("manifest-loader: " + label + ": crawl is required");

    Manifest out;
    out.pipeline = *pipeline;
    std::size_t i = 0;
    for (const auto& s : json_array(*crawl, "sources")) {
        if (!s.is_object() || json_string(s, "adapter").empty())
            throw std::runtime_error("manifest-loader: " + label + ": crawl.sources[" + std::to_string(i) + "] requires adapter");
        out.sources.push_back({json_string(s, "adapter"), s.value("config", Json())});
        ++i;
    }
    if (const auto r = crawl->find("rawSave"); r != crawl->end() && !r->is_null()) out.raw_save = sink_spec(*r, label, "crawl.rawSave");
    out.transform = json_opt_string(m, "transform");
    i = 0;
    for (const auto& s : json_array(m, "save")) out.save.push_back(sink_spec(s, label, "save[" + std::to_string(i++) + "]"));
    if (const auto r = m.find("replayFrom"); r != m.end() && !r->is_null()) out.replay_from = sink_spec(*r, label, "replayFrom");
    return out;
}

Manifest load_manifest(const std::string& path) {
    const std::string lower = str::to_lower_ascii(path);
    if (str::ends_with(lower, ".yaml") || str::ends_with(lower, ".yml")) {
        throw std::runtime_error("manifest-loader: " + path +
                                 ": YAML の manifest は C++ 版では読みません。JSON に変換してください (spec/plan/cpp-port.md D3)");
    }
    const auto text = fs::read_file(path, kMaxManifestBytes);
    if (!text) throw std::runtime_error("manifest-loader: cannot read " + path);
    JsonLimits limits;
    limits.max_bytes = kMaxManifestBytes;
    std::string error;
    const auto json = parse_json(*text, limits, &error);
    if (!json) throw std::runtime_error("manifest-loader: " + path + ": invalid JSON (" + error + ")");
    return manifest_from_json(*json, path);
}

} // namespace canalis::core
