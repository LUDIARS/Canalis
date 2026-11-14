#pragma once
// ①Crawl adapter: 公開 Notion ページをブラウザでレンダリングして取得し、RawRecord へ変換する
// (src/crawl/notion-public/source.ts の移植)。複数ページのリンク辿りは crawl_public_pages。
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "canalis/core/pipeline.hpp"
#include "canalis/crawl/notion_public/types.hpp"

namespace canalis::crawl::notion_public {

struct NotionPublicSourceDeps {
    /** 差し替える PageFetcher (呼び出し側の所有なので close しない)。null なら BrowserFetcher を作り、終わったら閉じる。 */
    PageFetcher* fetcher = nullptr;
    /** fetchedAt の時刻源。 */
    std::function<std::string()> now;
};

class NotionPublicSource final : public core::Source<NotionPublicSourceConfig> {
public:
    explicit NotionPublicSource(NotionPublicSourceDeps deps = {});
    std::string_view name() const override { return "notion-public"; }
    /** url 未指定・取得の失敗は std::runtime_error。 */
    std::vector<core::RawRecord> crawl(const NotionPublicSourceConfig& config) override;

private:
    NotionPublicSourceDeps deps_;
};

/**
 * 公開 Notion ページを起点 (depth 0) に幅優先で取得する (src/crawl/notion-public/crawl.ts の移植)。
 * fetcher が null なら BrowserFetcher を作り、終わったら閉じる。取得の失敗は errors に積んで続ける。
 */
NotionPublicCrawlResult crawl_public_pages(const std::string& root_url, const NotionPublicPageCrawlOptions& options = {},
                                           PageFetcher* fetcher = nullptr);

} // namespace canalis::crawl::notion_public
