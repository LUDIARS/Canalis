#include "canalis/crawl/notion/url.hpp"

#include <algorithm>

#include "canalis/base/strings.hpp"
#include "canalis/base/url.hpp"

namespace canalis::crawl::notion {
namespace {

bool is_hex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

/** 末尾が `[0-9a-f]{32}` (大文字小文字を問わない) なら、その 32 文字。 */
std::optional<std::string> trailing_hex32(std::string_view s) {
    if (s.size() < 32) return std::nullopt;
    const auto tail = s.substr(s.size() - 32);
    if (!std::all_of(tail.begin(), tail.end(), is_hex)) return std::nullopt;
    return std::string(tail);
}

/** 末尾が UUID (8-4-4-4-12) なら、その 36 文字。 */
std::optional<std::string> trailing_uuid(std::string_view s) {
    if (s.size() < 36) return std::nullopt;
    const auto tail = s.substr(s.size() - 36);
    for (std::size_t i = 0; i < tail.size(); ++i) {
        const bool dash = i == 8 || i == 13 || i == 18 || i == 23;
        if (dash ? tail[i] != '-' : !is_hex(tail[i])) return std::nullopt;
    }
    return std::string(tail);
}

std::string format_uuid(std::string_view hex) {
    const std::string h = str::to_lower_ascii(hex);
    return h.substr(0, 8) + "-" + h.substr(8, 4) + "-" + h.substr(12, 4) + "-" + h.substr(16, 4) + "-" + h.substr(20);
}

std::optional<std::string> id_from_segment(std::string_view segment) {
    if (auto uuid = trailing_uuid(segment)) return str::to_lower_ascii(*uuid);
    if (auto hex = trailing_hex32(segment)) return format_uuid(*hex);
    return std::nullopt;
}

std::optional<url::Url> to_url(std::string_view input) { return url::parse(url::encode_unsafe(str::trim(input))); }

/** URL の本文中の終わり: JS の \s と < > ( ) " ' ` (TS の /https?:\/\/[^\s<>()"'`]+/)。 */
bool ends_url(std::uint32_t cp) {
    return str::is_js_space(cp) || cp == '<' || cp == '>' || cp == '(' || cp == ')' || cp == '"' || cp == '\'' ||
           cp == '`';
}

/** 末尾から外す句読点 (TS の /[.,;:!?、。」』）\]]+$/u)。 */
bool trailing_punct(std::uint32_t cp) {
    switch (cp) {
    case '.': case ',': case ';': case ':': case '!': case '?': case ']':
    case 0x3001: case 0x3002: case 0x300D: case 0x300F: case 0xFF09:
        return true;
    default:
        return false;
    }
}

std::string strip_trailing_punct(const std::string& s) {
    auto cps = str::codepoints(s);
    while (!cps.empty() && trailing_punct(cps.back())) cps.pop_back();
    return str::from_codepoints(cps);
}

} // namespace

bool is_notion_url(std::string_view input) {
    const auto u = to_url(input);
    if (!u || (u->scheme != "http" && u->scheme != "https")) return false;
    const std::string& host = u->host;
    for (const char* tld : {"so", "site", "com"}) {
        const std::string base = std::string("notion.") + tld;
        if (host == base || str::ends_with(host, "." + base)) return true;
    }
    return false;
}

std::optional<std::string> parse_notion_page_id(std::string_view input) {
    if (!is_notion_url(input)) return std::nullopt;
    const auto u = to_url(input);
    for (const auto& [key, value] : url::parse_query(u->query)) {
        if (key != "p") continue;
        if (!value.empty())
            if (auto id = id_from_segment(value)) return id;
        break;  // URLSearchParams.get は最初の値だけを見る
    }
    const auto segments = str::split(u->path, "/");
    std::string last;
    for (const auto& s : segments)
        if (!s.empty()) last = s;
    if (last.empty()) return std::nullopt;
    const auto decoded = url::decode_component(last);
    if (!decoded) return std::nullopt;  // TS は decodeURIComponent が投げる (C++ は id 無し扱い)
    return id_from_segment(*decoded);
}

std::vector<std::string> find_notion_urls(std::string_view text) {
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t start = std::string_view::npos;
        for (std::size_t i = pos; i + 7 <= text.size(); ++i) {
            if (str::iequals_ascii(text.substr(i, 7), "http://") ||
                (i + 8 <= text.size() && str::iequals_ascii(text.substr(i, 8), "https://"))) {
                start = i;
                break;
            }
        }
        if (start == std::string_view::npos) break;
        const std::size_t scheme_len = str::iequals_ascii(text.substr(start, 5), "https") ? 8 : 7;
        std::size_t end = start + scheme_len;
        while (end < text.size()) {
            std::size_t next = end;
            if (ends_url(str::next_codepoint(text, next))) break;
            end = next;
        }
        if (end == start + scheme_len) {  // 「https://」だけ (+ が 1 文字以上を求める)
            pos = start + 1;
            continue;
        }
        const std::string url = strip_trailing_punct(std::string(text.substr(start, end - start)));
        if (is_notion_url(url) && std::find(out.begin(), out.end(), url) == out.end()) out.push_back(url);
        pos = end;
    }
    return out;
}

} // namespace canalis::crawl::notion
