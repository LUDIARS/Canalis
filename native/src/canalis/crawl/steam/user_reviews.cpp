#include "canalis/crawl/steam/user_reviews.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <unordered_set>

#include "canalis/base/strings.hpp"
#include "canalis/base/url.hpp"
#include "canalis/crawl/html_text.hpp"

namespace canalis::crawl::steam {
namespace {

struct Span {
    std::size_t start;
    std::size_t end;
};
/** src[from..] で最初に一致する位置 (無ければ nullopt)。 */
using Matcher = std::function<std::optional<Span>(std::string_view src, std::size_t from)>;

bool is_word(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }

Matcher literal(std::string_view text) {
    return [text](std::string_view src, std::size_t from) -> std::optional<Span> {
        const auto at = src.find(text, from);
        if (at == std::string_view::npos) return std::nullopt;
        return Span{at, at + text.size()};
    };
}

/** `prefix[^>]*>` (prefix の後、最初の `>` まで)。 */
Matcher open_tag(std::string_view prefix) {
    return [prefix](std::string_view src, std::size_t from) -> std::optional<Span> {
        const auto at = src.find(prefix, from);
        if (at == std::string_view::npos) return std::nullopt;
        const auto gt = src.find('>', at + prefix.size());
        if (gt == std::string_view::npos) return std::nullopt;
        return Span{at, gt + 1};
    };
}

/** `/<div class="content(?:\s[^"]*)?"[^>]*>/`。 */
std::optional<Span> content_open(std::string_view src, std::size_t from) {
    constexpr std::string_view prefix = "<div class=\"content";
    for (auto at = src.find(prefix, from); at != std::string_view::npos; at = src.find(prefix, at + 1)) {
        std::size_t k = at + prefix.size();
        if (k >= src.size()) return std::nullopt;
        if (html::is_ascii_space(src[k])) {
            k = src.find('"', k + 1);
            if (k == std::string_view::npos) return std::nullopt;
        } else if (src[k] != '"') {
            continue;
        }
        const auto gt = src.find('>', k + 1);
        if (gt == std::string_view::npos) return std::nullopt;
        return Span{at, gt + 1};
    }
    return std::nullopt;
}

/** `/<div class="(?:posted|received_compensation|review_rule_violation|reviewer_hardware_container)[\s"]/`。 */
std::optional<Span> content_end(std::string_view src, std::size_t from) {
    constexpr std::string_view prefix = "<div class=\"";
    constexpr std::string_view names[] = {"posted", "received_compensation", "review_rule_violation", "reviewer_hardware_container"};
    for (auto at = src.find(prefix, from); at != std::string_view::npos; at = src.find(prefix, at + 1)) {
        const std::size_t k = at + prefix.size();
        for (const auto name : names) {
            const std::size_t e = k + name.size();
            if (src.substr(k, name.size()) == name && e < src.size() && (html::is_ascii_space(src[e]) || src[e] == '"'))
                return Span{at, e + 1};
        }
    }
    return std::nullopt;
}

/** TS の between: 開始の一致の後から、終わりの一致の手前まで。終わりが無ければ最後まで。開始が無ければ nullopt。 */
std::optional<std::string_view> between(std::string_view src, const Matcher& start, const Matcher& end) {
    const auto s = start(src, 0);
    if (!s) return std::nullopt;
    const std::string_view rest = src.substr(s->end);
    const auto e = end(rest, 0);
    return e ? rest.substr(0, e->start) : rest;
}

/** `/<div\b[^>]*\bclass="review_box[\s"]/g` の一致を順に返す ([^>]* は貪欲なので、タグの中の最後の一致)。 */
std::vector<Span> review_boxes(std::string_view html) {
    constexpr std::string_view attr = "class=\"review_box";
    std::vector<Span> out;
    std::size_t pos = 0;
    while (true) {
        const auto p = html.find("<div", pos);
        if (p == std::string_view::npos) break;
        if (p + 4 < html.size() && is_word(html[p + 4])) {
            pos = p + 1;
            continue;
        }
        const auto gt = html.find('>', p + 4);
        const std::size_t limit = gt == std::string_view::npos ? html.size() : gt;
        std::optional<Span> found;
        for (auto at = html.substr(0, limit).rfind(attr); at != std::string_view::npos && at >= p + 4;
             at = at == 0 ? std::string_view::npos : html.substr(0, limit).rfind(attr, at - 1)) {
            const std::size_t e = at + attr.size();
            const bool boundary_before = !is_word(html[at - 1]);
            const bool next_ok = e < limit && (html::is_ascii_space(html[e]) || html[e] == '"');
            if (boundary_before && next_ok) {
                found = Span{p, e + 1};
                break;
            }
        }
        if (!found) {
            pos = p + 1;
            continue;
        }
        out.push_back(*found);
        pos = found->end;
    }
    return out;
}

/** `<br\s*\/?>` (大文字小文字を無視) を改行にする。 */
std::string br_to_newline(std::string_view s) {
    std::string out;
    std::size_t pos = 0;
    while (true) {
        const auto at = html::ifind(s, "<br", pos);
        if (at == std::string_view::npos) break;
        std::size_t k = at + 3;
        while (k < s.size() && html::is_ascii_space(s[k])) ++k;
        if (k < s.size() && s[k] == '/') ++k;
        if (k < s.size() && s[k] == '>') {
            out.append(s.substr(pos, at - pos));
            out += '\n';
            pos = k + 1;
        } else {
            out.append(s.substr(pos, at + 1 - pos));
            pos = at + 1;
        }
    }
    out.append(s.substr(pos));
    return out;
}

/** `[ \t]+` → 空白 1 つ。 */
std::string collapse_blanks(std::string_view s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == ' ' || s[i] == '\t') {
            while (i + 1 < s.size() && (s[i + 1] == ' ' || s[i + 1] == '\t')) ++i;
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}

/** `\n{3,}` → 改行 2 つ。 */
std::string limit_newlines(std::string_view s) {
    std::string out;
    for (std::size_t i = 0; i < s.size();) {
        if (s[i] != '\n') {
            out += s[i++];
            continue;
        }
        std::size_t j = i;
        while (j < s.size() && s[j] == '\n') ++j;
        out.append(std::min<std::size_t>(j - i, 2), '\n');
        i = j;
    }
    return out;
}

/** TS の user-reviews の stripTags (<br> は改行、タグは削除、空白・改行を畳む)。 */
std::string strip_tags(std::string_view s) {
    return str::trim(limit_newlines(collapse_blanks(html::decode_basic_entities(html::replace_tags(br_to_newline(s), ""), false))));
}

std::optional<std::string> non_empty(std::optional<std::string_view> html_part) {
    if (!html_part || html_part->empty()) return std::nullopt;
    std::string text = strip_tags(*html_part);
    if (text.empty()) return std::nullopt;
    return text;
}

bool is_steam_id64(const std::string& id) {
    return id.size() == 17 && std::all_of(id.begin(), id.end(), [](char c) { return c >= '0' && c <= '9'; });
}

} // namespace

std::vector<SteamUserReviewEntry> parse_user_reviews_html(std::string_view html, const std::string& steam_id) {
    std::vector<SteamUserReviewEntry> out;
    const auto boxes = review_boxes(html);
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        const std::size_t end = i + 1 < boxes.size() ? boxes[i + 1].start : html.size();
        const std::string_view block = html.substr(boxes[i].end, end - boxes[i].end);

        std::optional<std::int64_t> app_id;
        for (auto at = block.find("/recommended/"); at != std::string_view::npos; at = block.find("/recommended/", at + 1)) {
            std::size_t k = at + 13;
            while (k < block.size() && block[k] >= '0' && block[k] <= '9') ++k;
            if (k > at + 13 && k - at - 13 <= 15) {
                app_id = str::parse_int(block.substr(at + 13, k - at - 13));
                break;
            }
        }
        if (!app_id) continue;

        const std::string title = strip_tags(between(block, literal("<div class=\"title\">"), literal("</div>")).value_or(""));
        std::optional<bool> recommended;
        if (html::ifind(title, "not recommended") != std::string_view::npos) recommended = false;
        else if (html::ifind(title, "recommended") != std::string_view::npos) recommended = true;

        // content の中には spoiler などの入れ子の div があるので、posted (など) を終わりにして広めに切り出す。
        const auto content = between(block, content_open, content_end);
        if (!content) continue;

        SteamUserReviewEntry e;
        e.steam_id = steam_id;
        e.app_id = *app_id;
        e.recommended = recommended;
        e.text = strip_tags(*content);
        e.hours_text = non_empty(between(block, open_tag("<div class=\"hours\""), literal("</div>")));
        e.posted_text = non_empty(between(block, open_tag("<div class=\"posted\""), literal("</div>")));
        e.url = "https://steamcommunity.com/profiles/" + steam_id + "/recommended/" + std::to_string(*app_id) + "/";
        out.push_back(std::move(e));
    }
    return out;
}

std::vector<SteamUserReviewEntry> fetch_user_reviews(const FetchUserReviewsOptions& options) {
    if (!is_steam_id64(options.steam_id)) throw std::invalid_argument("fetchUserReviews: steamId must be SteamID64 (17 digits)");
    const int max_pages = options.max_pages.value_or(50);
    if (max_pages < 0) throw std::invalid_argument("steam maxPages must be a non-negative integer");
    const SteamHttp http(options.http);
    const std::string lang = options.language.value_or("english");

    std::vector<SteamUserReviewEntry> out;
    std::unordered_set<std::int64_t> seen_apps;
    for (int page = 1; page <= max_pages; ++page) {
        const std::string html = http.get_text("https://steamcommunity.com/profiles/" + options.steam_id + "/recommended/?p=" +
                                               std::to_string(page) + "&l=" + url::encode_component(lang));
        if (page == 1 && (html::ifind(html, "profile_private_info") != std::string_view::npos ||
                          html::ifind(html, "This profile is private") != std::string_view::npos))
            return {};
        const auto entries = parse_user_reviews_html(html, options.steam_id);
        if (entries.empty()) break;
        int added = 0;
        for (const auto& e : entries) {
            if (!seen_apps.insert(e.app_id).second) continue;  // 末尾のページが繰り返されるときの重複を除く
            out.push_back(e);
            ++added;
        }
        if (added == 0) break;  // すべて既出 = ページが進んでいない
        http.polite_wait();
    }
    return out;
}

} // namespace canalis::crawl::steam
