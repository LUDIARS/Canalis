#pragma once
// DB ID 配下 / 1 ページ起点のページを再帰クロールする本体と、RawRecord への ①Crawl adapter
// (src/crawl/notion/{crawl,source}.ts の移植)。NotionApi は注入する (テストで偽物にする)。
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "canalis/core/pipeline.hpp"
#include "canalis/crawl/notion/types.hpp"

namespace canalis::crawl::notion {

/** DB の各 row を起点に、ページ内の child_page / child_database を max_depth まで辿る。 */
CrawlResult crawl_database(NotionApi& api, const std::string& database_id, const CrawlOptions& options = {});

/** 1 ページを起点 (depth 0) にクロールする。max_depth = 0 なら起点ページの本文だけ。 */
PageCrawlResult crawl_page(NotionApi& api, const std::string& page_id, const CrawlOptions& options = {});

struct NotionSourceConfig {
    /** クロール対象の Notion database id。 */
    std::string database_id;
    /** integration token。省略時は env NOTION_TOKEN → config の暗号化 token。 */
    std::optional<std::string> token;
    CrawlOptions crawl;
};

struct NotionSourceDeps {
    /** 注入する NotionApi (呼び出し側の所有)。null なら token から NotionApiClient を作る。 */
    NotionApi* api = nullptr;
    std::function<std::string()> now;
};

/** Notion DB を 1 source として扱う ①Crawl adapter。 */
class NotionSource final : public core::Source<NotionSourceConfig> {
public:
    explicit NotionSource(NotionSourceDeps deps = {});
    std::string_view name() const override { return "notion"; }
    /** database_id 未指定・token が無いときは std::runtime_error。 */
    std::vector<core::RawRecord> crawl(const NotionSourceConfig& config) override;

private:
    NotionSourceDeps deps_;
};

} // namespace canalis::crawl::notion
