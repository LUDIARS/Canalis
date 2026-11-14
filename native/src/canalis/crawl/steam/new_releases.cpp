#include "canalis/crawl/steam/new_releases.hpp"

#include <stdexcept>
#include <unordered_set>

#include "canalis/base/strings.hpp"
#include "canalis/base/url.hpp"
#include "canalis/crawl/html_text.hpp"

namespace canalis::crawl::steam {
namespace {

constexpr int kPageSize = 50;
constexpr std::string_view kAppIdAttr = "data-ds-appid=\"";
// ページ送りの安全弁。
constexpr int kMaxSearchPages = 10'000;

struct Span {
    std::size_t start;
    std::size_t end;
};

/**
 * `/<a\s+[^>]*data-ds-appid="/g` の一致を順に返す。[^>]* は貪欲なので、
 * 1 つのタグの中では最後の `data-ds-appid="` で一致する (タグの `>` は越えない)。
 */
std::vector<Span> row_markers(std::string_view html) {
    std::vector<Span> out;
    std::size_t pos = 0;
    while (true) {
        const auto p = html.find("<a", pos);
        if (p == std::string_view::npos) break;
        if (p + 2 >= html.size() || !html::is_ascii_space(html[p + 2])) {
            pos = p + 1;
            continue;
        }
        const auto gt = html.find('>', p + 3);
        const std::size_t limit = gt == std::string_view::npos ? html.size() : gt;
        const auto region = html.substr(0, limit);
        const auto last = region.rfind(kAppIdAttr);
        if (last == std::string_view::npos || last < p + 3) {
            pos = p + 1;
            continue;
        }
        out.push_back({p, last + kAppIdAttr.size()});
        pos = last + kAppIdAttr.size();
    }
    return out;
}

/** `<[^>]+>` → 空白、実体参照、`\s+` → 空白、前後の空白を除く (TS の stripTags)。 */
std::string strip_tags(std::string_view s) {
    return str::trim(html::collapse_js_space(html::decode_basic_entities(html::replace_tags(s, " "), false), 1));
}

/** `open([\s\S]*?)close` の中身 (最初の open と、その後の最初の close)。 */
std::optional<std::string_view> lazy_between(std::string_view s, std::string_view open, std::string_view close) {
    const auto a = s.find(open);
    if (a == std::string_view::npos) return std::nullopt;
    const auto b = s.find(close, a + open.size());
    if (b == std::string_view::npos) return std::nullopt;
    return s.substr(a + open.size(), b - a - open.size());
}

/** `/<div class="col search_released[^"]*">([\s\S]*?)<\/div>/` の中身。 */
std::optional<std::string_view> released_cell(std::string_view s) {
    constexpr std::string_view prefix = "<div class=\"col search_released";
    std::size_t from = 0;
    while (true) {
        const auto q = s.find(prefix, from);
        if (q == std::string_view::npos) return std::nullopt;
        const auto quote = s.find('"', q + prefix.size());
        if (quote == std::string_view::npos) return std::nullopt;
        if (quote + 1 < s.size() && s[quote + 1] == '>') {
            const auto end = s.find("</div>", quote + 2);
            if (end == std::string_view::npos) return std::nullopt;
            return s.substr(quote + 2, end - quote - 2);
        }
        from = q + 1;
    }
}

} // namespace

std::vector<SteamSearchEntry> parse_search_results_html(std::string_view html) {
    std::vector<SteamSearchEntry> out;
    const auto markers = row_markers(html);
    for (std::size_t i = 0; i < markers.size(); ++i) {
        const std::size_t end = i + 1 < markers.size() ? markers[i + 1].start : html.size();
        const std::string_view chunk = html.substr(markers[i].end, end - markers[i].end);
        std::size_t digits = 0;
        while (digits < chunk.size() && chunk[digits] >= '0' && chunk[digits] <= '9') ++digits;
        if (digits == 0 || digits > 15) continue;  // バンドル (data-ds-appid="1,2") は先頭の id を採る
        const auto app_id = str::parse_int(chunk.substr(0, digits));
        const auto title = lazy_between(chunk, "<span class=\"title\">", "</span>");
        if (!app_id || !title) continue;
        SteamSearchEntry e;
        e.app_id = *app_id;
        e.title = strip_tags(*title);
        e.url = "https://store.steampowered.com/app/" + std::to_string(*app_id) + "/";
        if (const auto released = released_cell(chunk)) {
            std::string text = strip_tags(*released);
            if (!text.empty()) e.release_text = std::move(text);
        }
        out.push_back(std::move(e));
    }
    return out;
}

std::vector<SteamSearchEntry> fetch_new_releases(const FetchNewReleasesOptions& options) {
    const int max_apps = options.max_apps.value_or(100);
    if (max_apps < 0) throw std::invalid_argument("steam maxApps must be a non-negative integer");
    const SteamHttp http(options.http);
    const std::string cc = options.country_code.value_or("jp");
    const std::string lang = options.language.value_or("japanese");

    std::vector<SteamSearchEntry> out;
    std::unordered_set<std::int64_t> seen;
    for (int page = 0, start = 0; page < kMaxSearchPages && static_cast<int>(out.size()) < max_apps; ++page, start += kPageSize) {
        const std::string u = "https://store.steampowered.com/search/results/?query&start=" + std::to_string(start) +
                              "&count=" + std::to_string(kPageSize) + "&sort_by=Released_DESC&category1=998&infinite=1&cc=" +
                              url::encode_component(cc) + "&l=" + url::encode_component(lang);
        const Json data = http.get_json(u);
        if (const auto s = data.find("success"); s != data.end() && !(s->is_number() && s->get<double>() == 1))
            throw std::runtime_error("steam search success=" + json_stringify(*s));
        const auto entries = parse_search_results_html(json_string(data, "results_html"));
        if (entries.empty()) break;
        for (const auto& e : entries) {
            if (!seen.insert(e.app_id).second) continue;  // ページを跨ぐ重複 (掲載順の変動) を除く
            out.push_back(e);
            if (static_cast<int>(out.size()) >= max_apps) break;
        }
        if (const auto total = json_opt_number(data, "total_count"); total && start + kPageSize >= *total) break;
        http.polite_wait();
    }
    return out;
}

} // namespace canalis::crawl::steam
