#include "canalis/save/kuzu_writer.hpp"

#include <algorithm>
#include <stdexcept>

#include "canalis/base/strings.hpp"

namespace canalis::save {
namespace {

bool is_safe_name(std::string_view name) {
    if (name.empty()) return false;
    const auto alpha = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; };
    if (!alpha(name[0])) return false;
    return std::all_of(name.begin(), name.end(), [&](char c) { return alpha(c) || (c >= '0' && c <= '9'); });
}

std::string safe_label(const std::string& label) {
    if (!is_safe_name(label)) throw std::invalid_argument("unsafe graph label: " + label);
    return label;
}

/** props を `alias.k = $p_k` の SET 句とパラメタに展開する (Kuzu は SET n += $map を受けないため)。 */
std::string set_clauses(const std::string& alias, const Json& props, Json& params) {
    std::vector<std::string> clauses;
    if (!props.is_object()) return "";
    for (const auto& [k, v] : props.items()) {
        if (!is_safe_name(k)) throw std::invalid_argument("unsafe graph property: " + k);
        params["p_" + k] = v;
        clauses.push_back(alias + "." + k + " = $p_" + k);
    }
    return str::join(clauses, ", ");
}

} // namespace

KuzuSink::KuzuSink(CypherExecutor exec, KuzuSinkConfig config, std::string name)
    : exec_(std::move(exec)), key_prop_(config.key_prop.value_or("key")), name_(std::move(name)) {
    if (!exec_) throw std::invalid_argument("KuzuSink: CypherExecutor is required");
    if (!is_safe_name(key_prop_)) throw std::invalid_argument("unsafe graph property: " + key_prop_);
}

void KuzuSink::write_batch(const core::SinkBatch& batch) {
    for (const auto& node : json_array(batch, "nodes")) merge_node(node);
    for (const auto& edge : json_array(batch, "edges")) merge_edge(edge);
}

void KuzuSink::merge_node(const Json& node) {
    const std::string label = safe_label(json_string(node, "label"));
    Json params = Json::object();
    params["key"] = node.value("key", Json());
    const std::string set = set_clauses("n", node.value("props", Json::object()), params);
    exec_("MERGE (n:" + label + " {" + key_prop_ + ": $key})" + (set.empty() ? "" : " SET " + set), params);
}

void KuzuSink::merge_edge(const Json& edge) {
    const Json& from = json_object(edge, "from");
    const Json& to = json_object(edge, "to");
    const std::string from_label = safe_label(json_string(from, "label"));
    const std::string to_label = safe_label(json_string(to, "label"));
    const std::string rel_label = safe_label(json_string(edge, "label"));
    Json params = Json::object();
    params["fromKey"] = from.value("key", Json());
    params["toKey"] = to.value("key", Json());
    const std::string set = set_clauses("r", edge.value("props", Json::object()), params);
    exec_("MATCH (a:" + from_label + " {" + key_prop_ + ": $fromKey}), (b:" + to_label + " {" + key_prop_ + ": $toKey}) MERGE (a)-[r:" +
              rel_label + "]->(b)" + (set.empty() ? "" : " SET " + set),
          params);
}

} // namespace canalis::save
