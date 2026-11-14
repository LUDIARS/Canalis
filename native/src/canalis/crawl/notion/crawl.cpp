#include "canalis/crawl/notion/crawl.hpp"

#include <deque>
#include <memory>
#include <stdexcept>
#include <unordered_set>

#include "canalis/base/time.hpp"
#include "canalis/config/store.hpp"
#include "canalis/crawl/notion/blocks.hpp"
#include "canalis/crawl/notion/client.hpp"

namespace canalis::crawl::notion {
namespace {

// ページネーションの安全弁 (応答が has_more を返し続けても止まる)。1 回 100 件なので 100 万件まで。
constexpr int kMaxPagesPerListing = 10'000;
// 1 ページ内の入れ子展開の安全弁 (toggle / column の深い入れ子)。
constexpr int kMaxNestedIndent = 64;

struct QueueItem {
    std::string id;
    std::string kind;
    std::string parent_id;
    int depth = 0;
    /** DB query で取得済みの page object (あれば retrieve_page を省く)。 */
    std::optional<Json> page;
};

template <class Fetch>
std::vector<Json> all_pages(Fetch&& fetch) {
    std::vector<Json> out;
    std::optional<std::string> cursor;
    for (int i = 0; i < kMaxPagesPerListing; ++i) {
        Paged res = fetch(cursor);
        for (auto& r : res.results) out.push_back(std::move(r));
        if (!res.has_more || !res.next_cursor || res.next_cursor->empty()) return out;
        cursor = res.next_cursor;
    }
    throw std::runtime_error("notion: pagination did not end");
}

std::vector<Json> query_all_rows(NotionApi& api, const std::string& database_id) {
    return all_pages([&](const std::optional<std::string>& c) { return api.query_database(database_id, c); });
}

std::vector<Json> all_children(NotionApi& api, const std::string& block_id) {
    return all_pages([&](const std::optional<std::string>& c) { return api.get_block_children(block_id, c); });
}

struct PageContent {
    std::vector<IndentedBlock> flat;
    std::vector<std::string> child_page_ids;
    std::vector<std::string> child_database_ids;
};

/**
 * 1 ページの block ツリーを集める。child_page / child_database は「下位ページ」として id を集め、中には潜らない (queue が処理)。
 * それ以外の has_children (toggle / column 等) は本ページの一部として展開する。
 */
void walk(NotionApi& api, const std::string& block_id, int indent, PageContent& out) {
    if (indent > kMaxNestedIndent) throw std::runtime_error("notion: blocks are nested too deeply");
    for (auto& block : all_children(api, block_id)) {
        if (!block.is_object()) continue;
        const std::string type = json_string(block, "type");
        const std::string id = json_string(block, "id");
        const bool has_children = json_truthy(block.value("has_children", Json()));
        out.flat.push_back({block, indent});
        if (type == "child_page") {
            out.child_page_ids.push_back(id);
            continue;
        }
        if (type == "child_database") {
            out.child_database_ids.push_back(id);
            continue;
        }
        if (has_children) walk(api, id, indent + 1, out);
    }
}

struct QueueState {
    std::deque<QueueItem> queue;
    std::vector<NotionCrawledPage> pages;
    std::vector<CrawlError> errors;
};

/** queue を max_depth / max_pages まで消化する (DB 起点・ページ起点で共有)。打ち切ったら true。 */
bool drain_queue(NotionApi& api, QueueState& state, const CrawlOptions& options) {
    const int max_depth = options.max_depth.value_or(3);
    const std::size_t max_pages = static_cast<std::size_t>(std::max(0, options.max_pages.value_or(500)));
    const bool include_child_databases = options.include_child_databases.value_or(true);
    std::unordered_set<std::string> visited;

    while (!state.queue.empty()) {
        if (state.pages.size() >= max_pages) return true;
        QueueItem item = std::move(state.queue.front());
        state.queue.pop_front();
        if (!visited.insert(item.id).second) continue;

        Json page;
        if (item.page) {
            page = *item.page;
        } else {
            try {
                page = api.retrieve_page(item.id);
            } catch (const std::exception& e) {
                state.errors.push_back({item.id, "retrievePage", e.what()});
                continue;
            }
        }
        if (!page.is_object()) page = Json::object();
        if (json_truthy(page.value("archived", Json()))) continue;

        PageContent content;
        try {
            walk(api, item.id, 0, content);
        } catch (const std::exception& e) {
            state.errors.push_back({item.id, "collectContent", e.what()});
            content = {};
        }

        NotionCrawledPage crawled;
        crawled.id = item.id;
        crawled.url = json_string(page, "url");
        crawled.title = extract_title(page);
        crawled.kind = item.kind;
        crawled.parent_id = item.parent_id;
        crawled.depth = item.depth;
        crawled.markdown = blocks_to_markdown(content.flat);
        crawled.properties = simplify_properties(page);
        state.pages.push_back(std::move(crawled));

        if (item.depth >= max_depth) continue;
        for (const auto& cp : content.child_page_ids)
            if (!visited.count(cp)) state.queue.push_back({cp, "child_page", item.id, item.depth + 1, std::nullopt});
        if (!include_child_databases) continue;
        for (const auto& cdb : content.child_database_ids) {
            try {
                for (auto& row : query_all_rows(api, cdb)) {
                    const std::string row_id = json_string(row, "id");
                    if (!visited.count(row_id))
                        state.queue.push_back({row_id, "database_row", cdb, item.depth + 1, std::move(row)});
                }
            } catch (const std::exception& e) {
                state.errors.push_back({cdb, "queryChildDatabase", e.what()});
            }
        }
    }
    return false;
}

} // namespace

CrawlResult crawl_database(NotionApi& api, const std::string& database_id, const CrawlOptions& options) {
    QueueState state;
    try {
        for (auto& page : query_all_rows(api, database_id)) {
            const std::string id = json_string(page, "id");
            state.queue.push_back({id, "database_row", database_id, 0, std::move(page)});
        }
    } catch (const std::exception& e) {
        state.errors.push_back({database_id, "queryDatabase", e.what()});
    }
    const bool truncated = drain_queue(api, state, options);
    return {database_id, std::move(state.pages), std::move(state.errors), truncated};
}

PageCrawlResult crawl_page(NotionApi& api, const std::string& page_id, const CrawlOptions& options) {
    QueueState state;
    state.queue.push_back({page_id, "page", "", 0, std::nullopt});
    const bool truncated = drain_queue(api, state, options);
    return {page_id, std::move(state.pages), std::move(state.errors), truncated};
}

NotionSource::NotionSource(NotionSourceDeps deps) : deps_(std::move(deps)) {
    if (!deps_.now) deps_.now = [] { return time::now_iso(); };
}

std::vector<core::RawRecord> NotionSource::crawl(const NotionSourceConfig& config) {
    if (config.database_id.empty()) throw std::runtime_error("NotionSource: databaseId is required");
    std::unique_ptr<NotionApiClient> owned;
    NotionApi* api = deps_.api;
    if (!api) {
        auto token = config.token ? config.token : config::resolve_notion_token();
        if (!token || token->empty())
            throw std::runtime_error("NotionSource: token is required (config.token or env NOTION_TOKEN)");
        const auto settings = config::notion_settings();
        NotionClientConfig cfg;
        cfg.token = *token;
        cfg.notion_version = settings.version;
        if (settings.min_interval_ms) cfg.min_interval_ms = static_cast<std::int64_t>(*settings.min_interval_ms);
        owned = std::make_unique<NotionApiClient>(std::move(cfg));
        api = owned.get();
    }

    const CrawlResult result = crawl_database(*api, config.database_id, config.crawl);
    const std::string fetched_at = deps_.now();
    std::vector<core::RawRecord> records;
    for (const auto& p : result.pages) {
        core::RawRecord r;
        r.source = "notion";
        r.source_id = p.id;
        r.fetched_at = fetched_at;
        if (!p.url.empty()) r.url = p.url;
        if (!p.title.empty()) r.title = p.title;
        if (!p.markdown.empty()) r.text = p.markdown;
        r.raw = to_json(p);  // クロール結果 (properties を含む) を verbatim 保持
        Json meta = Json::object();
        meta["kind"] = p.kind;
        meta["parentId"] = p.parent_id;
        meta["depth"] = p.depth;
        meta["databaseId"] = config.database_id;
        meta["properties"] = p.properties;
        meta["truncated"] = result.truncated;
        r.meta = std::move(meta);
        records.push_back(std::move(r));
    }
    return records;
}

} // namespace canalis::crawl::notion
