// notion-public の移植テスト (test/notion-public-source.test.ts と test/notion-public-crawl.test.ts と同じ観点)。
// ブラウザは起動しない (偽の PageFetcher を使う)。
#include "check.hpp"

#include <map>

#include "canalis/crawl/notion_public/extract.hpp"
#include "canalis/crawl/notion_public/fetcher.hpp"
#include "canalis/crawl/notion_public/source.hpp"

using namespace canalis;
using namespace canalis::crawl::notion_public;

namespace {

const std::string kRoot = "https://acme.notion.site/Root-00000000000000000000000000000000";
const std::string kC1 = "https://acme.notion.site/C1-11111111111111111111111111111111";
const std::string kC2 = "https://acme.notion.site/C2-22222222222222222222222222222222";
const std::string kC3 = "https://acme.notion.site/C3-33333333333333333333333333333333";

/** ROOT → C1 → C2 → C3 の連鎖。C1 は ROOT へ戻るリンクも持つ (循環)。 */
class ChainFetcher final : public PageFetcher {
public:
    std::vector<std::string> fetched;
    int closed = 0;
    FetchedPage fetch(const std::string& url, const FetchPageOptions&) override {
        fetched.push_back(url);
        const auto it = links_.find(url);
        if (it == links_.end()) throw std::runtime_error("404 " + url);
        const std::string last = url.substr(url.rfind('/') + 1);
        const std::string title = last.substr(0, last.find('-'));
        FetchedPage page;
        page.url = url;
        page.title = title;
        page.markdown = title + " body";
        page.raw = {title, {{"text", title + " body"}}, it->second};
        return page;
    }
    void close() override { ++closed; }

private:
    std::map<std::string, std::vector<std::string>> links_{
        {kRoot, {kC1}}, {kC1, {kRoot, kC2}}, {kC2, {kC3}}, {kC3, {}}};
};

class FixedFetcher final : public PageFetcher {
public:
    FetchPageOptions seen;
    FetchedPage fetch(const std::string& url, const FetchPageOptions& options) override {
        seen = options;
        FetchedPage page;
        page.url = url;
        page.title = "Test Page";
        page.markdown = "# Test Page\n\nSome content here.";
        page.raw = {"Test Page", {{"h1", "Test Page"}, {"text", "Some content here."}}, std::nullopt};
        return page;
    }
};

std::vector<std::string> titles(const NotionPublicCrawlResult& r) {
    std::vector<std::string> out;
    for (const auto& p : r.pages) out.push_back(p.title);
    return out;
}

} // namespace

TEST_CASE("NotionPublicSource は TS と同じ形の RawRecord を返す") {
    FixedFetcher fetcher;
    NotionPublicSource source({&fetcher, [] { return std::string("2026-06-11T00:00:00.000Z"); }});
    const auto records = source.crawl({"https://notion.so/test-abc123", {}});
    CHECK_EQ(records.size(), std::size_t{1});
    const Json j = core::to_json(records[0]);
    CHECK_EQ(json_stringify(j),
             std::string("{\"source\":\"notion-public\",\"sourceId\":\"https://notion.so/test-abc123\","
                         "\"fetchedAt\":\"2026-06-11T00:00:00.000Z\",\"url\":\"https://notion.so/test-abc123\","
                         "\"title\":\"Test Page\",\"text\":\"# Test Page\\n\\nSome content here.\","
                         "\"raw\":{\"title\":\"Test Page\",\"blocks\":[{\"type\":\"h1\",\"text\":\"Test Page\"},"
                         "{\"type\":\"text\",\"text\":\"Some content here.\"}]},"
                         "\"meta\":{\"url\":\"https://notion.so/test-abc123\",\"blockCount\":2}}"));
    // 既定のオプション (timeout 30000 / scrollDelay 800 / maxScrolls 15)
    CHECK_EQ(fetcher.seen.timeout, std::int64_t{30'000});
    CHECK_EQ(fetcher.seen.scroll_delay, std::int64_t{800});
    CHECK_EQ(fetcher.seen.max_scrolls, 15);
}

TEST_CASE("url が空なら例外") {
    FixedFetcher fetcher;
    NotionPublicSource source({&fetcher, {}});
    try {
        source.crawl({"", {}});
        CHECK(false);
    } catch (const std::runtime_error& e) {
        CHECK(std::string(e.what()).find("url is required") != std::string::npos);
    }
}

TEST_CASE("blocks_to_markdown は TS と同じ書式") {
    CHECK_EQ(blocks_to_markdown({{"h1", "Title"}}), std::string("# Title"));
    CHECK_EQ(blocks_to_markdown({{"h2", "Sub"}}), std::string("## Sub"));
    CHECK_EQ(blocks_to_markdown({{"h3", "Sub2"}}), std::string("### Sub2"));
    CHECK_EQ(blocks_to_markdown({{"li", "Item"}}), std::string("- Item"));
    CHECK_EQ(blocks_to_markdown({{"oli", "Item"}}), std::string("1. Item"));
    CHECK_EQ(blocks_to_markdown({{"quote", "cited"}}), std::string("> cited"));
    CHECK_EQ(blocks_to_markdown({{"quote", "a\nb"}}), std::string("> a\n> b"));
    CHECK_EQ(blocks_to_markdown({{"code", "x = 1"}}), std::string("```\nx = 1\n```"));
    CHECK_EQ(blocks_to_markdown({{"callout", "note"}}), std::string("> \xF0\x9F\x92\xA1 note"));
    CHECK_EQ(blocks_to_markdown({{"divider", ""}}), std::string("---"));
    CHECK_EQ(blocks_to_markdown({{"h1", "Title"}, {"text", "Body text."}}), std::string("# Title\n\nBody text."));
    CHECK_EQ(blocks_to_markdown({{"text", "  "}, {"text", "x"}}), std::string("x"));
}

TEST_CASE("crawl_public_pages: 既定 (maxDepth 0) は起点ページのみ") {
    ChainFetcher f;
    const auto r = crawl_public_pages(kRoot, {}, &f);
    CHECK_EQ(titles(r), std::vector<std::string>{"Root"});
}

TEST_CASE("crawl_public_pages: maxDepth まで幅優先で辿り、循環リンクは 1 回だけ取る") {
    ChainFetcher f;
    NotionPublicPageCrawlOptions o;
    o.max_depth = 2;
    const auto r = crawl_public_pages(kRoot, o, &f);
    CHECK_EQ(r.pages.size(), std::size_t{3});
    CHECK_EQ(r.pages[0].title + "/" + std::to_string(r.pages[0].depth) + "/" + r.pages[0].parent_url, std::string("Root/0/"));
    CHECK_EQ(r.pages[1].title + "/" + std::to_string(r.pages[1].depth) + "/" + r.pages[1].parent_url, "C1/1/" + kRoot);
    CHECK_EQ(r.pages[2].title + "/" + std::to_string(r.pages[2].depth) + "/" + r.pages[2].parent_url, "C2/2/" + kC1);
    CHECK_EQ(f.fetched, (std::vector<std::string>{kRoot, kC1, kC2}));
    CHECK(!r.truncated);
}

TEST_CASE("crawl_public_pages: maxPages で打ち切ると truncated=true") {
    ChainFetcher f;
    NotionPublicPageCrawlOptions o;
    o.max_depth = 5;
    o.max_pages = 2;
    const auto r = crawl_public_pages(kRoot, o, &f);
    CHECK_EQ(r.pages.size(), std::size_t{2});
    CHECK(r.truncated);
}

TEST_CASE("crawl_public_pages: 取得失敗は errors に積んで続け、注入した fetcher は閉じない") {
    ChainFetcher f;
    const auto r = crawl_public_pages("https://acme.notion.site/Missing-44444444444444444444444444444444", {}, &f);
    CHECK(r.pages.empty());
    CHECK(r.errors.at(0).message.find("404") != std::string::npos);
    CHECK_EQ(f.closed, 0);
    const Json j = to_json(r);
    CHECK_EQ(json_stringify(j["errors"][0]["url"]), std::string("\"https://acme.notion.site/Missing-44444444444444444444444444444444\""));
    CHECK(j.contains("rootUrl") && j.contains("truncated"));
}

TEST_CASE("is_bot_challenge_title: Cloudflare の確認画面を判定する") {
    CHECK(is_bot_challenge_title("Just a moment..."));
    CHECK(is_bot_challenge_title("  attention required! | Cloudflare"));
    CHECK(is_bot_challenge_title("しばらくお待ちください..."));
    CHECK(!is_bot_challenge_title("コンビニドミナント"));
}

TEST_CASE("is_allowed_page_url: 公開ホストの http/https だけを取る") {
    CHECK(is_allowed_page_url("https://acme.notion.site/Page-0123"));
    CHECK(is_allowed_page_url("https://www.notion.so/x"));
    CHECK(is_allowed_page_url("http://8.8.8.8/"));
    CHECK(!is_allowed_page_url("http://localhost:3000/"));
    CHECK(!is_allowed_page_url("http://127.0.0.1/"));
    CHECK(!is_allowed_page_url("http://192.168.1.10/"));
    CHECK(!is_allowed_page_url("http://[::1]/"));
    CHECK(!is_allowed_page_url("file:///etc/passwd"));
    CHECK(!is_allowed_page_url("javascript:alert(1)"));
}

TEST_CASE("extraction_from_json は形の違う値を弾き、links の有無を保つ") {
    const auto e = extraction_from_json(parse_json_or_throw(
        "{\"title\":\"T\",\"blocks\":[{\"type\":\"h1\",\"text\":\"a\"},7],\"links\":[\"https://x/1\",3]}"));
    CHECK_EQ(e.blocks.size(), std::size_t{1});
    CHECK_EQ(e.links->size(), std::size_t{1});
    CHECK(!extraction_from_json(parse_json_or_throw("{\"title\":\"T\",\"blocks\":[]}")).links.has_value());
    CHECK_THROWS(extraction_from_json(Json("x")));
}

TEST_CASE("抽出の式は extract.ts の関数を即時に呼ぶ形") {
    const std::string_view js = extract_page_content_expression();
    CHECK(js.substr(0, 37) == "(function extractPageContent() {\n  co");
    CHECK(js.substr(js.size() - 4) == "})()");
    CHECK(js.find(".notion-page-content") != std::string_view::npos);
}

CI_TEST_MAIN()
