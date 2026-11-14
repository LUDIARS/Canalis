#pragma once
// notion-public クローラーの型 (src/crawl/notion-public/types.ts の移植)。
// API トークン不要の公開 Notion ページを、導入済みのブラウザでレンダリングして取得する。
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "canalis/base/json.hpp"

namespace canalis::crawl::notion_public {

struct NotionPublicCrawlOptions {
    /** ページ全体のタイムアウト (ms)。既定 30000。 */
    std::optional<std::int64_t> timeout;
    /** スクロール間の待機時間 (ms)。既定 800。 */
    std::optional<std::int64_t> scroll_delay;
    /** 最大スクロール回数。既定 15。 */
    std::optional<int> max_scrolls;
};

struct NotionPublicSourceConfig {
    /** クロール対象の公開 Notion ページ URL。 */
    std::string url;
    NotionPublicCrawlOptions options;
};

/** PageFetcher に渡す解決済みのオプション。 */
struct FetchPageOptions {
    std::int64_t timeout = 30'000;
    std::int64_t scroll_delay = 800;
    int max_scrolls = 15;
};

/** NotionPublicCrawlOptions に既定値を入れる。 */
FetchPageOptions resolve_options(const NotionPublicCrawlOptions& options);

/** ブラウザから抽出した 1 ブロック。type は h1 / h2 / h3 / li / oli / quote / code / callout / divider / text。 */
struct ExtractedBlock {
    std::string type;
    std::string text;
};

/** ブラウザ内で抽出したページ全体のデータ。 */
struct ExtractionResult {
    std::string title;
    std::vector<ExtractedBlock> blocks;
    /** 本文中の同一サイト Notion ページへのリンク (絶対 URL・重複なし)。子ページ辿りに使う。 */
    std::optional<std::vector<std::string>> links;
};

/** TS と同じキー (title, blocks[{type, text}], links) の object。links が無ければ出さない。 */
Json to_json(const ExtractionResult& result);
/** ブラウザの evaluate の戻り値から読む (上限つき。形が違えば std::runtime_error)。 */
ExtractionResult extraction_from_json(const Json& value);

/** PageFetcher の戻り値。 */
struct FetchedPage {
    std::string url;
    std::string title;
    /** blocks_to_markdown で作った本文 Markdown。 */
    std::string markdown;
    /** verbatim 保持用の生データ。 */
    ExtractionResult raw;
};

/** ページ取得の抽象 (テスト時に差し替える)。 */
class PageFetcher {
public:
    virtual ~PageFetcher() = default;
    /** 失敗は例外 (std::exception)。 */
    virtual FetchedPage fetch(const std::string& url, const FetchPageOptions& options) = 0;
    /** 保持しているブラウザ等を解放する (複数ページ取得後に 1 回呼ぶ)。 */
    virtual void close() {}
};

/** crawl_public_pages の設定。 */
struct NotionPublicPageCrawlOptions {
    NotionPublicCrawlOptions page;
    /** 起点ページ (=0) から辿るリンク先ページの深さ。既定 0 (起点のみ)。 */
    std::optional<int> max_depth;
    /** 取得する最大ページ数 (安全弁)。既定 50。 */
    std::optional<int> max_pages;
};

struct NotionPublicCrawledPage {
    std::string url;
    std::string title;
    int depth = 0;
    /** リンク元ページの URL (起点は "")。 */
    std::string parent_url;
    std::string markdown;
};

struct CrawlError {
    std::string url;
    std::string message;
};

struct NotionPublicCrawlResult {
    std::string root_url;
    std::vector<NotionPublicCrawledPage> pages;
    std::vector<CrawlError> errors;
    /** max_pages で打ち切ったか。 */
    bool truncated = false;
};

/** TS と同じキー (rootUrl, pages[{url, title, depth, parentUrl, markdown}], errors[{url, message}], truncated)。 */
Json to_json(const NotionPublicCrawlResult& result);

} // namespace canalis::crawl::notion_public
