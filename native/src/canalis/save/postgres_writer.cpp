#include "canalis/save/postgres_writer.hpp"

#include <algorithm>
#include <stdexcept>

#include "canalis/base/strings.hpp"

namespace canalis::save {
namespace {

bool is_plain_identifier(std::string_view name) {
    if (name.empty()) return false;
    const auto alpha = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; };
    if (!alpha(name[0])) return false;
    return std::all_of(name.begin(), name.end(), [&](char c) { return alpha(c) || (c >= '0' && c <= '9'); });
}

/** Postgres の識別子を引用する ("..."、中の " は重ねる)。TS と同じく、普通の名前でなく " を含むものは拒否する。 */
std::string quote_ident(const std::string& name) {
    if (!is_plain_identifier(name) && name.find('"') != std::string::npos) throw std::invalid_argument("unsafe identifier: " + name);
    if (name.find('\0') != std::string::npos) throw std::invalid_argument("unsafe identifier (NUL)");
    return "\"" + str::replace_all(name, "\"", "\"\"") + "\"";
}

} // namespace

std::vector<SqlStatement> build_upsert_statements(const Json& u) {
    std::vector<SqlStatement> out;
    const Json& rows = json_array(u, "rows");
    if (rows.empty()) return out;
    const std::string table = quote_ident(json_string(u, "table"));
    std::vector<std::string> conflict_key;
    for (const auto& k : json_array(u, "conflictKey"))
        if (k.is_string()) conflict_key.push_back(k.get<std::string>());
    std::vector<std::string> quoted_conflict;
    for (const auto& k : conflict_key) quoted_conflict.push_back(quote_ident(k));
    const std::string conflict = str::join(quoted_conflict, ", ");

    for (const auto& row : rows) {
        if (!row.is_object() || row.empty()) continue;
        std::vector<std::string> cols, placeholders, updates;
        SqlStatement stmt;
        int i = 0;
        for (const auto& [col, value] : row.items()) {
            const std::string q = quote_ident(col);
            cols.push_back(q);
            placeholders.push_back("$" + std::to_string(++i));
            stmt.params.push_back(value);
            if (std::find(conflict_key.begin(), conflict_key.end(), col) == conflict_key.end())
                updates.push_back(q + " = EXCLUDED." + q);
        }
        const std::string on_conflict = updates.empty() ? "ON CONFLICT (" + conflict + ") DO NOTHING"
                                                        : "ON CONFLICT (" + conflict + ") DO UPDATE SET " + str::join(updates, ", ");
        stmt.query = "INSERT INTO " + table + " (" + str::join(cols, ", ") + ") VALUES (" + str::join(placeholders, ", ") + ") " +
                     on_conflict;
        out.push_back(std::move(stmt));
    }
    return out;
}

PostgresSink::PostgresSink(SqlExecutor exec, std::string name) : exec_(std::move(exec)), name_(std::move(name)) {
    if (!exec_) throw std::invalid_argument("PostgresSink: SqlExecutor is required");
}

void PostgresSink::write_batch(const core::SinkBatch& batch) {
    for (const auto& u : json_array(batch, "upserts"))
        for (const auto& stmt : build_upsert_statements(u)) exec_(stmt.query, stmt.params);
}

} // namespace canalis::save
