#pragma once
// Notion クローラーの型 (src/crawl/notion/types.ts の移植)。指定トークンで DB ID 配下のページを再帰クロールする。
// Notion の block / page は種類ごとにフィールドが違うので、TS と同じく緩く (JSON のまま) 持つ。
#include <optional>
#include <string>
#include <vector>

#include "canalis/base/json.hpp"

namespace canalis::crawl::notion {

/** ページネーション結果。 */
struct Paged {
    std::vector<Json> results;
    std::optional<std::string> next_cursor;
    bool has_more = false;
};

/** crawl が依存する Notion API 操作 (テストで偽物に差し替える)。実装は client の NotionApiClient。失敗は例外。 */
class NotionApi {
public:
    virtual ~NotionApi() = default;
    virtual Paged query_database(const std::string& database_id, const std::optional<std::string>& cursor) = 0;
    virtual Paged get_block_children(const std::string& block_id, const std::optional<std::string>& cursor) = 0;
    virtual Json retrieve_page(const std::string& page_id) = 0;
};

struct CrawlOptions {
    /** 起点 (DB row / 起点ページ = 0) から潜る最大深さ。既定 3。 */
    std::optional<int> max_depth;
    /** クロールする最大ページ数 (安全弁)。既定 500。 */
    std::optional<int> max_pages;
    /** child_database を見つけたら、その DB の row も辿るか。既定 true。 */
    std::optional<bool> include_child_databases;
};

/** クロールで得た 1 ページ。kind は "database_row" | "child_page" | "page"。 */
struct NotionCrawledPage {
    std::string id;
    std::string url;
    std::string title;
    std::string kind;
    std::string parent_id;
    int depth = 0;
    /** ページ本文を Markdown 化したもの。 */
    std::string markdown;
    /** プロパティを簡易 key → string に落としたもの (キーの順は Notion の応答の順)。 */
    Json properties = Json::object();
};

struct CrawlError {
    std::string id;
    std::string stage;
    std::string message;
};

struct CrawlResult {
    std::string database_id;
    std::vector<NotionCrawledPage> pages;
    std::vector<CrawlError> errors;
    bool truncated = false;
};

struct PageCrawlResult {
    std::string page_id;
    std::vector<NotionCrawledPage> pages;
    std::vector<CrawlError> errors;
    bool truncated = false;
};

/** TS と同じキー順 (id, url, title, kind, parentId, depth, markdown, properties)。 */
Json to_json(const NotionCrawledPage& page);
Json to_json(const CrawlError& error);

} // namespace canalis::crawl::notion
