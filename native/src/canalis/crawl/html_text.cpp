#include "canalis/crawl/html_text.hpp"

#include "canalis/base/strings.hpp"

namespace canalis::crawl::html {

std::size_t ifind(std::string_view haystack, std::string_view needle, std::size_t from) {
    if (needle.empty() || haystack.size() < needle.size()) return std::string_view::npos;
    for (std::size_t i = from; i + needle.size() <= haystack.size(); ++i)
        if (str::iequals_ascii(haystack.substr(i, needle.size()), needle)) return i;
    return std::string_view::npos;
}

std::string replace_tags(std::string_view s, std::string_view replacement) {
    std::string out;
    out.reserve(s.size());
    std::size_t pos = 0;
    while (pos < s.size()) {
        const auto lt = s.find('<', pos);
        if (lt == std::string_view::npos) break;
        const auto gt = s.find('>', lt + 1);
        if (gt == std::string_view::npos || gt == lt + 1) {
            out.append(s.substr(pos, lt + 1 - pos));
            pos = lt + 1;
            continue;
        }
        out.append(s.substr(pos, lt - pos));
        out.append(replacement);
        pos = gt + 1;
    }
    out.append(s.substr(pos));
    return out;
}

std::string replace_blocks(std::string_view html, std::string_view open, std::string_view close) {
    std::string out;
    out.reserve(html.size());
    std::size_t pos = 0;
    while (pos < html.size()) {
        const auto start = ifind(html, open, pos);
        if (start == std::string_view::npos) break;
        const auto end = ifind(html, close, start + open.size());
        if (end == std::string_view::npos) break;
        out.append(html.substr(pos, start - pos));
        out += ' ';
        pos = end + close.size();
    }
    out.append(html.substr(pos));
    return out;
}

std::string collapse_js_space(std::string_view s, std::size_t min_run) {
    std::string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        std::size_t run = 0;
        std::size_t j = i;
        while (j < s.size()) {
            std::size_t next = j;
            if (!str::is_js_space(str::next_codepoint(s, next))) break;
            ++run;
            j = next;
        }
        if (run >= min_run && run > 0) {
            out += ' ';
            i = j;
        } else if (run > 0) {
            out.append(s.substr(i, j - i));
            i = j;
        } else {
            std::size_t next = i;
            str::next_codepoint(s, next);
            out.append(s.substr(i, next - i));
            i = next;
        }
    }
    return out;
}

std::string decode_basic_entities(std::string_view s, bool with_nbsp) {
    std::string out = str::replace_all(s, "&amp;", "&");
    out = str::replace_all(out, "&lt;", "<");
    out = str::replace_all(out, "&gt;", ">");
    out = str::replace_all(out, "&quot;", "\"");
    out = str::replace_all(out, "&#39;", "'");
    if (with_nbsp) out = str::replace_all(out, "&nbsp;", " ");
    return out;
}

bool is_ascii_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'; }

} // namespace canalis::crawl::html
