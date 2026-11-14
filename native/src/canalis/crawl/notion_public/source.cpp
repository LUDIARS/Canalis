#include "canalis/crawl/notion_public/source.hpp"

#include <algorithm>
#include <deque>
#include <stdexcept>
#include <unordered_set>

#include "canalis/base/time.hpp"
#include "canalis/base/url.hpp"
#include "canalis/crawl/notion_public/fetcher.hpp"

namespace canalis::crawl::notion_public {
namespace {

/** 呼び出し側の fetcher を使うか、自分で作って終わったら閉じる。 */
class FetcherScope {
public:
    explicit FetcherScope(PageFetcher* given) : given_(given) {
        if (!given_) owned_ = std::make_unique<BrowserFetcher>();
    }
    ~FetcherScope() {
        if (owned_) {
            try {
                owned_->close();
            } catch (...) {
            }
        }
    }
    PageFetcher& get() { return given_ ? *given_ : *owned_; }

private:
    PageFetcher* given_;
    std::unique_ptr<PageFetcher> owned_;
};

/** 末尾スラッシュ・クエリ・ハッシュを落として同じページを判定する (TS の normalize)。 */
std::string normalize(const std::string& u) {
    const auto parsed = url::parse(u);
    if (!parsed) return u;
    std::string out = parsed->origin() + parsed->path;
    if (!out.empty() && out.back() == '/') out.pop_back();
    return out;
}

} // namespace

NotionPublicSource::NotionPublicSource(NotionPublicSourceDeps deps) : deps_(std::move(deps)) {
    if (!deps_.now) deps_.now = [] { return time::now_iso(); };
}

std::vector<core::RawRecord> NotionPublicSource::crawl(const NotionPublicSourceConfig& config) {
    if (config.url.empty()) throw std::runtime_error("NotionPublicSource: url is required");
    FetchedPage page;
    {
        FetcherScope fetcher(deps_.fetcher);
        page = fetcher.get().fetch(config.url, resolve_options(config.options));
    }
    core::RawRecord record;
    record.source = "notion-public";
    record.source_id = config.url;
    record.fetched_at = deps_.now();
    record.url = config.url;
    if (!page.title.empty()) record.title = page.title;
    if (!page.markdown.empty()) record.text = page.markdown;
    record.raw = to_json(page.raw);
    record.meta = Json{{"url", config.url}, {"blockCount", page.raw.blocks.size()}};
    return {record};
}

NotionPublicCrawlResult crawl_public_pages(const std::string& root_url, const NotionPublicPageCrawlOptions& options,
                                           PageFetcher* fetcher) {
    const int max_depth = options.max_depth.value_or(0);
    const std::size_t max_pages = static_cast<std::size_t>(std::max(0, options.max_pages.value_or(50)));
    const FetchPageOptions fetch_options = resolve_options(options.page);

    struct QueueItem {
        std::string url;
        int depth;
        std::string parent_url;
    };
    NotionPublicCrawlResult result;
    result.root_url = root_url;
    std::unordered_set<std::string> visited;
    std::deque<QueueItem> queue{{root_url, 0, ""}};

    FetcherScope scope(fetcher);
    while (!queue.empty()) {
        if (result.pages.size() >= max_pages) {
            result.truncated = true;
            break;
        }
        QueueItem item = std::move(queue.front());
        queue.pop_front();
        if (!visited.insert(normalize(item.url)).second) continue;

        FetchedPage fetched;
        try {
            fetched = scope.get().fetch(item.url, fetch_options);
        } catch (const std::exception& e) {
            result.errors.push_back({item.url, e.what()});
            continue;
        }
        result.pages.push_back({item.url, fetched.title, item.depth, item.parent_url, fetched.markdown});

        if (item.depth >= max_depth || !fetched.raw.links) continue;
        for (const auto& link : *fetched.raw.links)
            if (!visited.count(normalize(link))) queue.push_back({link, item.depth + 1, item.url});
    }
    return result;
}

} // namespace canalis::crawl::notion_public
