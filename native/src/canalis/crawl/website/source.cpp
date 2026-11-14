#include "canalis/crawl/website/source.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

#include "canalis/base/strings.hpp"
#include "canalis/base/time.hpp"
#include "canalis/base/url.hpp"

namespace canalis::crawl::website {
namespace {

constexpr std::string_view kDefaultUa = "Canalis-Crawler/1.0 (LUDIARS; +https://github.com/LUDIARS)";
constexpr std::int64_t kDefaultTimeoutMs = 10'000;
constexpr std::int64_t kMaxTextLength = 50'000;  // UTF-16 単位 (TS の slice(0, 50_000))
constexpr std::size_t kMaxLinks = 100;

/** haystack[from..] で needle を ASCII の大文字小文字を無視して探す。 */
std::size_t ifind(std::string_view haystack, std::string_view needle, std::size_t from = 0) {
    if (needle.empty() || haystack.size() < needle.size()) return std::string_view::npos;
    for (std::size_t i = from; i + needle.size() <= haystack.size(); ++i)
        if (str::iequals_ascii(haystack.substr(i, needle.size()), needle)) return i;
    return std::string_view::npos;
}

/** `<open[\s\S]*?</close>` (大文字小文字を無視) を空白 1 つに置き換える。閉じが無ければそこで止める (正規表現と同じく一致しない)。 */
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

/** `<[^>]+>` を空白 1 つに置き換える。 */
std::string replace_tags(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    std::size_t pos = 0;
    while (pos < s.size()) {
        const auto lt = s.find('<', pos);
        if (lt == std::string_view::npos) break;
        const auto gt = s.find('>', lt + 1);
        if (gt == std::string_view::npos || gt == lt + 1) {
            // "<>" や閉じの無い "<" はタグではない。"<" を残して先へ進む。
            out.append(s.substr(pos, lt + 1 - pos));
            pos = lt + 1;
            continue;
        }
        out.append(s.substr(pos, lt - pos));
        out += ' ';
        pos = gt + 1;
    }
    out.append(s.substr(pos));
    return out;
}

/** `\s{2,}` → " " (JS の \s と同じ空白の判定)。 */
std::string collapse_runs(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        std::size_t run_start = i;
        std::size_t run = 0;
        std::size_t j = i;
        while (j < s.size()) {
            std::size_t next = j;
            const auto cp = str::next_codepoint(s, next);
            if (!str::is_js_space(cp)) break;
            ++run;
            j = next;
        }
        if (run >= 2) {
            out += ' ';
            i = j;
        } else if (run == 1) {
            out.append(s.substr(run_start, j - run_start));
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

/** TS の decodeHtmlEntities と同じ 6 つを、同じ順で置き換える (&amp; が先)。 */
std::string decode_html_entities(std::string_view s) {
    std::string out = str::replace_all(s, "&amp;", "&");
    out = str::replace_all(out, "&lt;", "<");
    out = str::replace_all(out, "&gt;", ">");
    out = str::replace_all(out, "&quot;", "\"");
    out = str::replace_all(out, "&#39;", "'");
    return str::replace_all(out, "&nbsp;", " ");
}

std::optional<std::string> extract_title(std::string_view html) {
    const auto open = ifind(html, "<title");
    if (open == std::string_view::npos) return std::nullopt;
    const auto gt = html.find('>', open + 6);
    if (gt == std::string_view::npos) return std::nullopt;
    const auto close = ifind(html, "</title>", gt + 1);
    if (close == std::string_view::npos) return std::nullopt;
    return decode_html_entities(str::trim(html.substr(gt + 1, close - gt - 1)));
}

/** `href="([^"]+)"` (大文字小文字を無視) の値を順に取る。 */
std::vector<std::string> href_values(std::string_view html) {
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (true) {
        const auto at = ifind(html, "href=\"", pos);
        if (at == std::string_view::npos) break;
        const auto start = at + 6;
        const auto end = html.find('"', start);
        if (end == std::string_view::npos) break;
        if (end == start) {
            pos = start;  // 空の値は一致しない。次の位置から探し直す
            continue;
        }
        out.emplace_back(html.substr(start, end - start));
        pos = end + 1;
    }
    return out;
}

http::ClientResponse default_fetch(const http::ClientRequest& request) { return http::fetch(request); }

} // namespace

ParsedPage fallback_parse(std::string_view html, std::string_view url) {
    ParsedPage page;
    page.title = extract_title(html);

    std::string stripped = replace_blocks(html, "<script", "</script>");
    stripped = replace_blocks(stripped, "<style", "</style>");
    stripped = str::trim(collapse_runs(replace_tags(stripped)));
    page.text = str::js_slice(decode_html_entities(stripped), 0, kMaxTextLength);

    const auto parsed = url::parse(url);
    const std::string base = parsed ? parsed->origin() : std::string();
    std::unordered_set<std::string> seen;
    for (const auto& href : href_values(html)) {
        const auto abs = url::resolve(url, href);
        if (!abs || base.empty() || !str::starts_with(*abs, base)) continue;
        if (!seen.insert(*abs).second) continue;
        page.links.push_back(*abs);
        if (page.links.size() >= kMaxLinks) break;
    }
    return page;
}

std::string to_source_id(std::string_view url) {
    // new URL() と同じく、パスの空白・非 ASCII は %XX にしてから読む。
    const auto u = url::parse(url::encode_unsafe(url));
    if (!u) return str::js_slice(url, 0, 200);
    const std::string id = u->host_for_header() + u->path;
    std::string out;
    std::size_t i = 0;
    while (i < id.size()) {
        const char c = id[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                        c == '_' || c == '/' || c == '-';
        std::size_t next = i;
        const auto cp = str::next_codepoint(id, next);
        if (ok) out += c;
        else out.append(cp > 0xFFFF ? 2 : 1, '_');  // JS は UTF-16 の単位ごとに置き換える
        i = next;
    }
    return out.substr(0, 200);
}

WebsiteSource::WebsiteSource(WebsiteSourceDeps deps) : deps_(std::move(deps)) {
    if (!deps_.fetch) deps_.fetch = default_fetch;
    if (!deps_.now) deps_.now = [] { return time::now_iso(); };
}

std::vector<core::RawRecord> WebsiteSource::crawl(const WebsiteSourceConfig& config) {
    if (config.url.empty()) throw std::runtime_error("WebsiteSource: url is required");

    http::ClientRequest request;
    request.url = config.url;
    request.headers.set("User-Agent", config.user_agent.value_or(std::string(kDefaultUa)));
    request.timeout = std::chrono::milliseconds(config.timeout_ms.value_or(kDefaultTimeoutMs));
    request.connect_timeout = std::min(request.connect_timeout, request.timeout);
    request.allow_private_network = deps_.allow_private_network;
    request.max_response_bytes = deps_.max_response_bytes;

    const http::ClientResponse res = deps_.fetch(request);
    if (!res.transport_ok) throw std::runtime_error("WebsiteSource: " + res.error + " " + config.url);
    const std::string final_url = res.final_url.empty() ? config.url : res.final_url;
    if (!res.ok()) throw std::runtime_error("WebsiteSource: HTTP " + std::to_string(res.status) + " " + config.url);

    const ParsedPage parsed = deps_.html_parser ? deps_.html_parser->parse(res.body, final_url)
                                                : fallback_parse(res.body, final_url);
    core::RawRecord record;
    record.source = "website";
    record.source_id = to_source_id(final_url);
    record.fetched_at = deps_.now();
    record.url = final_url;
    record.title = parsed.title;
    record.text = parsed.text;
    record.raw = Json{{"html", res.body}, {"url", final_url}};
    Json links = Json::array();
    for (const auto& l : parsed.links) links.push_back(l);
    record.meta = Json{{"links", links}, {"originalUrl", config.url}};
    return {record};
}

} // namespace canalis::crawl::website
