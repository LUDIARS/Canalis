#include "canalis/crawl/notion_public/types.hpp"

#include <stdexcept>

namespace canalis::crawl::notion_public {
namespace {

// ブラウザから受け取る抽出結果の上限 (異常に大きいページで確保し続けない)。
constexpr std::size_t kMaxBlocks = 100'000;
constexpr std::size_t kMaxLinks = 10'000;

} // namespace

FetchPageOptions resolve_options(const NotionPublicCrawlOptions& options) {
    FetchPageOptions out;
    out.timeout = options.timeout.value_or(30'000);
    out.scroll_delay = options.scroll_delay.value_or(800);
    out.max_scrolls = options.max_scrolls.value_or(15);
    return out;
}

Json to_json(const ExtractionResult& result) {
    Json blocks = Json::array();
    for (const auto& b : result.blocks) {
        Json block = Json::object();
        block["type"] = b.type;
        block["text"] = b.text;
        blocks.push_back(std::move(block));
    }
    Json out = Json::object();
    out["title"] = result.title;
    out["blocks"] = std::move(blocks);
    if (result.links) {
        Json links = Json::array();
        for (const auto& l : *result.links) links.push_back(l);
        out["links"] = std::move(links);
    }
    return out;
}

ExtractionResult extraction_from_json(const Json& value) {
    if (!value.is_object()) throw std::runtime_error("notion-public: extraction result is not an object");
    ExtractionResult out;
    out.title = json_string(value, "title");
    const Json& blocks = json_array(value, "blocks");
    if (blocks.size() > kMaxBlocks) throw std::runtime_error("notion-public: too many blocks");
    for (const auto& b : blocks) {
        if (!b.is_object()) continue;
        out.blocks.push_back({json_string(b, "type", "text"), json_string(b, "text")});
    }
    if (const auto links = value.find("links"); links != value.end() && links->is_array()) {
        if (links->size() > kMaxLinks) throw std::runtime_error("notion-public: too many links");
        std::vector<std::string> list;
        for (const auto& l : *links)
            if (l.is_string()) list.push_back(l.get<std::string>());
        out.links = std::move(list);
    }
    return out;
}

Json to_json(const NotionPublicCrawlResult& result) {
    Json pages = Json::array();
    for (const auto& p : result.pages) {
        Json page = Json::object();
        page["url"] = p.url;
        page["title"] = p.title;
        page["depth"] = p.depth;
        page["parentUrl"] = p.parent_url;
        page["markdown"] = p.markdown;
        pages.push_back(std::move(page));
    }
    Json errors = Json::array();
    for (const auto& e : result.errors) {
        Json error = Json::object();
        error["url"] = e.url;
        error["message"] = e.message;
        errors.push_back(std::move(error));
    }
    Json out = Json::object();
    out["rootUrl"] = result.root_url;
    out["pages"] = std::move(pages);
    out["errors"] = std::move(errors);
    out["truncated"] = result.truncated;
    return out;
}

} // namespace canalis::crawl::notion_public
