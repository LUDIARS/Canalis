#include "canalis/crawl/notion/blocks.hpp"

#include "canalis/base/strings.hpp"

namespace canalis::crawl::notion {
namespace {

/** obj[key] が object ならそれ、違えば空 object (TS の `(x as {...} | undefined)?.y`)。 */
const Json& sub(const Json& obj, const std::string& key) { return json_object(obj, key); }

/** 値が文字列ならそれ、違えば fallback (TS の `?? ''` と `as string`)。 */
std::string str_or(const Json& obj, const std::string& key, const std::string& fallback = "") {
    const auto v = json_opt_string(obj, key);
    return v ? *v : fallback;
}

std::string block_text(const Json& block) {
    return rich_text_to_plain(sub(block, json_string(block, "type")).value("rich_text", Json()));
}

/** JS の String(x) (formula の値に使う)。 */
std::string js_string(const Json& v) {
    if (v.is_string()) return v.get<std::string>();
    if (v.is_boolean()) return v.get<bool>() ? "true" : "false";
    if (v.is_number()) return js_number_to_string(v.get<double>());
    if (v.is_null()) return "null";
    if (v.is_array()) {
        std::vector<std::string> parts;
        for (const auto& e : v) parts.push_back(e.is_null() ? "" : js_string(e));
        return str::join(parts, ",");
    }
    return "[object Object]";
}

/** `a ?? b ?? ...` の最初の null / 無し でない値。 */
const Json* first_present(const Json& obj, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        const auto it = obj.find(k);
        if (it != obj.end() && !it->is_null()) return &*it;
    }
    return nullptr;
}

std::size_t array_size(const Json& obj, const std::string& key) {
    const auto it = obj.find(key);
    return it != obj.end() && it->is_array() ? it->size() : 0;
}

} // namespace

std::string rich_text_to_plain(const Json& rich) {
    if (!rich.is_array()) return "";
    std::string out;
    for (const auto& r : rich) {
        if (r.is_object()) out += str_or(r, "plain_text");
    }
    return out;
}

std::string block_to_markdown(const Json& block) {
    const std::string t = block_text(block);
    const std::string type = json_string(block, "type");
    if (type == "heading_1") return "# " + t;
    if (type == "heading_2") return "## " + t;
    if (type == "heading_3") return "### " + t;
    if (type == "paragraph") return t;
    if (type == "bulleted_list_item") return "- " + t;
    if (type == "numbered_list_item") return "1. " + t;
    if (type == "to_do") return std::string("- [") + (json_truthy(sub(block, "to_do").value("checked", Json())) ? "x" : " ") + "] " + t;
    if (type == "toggle") return "- " + t;
    if (type == "quote" || type == "callout") return "> " + t;
    if (type == "code") return "```" + str_or(sub(block, "code"), "language") + "\n" + t + "\n```";
    if (type == "divider") return "---";
    if (type == "child_page") return "## " + str_or(sub(block, "child_page"), "title");
    if (type == "child_database") return "## " + str_or(sub(block, "child_database"), "title") + " (database)";
    if (type == "bookmark" || type == "embed" || type == "link_preview") {
        const std::string url = str_or(sub(block, type), "url");
        return url.empty() ? "" : "[" + url + "](" + url + ")";
    }
    return t;  // table_row 等は rich_text を持たないこともあるが best-effort
}

std::string blocks_to_markdown(const std::vector<IndentedBlock>& blocks) {
    std::vector<std::string> lines;
    for (const auto& [block, indent] : blocks) {
        const std::string md = block_to_markdown(block);
        if (md.empty()) continue;
        lines.push_back(str::repeat("  ", static_cast<std::size_t>(std::max(0, indent))) + md);
    }
    return str::join(lines, "\n");
}

std::string extract_title(const Json& page) {
    const Json& props = json_object(page, "properties");
    for (const auto& value : props) {
        if (value.is_object() && json_string(value, "type") == "title")
            return str::trim(rich_text_to_plain(value.value("title", Json())));
    }
    return "";
}

std::string prop_to_string(const Json& p) {
    if (!p.is_object()) return "";
    const std::string type = json_string(p, "type");
    if (type == "title" || type == "rich_text") return rich_text_to_plain(p.value(type, Json()));
    if (type == "number") {
        const auto it = p.find("number");
        if (it == p.end() || it->is_null()) return "";
        return js_string(*it);
    }
    if (type == "select" || type == "status") return str_or(sub(p, type), "name");
    if (type == "multi_select") {
        std::vector<std::string> names;
        for (const auto& s : json_array(p, "multi_select")) {
            const std::string name = s.is_object() ? str_or(s, "name") : "";
            if (!name.empty()) names.push_back(name);
        }
        return str::join(names, ", ");
    }
    if (type == "date") {
        const Json& d = sub(p, "date");
        const std::string start = str_or(d, "start");
        if (start.empty()) return "";
        const std::string end = str_or(d, "end");
        return end.empty() ? start : start + " ~ " + end;
    }
    if (type == "checkbox") return json_truthy(p.value("checkbox", Json())) ? "true" : "false";
    if (type == "url" || type == "email" || type == "phone_number" || type == "created_time" || type == "last_edited_time")
        return str_or(p, type);
    if (type == "people") return std::to_string(array_size(p, "people")) + " people";
    if (type == "files") return std::to_string(array_size(p, "files")) + " files";
    if (type == "formula") {
        const Json& f = sub(p, "formula");
        const Json* v = first_present(f, {"string", "number", "boolean", "date"});
        return v ? js_string(*v) : "";
    }
    return "";
}

Json simplify_properties(const Json& page) {
    Json out = Json::object();
    for (const auto& [key, value] : json_object(page, "properties").items()) {
        const std::string s = str::trim(prop_to_string(value));
        if (!s.empty()) out[key] = s;
    }
    return out;
}

} // namespace canalis::crawl::notion
