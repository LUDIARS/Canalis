#include "canalis/crawl/notion/types.hpp"

namespace canalis::crawl::notion {

Json to_json(const NotionCrawledPage& page) {
    Json out = Json::object();
    out["id"] = page.id;
    out["url"] = page.url;
    out["title"] = page.title;
    out["kind"] = page.kind;
    out["parentId"] = page.parent_id;
    out["depth"] = page.depth;
    out["markdown"] = page.markdown;
    out["properties"] = page.properties;
    return out;
}

Json to_json(const CrawlError& error) {
    Json out = Json::object();
    out["id"] = error.id;
    out["stage"] = error.stage;
    out["message"] = error.message;
    return out;
}

} // namespace canalis::crawl::notion
