#include "canalis/base/url.hpp"

#include "canalis/base/strings.hpp"

namespace canalis::url {

std::string Url::host_for_header() const {
    if (host.find(':') != std::string::npos) return "[" + host + "]";
    return host;
}

std::string Url::origin() const {
    std::string out = scheme + "://" + host_for_header();
    if (explicit_port) out += ":" + std::to_string(port);
    return out;
}

std::string Url::href() const {
    std::string out = origin() + path;
    if (!query.empty()) out += "?" + query;
    if (!fragment.empty()) out += "#" + fragment;
    return out;
}

std::optional<Url> parse(std::string_view text) {
    Url u;
    const auto sep = text.find("://");
    if (sep == std::string_view::npos || sep == 0) return std::nullopt;
    u.scheme = str::to_lower_ascii(text.substr(0, sep));
    std::uint16_t default_port;
    if (u.scheme == "http" || u.scheme == "ws") default_port = 80;
    else if (u.scheme == "https" || u.scheme == "wss") default_port = 443;
    else return std::nullopt;
    std::string_view rest = text.substr(sep + 3);
    const auto end_authority = rest.find_first_of("/?#");
    std::string_view authority = rest.substr(0, end_authority);
    std::string_view tail = end_authority == std::string_view::npos ? std::string_view{} : rest.substr(end_authority);
    if (authority.empty() || authority.find('@') != std::string_view::npos) return std::nullopt;
    for (char c : authority) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc <= 0x20 || uc == 0x7F || c == '\\') return std::nullopt;
    }
    std::string_view host;
    std::string_view port;
    if (authority.front() == '[') {
        const auto close = authority.find(']');
        if (close == std::string_view::npos) return std::nullopt;
        host = authority.substr(1, close - 1);
        auto after = authority.substr(close + 1);
        if (!after.empty()) {
            if (after.front() != ':') return std::nullopt;
            port = after.substr(1);
        }
    } else {
        const auto colon = authority.rfind(':');
        if (colon != std::string_view::npos) {
            host = authority.substr(0, colon);
            port = authority.substr(colon + 1);
        } else {
            host = authority;
        }
    }
    if (host.empty()) return std::nullopt;
    u.host = str::to_lower_ascii(host);
    u.port = default_port;
    if (!port.empty()) {
        auto p = str::parse_int(port);
        if (!p || *p <= 0 || *p > 65535) return std::nullopt;
        if (*p != default_port) {
            u.port = static_cast<std::uint16_t>(*p);
            u.explicit_port = true;
        }
    }
    const auto hash = tail.find('#');
    if (hash != std::string_view::npos) {
        u.fragment = std::string(tail.substr(hash + 1));
        tail = tail.substr(0, hash);
    }
    const auto q = tail.find('?');
    u.path = std::string(q == std::string_view::npos ? tail : tail.substr(0, q));
    if (u.path.empty()) u.path = "/";
    if (q != std::string_view::npos) u.query = std::string(tail.substr(q + 1));
    u.target = u.path + (u.query.empty() ? "" : "?" + u.query);
    for (char c : u.target) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc < 0x20 || uc == 0x7F || c == ' ') return std::nullopt;  // 要求行の注入を防ぐ
    }
    return u;
}

std::string encode_component(std::string_view s) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
                                c == '_' || c == '.' || c == '!' || c == '~' || c == '*' || c == '\'' || c == '(' || c == ')';
        if (unreserved) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0xF]);
        }
    }
    return out;
}

namespace {
int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
} // namespace

std::optional<std::string> decode_component(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '%') {
            out.push_back(s[i]);
            continue;
        }
        if (i + 2 >= s.size()) return std::nullopt;
        const int hi = hexval(s[i + 1]);
        const int lo = hexval(s[i + 2]);
        if (hi < 0 || lo < 0) return std::nullopt;
        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
    }
    if (!str::is_valid_utf8(out)) return std::nullopt;
    return out;
}

std::string decode_form(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            out.push_back(' ');
        } else if (s[i] == '%' && i + 2 < s.size() && hexval(s[i + 1]) >= 0 && hexval(s[i + 2]) >= 0) {
            out.push_back(static_cast<char>((hexval(s[i + 1]) << 4) | hexval(s[i + 2])));
            i += 2;
        } else {
            out.push_back(s[i]);
        }
    }
    return str::sanitize_utf8(out);
}

std::vector<std::pair<std::string, std::string>> parse_query(std::string_view query) {
    std::vector<std::pair<std::string, std::string>> out;
    if (!query.empty() && query.front() == '?') query.remove_prefix(1);
    for (const auto& part : str::split(query, "&")) {
        if (part.empty()) continue;
        const auto eq = part.find('=');
        if (eq == std::string::npos) out.emplace_back(decode_form(part), "");
        else out.emplace_back(decode_form(part.substr(0, eq)), decode_form(part.substr(eq + 1)));
    }
    return out;
}

std::string build_query(const std::vector<std::pair<std::string, std::string>>& params) {
    auto enc = [](std::string_view s) {
        static constexpr char hex[] = "0123456789ABCDEF";
        std::string out;
        for (unsigned char c : s) {
            const bool keep = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                              c == '.' || c == '*';
            if (keep) out.push_back(static_cast<char>(c));
            else if (c == ' ') out.push_back('+');
            else {
                out.push_back('%');
                out.push_back(hex[c >> 4]);
                out.push_back(hex[c & 0xF]);
            }
        }
        return out;
    };
    std::string out;
    for (const auto& [k, v] : params) {
        if (!out.empty()) out.push_back('&');
        out += enc(k) + "=" + enc(v);
    }
    return out;
}

namespace {

/** RFC 3986 5.2.4 remove_dot_segments。 */
std::string remove_dot_segments(std::string_view path) {
    std::vector<std::string> out;
    const auto parts = str::split(path, "/");
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const std::string& seg = parts[i];
        const bool last = i + 1 == parts.size();
        if (seg == ".") {
            if (last) out.emplace_back();
        } else if (seg == "..") {
            if (out.size() > 1) out.pop_back();
            if (last) out.emplace_back();
        } else {
            out.push_back(seg);
        }
    }
    std::string joined = str::join(out, "/");
    if (joined.empty() || joined.front() != '/') joined.insert(joined.begin(), '/');
    return joined;
}

bool has_scheme(std::string_view ref) {
    const auto colon = ref.find(':');
    if (colon == std::string_view::npos || colon == 0) return false;
    for (std::size_t i = 0; i < colon; ++i) {
        const char c = ref[i];
        const bool alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (!(alpha || (i > 0 && ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.')))) return false;
    }
    return true;
}

} // namespace

std::string encode_unsafe(std::string_view s) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc <= 0x20 || uc >= 0x7F || c == '"' || c == '<' || c == '>' || c == '`') {
            out += '%';
            out += kHex[uc >> 4];
            out += kHex[uc & 0x0F];
        } else {
            out += c;
        }
    }
    return out;
}

std::optional<std::string> resolve(std::string_view base, std::string_view ref) {
    const auto b = parse(encode_unsafe(base));  // new URL と同じく base の空白・非 ASCII も %XX にする
    if (!b || (b->scheme != "http" && b->scheme != "https")) return std::nullopt;
    const std::string r = encode_unsafe(str::trim(ref));
    if (has_scheme(r)) {
        const auto abs = parse(r);
        if (!abs || (abs->scheme != "http" && abs->scheme != "https")) return std::nullopt;
        Url u = *abs;
        u.path = remove_dot_segments(u.path);
        return u.href();
    }
    if (r.rfind("//", 0) == 0) return resolve(base, b->scheme + ":" + r);
    Url u = *b;
    std::string_view rest = r;
    std::string fragment;
    if (const auto hash = rest.find('#'); hash != std::string_view::npos) {
        fragment = std::string(rest.substr(hash + 1));
        rest = rest.substr(0, hash);
    }
    std::optional<std::string> query;
    if (const auto q = rest.find('?'); q != std::string_view::npos) {
        query = std::string(rest.substr(q + 1));
        rest = rest.substr(0, q);
    }
    if (rest.empty()) {
        if (query) u.query = *query;  // "?x" はパスを保ち、クエリを替える。"#x" だけならクエリも保つ
    } else {
        std::string merged;
        if (rest.front() == '/') merged = std::string(rest);
        else merged = u.path.substr(0, u.path.rfind('/') + 1) + std::string(rest);
        u.path = remove_dot_segments(merged);
        u.query = query.value_or("");
    }
    u.fragment = fragment;
    return u.href();
}

} // namespace canalis::url
