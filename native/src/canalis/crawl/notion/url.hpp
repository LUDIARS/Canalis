#pragma once
// Notion の URL ⇄ page id の決定論変換と、本文中の Notion リンクの検出 (src/crawl/notion/url.ts の移植)。
// 対応: www.notion.so / app.notion.com / *.notion.site。`?p=<id>` (peek 表示) は path より優先する。
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace canalis::crawl::notion {

/** Notion のホスト (notion.so / notion.com / *.notion.site) を持つ http(s) の URL か。 */
bool is_notion_url(std::string_view input);

/** Notion URL から page (または database) の id を UUID 形式 (小文字・ハイフン付き) で取り出す。無ければ nullopt。 */
std::optional<std::string> parse_notion_page_id(std::string_view input);

/** 本文中の Notion URL を出現順・重複なしで列挙する (末尾の句読点は除く)。 */
std::vector<std::string> find_notion_urls(std::string_view text);

} // namespace canalis::crawl::notion
