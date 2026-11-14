// Notion 公式 API 版の移植テスト (test/notion-source.test.ts・test/notion-page-crawl.test.ts と同じ観点 + client と blocks)。
// 本物の Notion には繋がない (偽の NotionApi・偽の取得関数を使う)。
#include "check.hpp"

#include <map>
#include <memory>

#include "canalis/crawl/notion/blocks.hpp"
#include "canalis/crawl/notion/client.hpp"
#include "canalis/crawl/notion/crawl.hpp"
#include "canalis/crawl/notion/url.hpp"

using namespace canalis;
using namespace canalis::crawl::notion;

namespace {

Json para(const std::string& id, const std::string& text) {
    return Json{{"id", id}, {"type", "paragraph"}, {"paragraph", Json{{"rich_text", Json::array({Json{{"plain_text", text}}})}}}};
}
Json child_page(const std::string& id, const std::string& title) {
    return Json{{"id", id}, {"type", "child_page"}, {"child_page", Json{{"title", title}}}};
}
Paged paged(std::vector<Json> results) { return {std::move(results), std::nullopt, false}; }

/** root → c1 → c2 → c3 の 3 段の子ページを持つ偽物。 */
class ChainNotionApi : public NotionApi {
public:
    bool fail_retrieve = false;
    Paged query_database(const std::string&, const std::optional<std::string>&) override { return paged({}); }
    Paged get_block_children(const std::string& id, const std::optional<std::string>&) override {
        const auto it = tree_.find(id);
        return paged(it == tree_.end() ? std::vector<Json>{} : it->second);
    }
    Json retrieve_page(const std::string& id) override {
        if (fail_retrieve) throw std::runtime_error("404");
        return Json{{"id", id},
                    {"url", "https://www.notion.so/" + id},
                    {"properties", Json{{"title", Json{{"type", "title"}, {"title", Json::array({Json{{"plain_text", "T-" + id}}})}}}}}};
    }

private:
    std::map<std::string, std::vector<Json>> tree_{
        {"root", {para("p0", "root body"), child_page("c1", "C1")}},
        {"c1", {para("p1", "c1 body"), child_page("c2", "C2")}},
        {"c2", {para("p2", "c2 body"), child_page("c3", "C3")}},
        {"c3", {para("p3", "c3 body")}},
    };
};

/** 1 row + 1 paragraph だけ返す最小の偽物 (notion-source.test.ts と同じ)。 */
class FakeDbApi : public NotionApi {
public:
    Paged query_database(const std::string&, const std::optional<std::string>&) override {
        return paged({Json{{"id", "page-1"},
                           {"url", "https://notion.so/page-1"},
                           {"properties", Json{{"Name", Json{{"type", "title"}, {"title", Json::array({Json{{"plain_text", "Acme Inc"}}})}}},
                                               {"Tag", Json{{"type", "multi_select"},
                                                            {"multi_select", Json::array({Json{{"name", "saas"}}, Json{{"name", "b2b"}}})}}}}}}});
    }
    Paged get_block_children(const std::string& id, const std::optional<std::string>&) override {
        if (id == "page-1") return paged({para("b1", "hello world")});
        return paged({});
    }
    Json retrieve_page(const std::string& id) override { return Json{{"id", id}}; }
};

std::string ids(const PageCrawlResult& r) {
    std::string out;
    for (const auto& p : r.pages) out += p.id + "/" + std::to_string(p.depth) + "/" + p.kind + " ";
    return out;
}

} // namespace

TEST_CASE("crawl_page: 起点を depth 0 / kind=page で返し、max_depth まで子ページを辿る") {
    ChainNotionApi api;
    CrawlOptions o;
    o.max_depth = 2;
    const auto r = crawl_page(api, "root", o);
    CHECK_EQ(r.page_id, std::string("root"));
    CHECK_EQ(ids(r), std::string("root/0/page c1/1/child_page c2/2/child_page "));
    CHECK_EQ(r.pages[0].title, std::string("T-root"));
    CHECK(r.pages[0].markdown.find("root body") != std::string::npos);
    CHECK(r.errors.empty());
    CHECK(!r.truncated);
}

TEST_CASE("crawl_page: max_depth=0 は起点のみ、max_pages で打ち切ると truncated") {
    ChainNotionApi api;
    CrawlOptions zero;
    zero.max_depth = 0;
    CHECK_EQ(ids(crawl_page(api, "root", zero)), std::string("root/0/page "));
    CrawlOptions limited;
    limited.max_depth = 5;
    limited.max_pages = 2;
    const auto r = crawl_page(api, "root", limited);
    CHECK_EQ(r.pages.size(), std::size_t{2});
    CHECK(r.truncated);
}

TEST_CASE("crawl_page: retrievePage の失敗は errors に積んで落ちない") {
    ChainNotionApi api;
    api.fail_retrieve = true;
    const auto r = crawl_page(api, "root");
    CHECK(r.pages.empty());
    CHECK_EQ(r.errors.at(0).id, std::string("root"));
    CHECK_EQ(r.errors.at(0).stage, std::string("retrievePage"));
}

TEST_CASE("NotionSource は crawl 結果を RawRecord に変換する") {
    FakeDbApi api;
    NotionSource source({&api, [] { return std::string("2026-06-08T00:00:00.000Z"); }});
    const auto records = source.crawl({"db-1", std::nullopt, {}});
    CHECK_EQ(records.size(), std::size_t{1});
    const auto& r = records[0];
    CHECK_EQ(r.source, std::string("notion"));
    CHECK_EQ(r.source_id, std::string("page-1"));
    CHECK_EQ(r.fetched_at, std::string("2026-06-08T00:00:00.000Z"));
    CHECK_EQ(r.title.value_or(""), std::string("Acme Inc"));
    CHECK(r.text->find("hello world") != std::string::npos);
    CHECK_EQ(json_string(json_object(r.meta.value(), "properties"), "Tag"), std::string("saas, b2b"));
    CHECK_EQ(json_string(r.meta.value(), "kind"), std::string("database_row"));
    CHECK_EQ(json_string(r.meta.value(), "databaseId"), std::string("db-1"));
    CHECK(r.raw.is_object());
    CHECK_EQ(json_stringify(r.raw),
             std::string("{\"id\":\"page-1\",\"url\":\"https://notion.so/page-1\",\"title\":\"Acme Inc\",\"kind\":\"database_row\","
                         "\"parentId\":\"db-1\",\"depth\":0,\"markdown\":\"hello world\","
                         "\"properties\":{\"Name\":\"Acme Inc\",\"Tag\":\"saas, b2b\"}}"));
}

TEST_CASE("NotionSource: databaseId が空なら例外") {
    FakeDbApi api;
    NotionSource source({&api, {}});
    try {
        source.crawl({"", std::nullopt, {}});
        CHECK(false);
    } catch (const std::runtime_error& e) {
        CHECK(std::string(e.what()).find("databaseId") != std::string::npos);
    }
}

TEST_CASE("Notion URL: 末尾の 32hex / UUID・?p= の優先・Notion 以外") {
    const std::string hex = "0123456789abcdef0123456789abcdef";
    const std::string uuid = "01234567-89ab-cdef-0123-456789abcdef";
    CHECK_EQ(parse_notion_page_id("https://www.notion.so/ws/Some-Title-" + hex).value_or(""), uuid);
    CHECK_EQ(parse_notion_page_id("https://acme.notion.site/Page-" + hex + "?pvs=4").value_or(""), uuid);
    CHECK_EQ(parse_notion_page_id("https://www.notion.so/ffffffffffffffffffffffffffffffff?v=1&p=" + hex).value_or(""), uuid);
    CHECK_EQ(parse_notion_page_id("https://www.notion.so/" + uuid).value_or(""), uuid);
    CHECK_EQ(parse_notion_page_id("https://app.notion.com/p/2c439cbfbab98011b008e3262ce3a98b?pvs=204").value_or(""),
             std::string("2c439cbf-bab9-8011-b008-e3262ce3a98b"));
    CHECK(!parse_notion_page_id("https://example.com/" + hex).has_value());
    CHECK(!parse_notion_page_id("https://www.notion.so/ws").has_value());
    CHECK(!is_notion_url("https://evilnotion.so/x"));
    CHECK(is_notion_url("https://app.notion.com/p/x"));
    CHECK(!is_notion_url("ftp://www.notion.so/x"));
}

TEST_CASE("本文中の Notion URL を重複なしで列挙し、末尾の句読点を外す") {
    const std::string hex = "0123456789abcdef0123456789abcdef";
    const std::string text = "仕様は https://www.notion.so/a-" + hex + "。 参考 https://example.com/x と https://www.notion.so/a-" + hex;
    const auto urls = find_notion_urls(text);
    CHECK_EQ(urls.size(), std::size_t{1});
    CHECK_EQ(urls.at(0), "https://www.notion.so/a-" + hex);
    CHECK_EQ(find_notion_urls("(https://x.notion.site/P-" + hex + "), https://www.notion.so/b!?").size(), std::size_t{2});
}

TEST_CASE("blocks: 主なブロックと properties を TS と同じ文字列にする") {
    CHECK_EQ(block_to_markdown(Json{{"type", "heading_2"}, {"heading_2", Json{{"rich_text", Json::array({Json{{"plain_text", "H"}}})}}}}),
             std::string("## H"));
    CHECK_EQ(block_to_markdown(Json{{"type", "to_do"}, {"to_do", Json{{"checked", true}, {"rich_text", Json::array()}}}}),
             std::string("- [x] "));
    CHECK_EQ(block_to_markdown(Json{{"type", "code"}, {"code", Json{{"language", "js"}, {"rich_text", Json::array({Json{{"plain_text", "x"}}})}}}}),
             std::string("```js\nx\n```"));
    CHECK_EQ(block_to_markdown(Json{{"type", "child_database"}, {"child_database", Json{{"title", "DB"}}}}), std::string("## DB (database)"));
    CHECK_EQ(block_to_markdown(Json{{"type", "bookmark"}, {"bookmark", Json{{"url", "https://e.x"}}}}), std::string("[https://e.x](https://e.x)"));
    CHECK_EQ(block_to_markdown(Json{{"type", "embed"}, {"embed", Json::object()}}), std::string(""));
    CHECK_EQ(blocks_to_markdown({{Json{{"type", "divider"}}, 0}, {Json{{"type", "quote"}, {"quote", Json{{"rich_text", Json::array({Json{{"plain_text", "q"}}})}}}}, 2}}),
             std::string("---\n    > q"));

    CHECK_EQ(prop_to_string(Json{{"type", "number"}, {"number", 3.5}}), std::string("3.5"));
    CHECK_EQ(prop_to_string(Json{{"type", "number"}, {"number", nullptr}}), std::string(""));
    CHECK_EQ(prop_to_string(Json{{"type", "date"}, {"date", Json{{"start", "2026-01-01"}, {"end", "2026-01-02"}}}}),
             std::string("2026-01-01 ~ 2026-01-02"));
    CHECK_EQ(prop_to_string(Json{{"type", "checkbox"}, {"checkbox", false}}), std::string("false"));
    CHECK_EQ(prop_to_string(Json{{"type", "people"}, {"people", Json::array({1, 2})}}), std::string("2 people"));
    CHECK_EQ(prop_to_string(Json{{"type", "formula"}, {"formula", Json{{"string", nullptr}, {"number", 42}}}}), std::string("42"));
    CHECK_EQ(prop_to_string(Json{{"type", "select"}, {"select", nullptr}}), std::string(""));
    CHECK_EQ(prop_to_string(Json{{"type", "unknown"}}), std::string(""));
}

TEST_CASE("client: 要求のヘッダ・本文・パス、429 の再試行 (Retry-After)、失敗の文言") {
    auto calls = std::make_shared<std::vector<http::ClientRequest>>();
    auto sleeps = std::make_shared<std::vector<std::int64_t>>();
    auto statuses = std::make_shared<std::vector<int>>(std::vector<int>{429, 503, 200});
    NotionClientConfig cfg;
    cfg.token = "ntn_fake_token";
    cfg.min_interval_ms = 0;
    cfg.sleep = [sleeps](std::chrono::milliseconds d) { sleeps->push_back(d.count()); };
    cfg.fetch = [calls, statuses](const http::ClientRequest& req) {
        calls->push_back(req);
        http::ClientResponse res;
        res.transport_ok = true;
        res.status = statuses->empty() ? 200 : statuses->front();
        if (!statuses->empty()) statuses->erase(statuses->begin());
        if (res.status == 429) res.headers.set("Retry-After", "2");
        res.body = res.status == 200 ? "{\"results\":[{\"id\":\"r1\"}],\"next_cursor\":\"c2\",\"has_more\":true}" : "busy";
        return res;
    };
    NotionApiClient client(cfg);
    const auto page = client.query_database("db/1", std::string("c1"));
    CHECK_EQ(page.results.size(), std::size_t{1});
    CHECK_EQ(page.next_cursor.value_or(""), std::string("c2"));
    CHECK(page.has_more);
    CHECK_EQ(calls->size(), std::size_t{3});
    const auto& req = calls->at(0);
    CHECK_EQ(req.method, std::string("POST"));
    CHECK_EQ(req.url, std::string("https://api.notion.com/v1/databases/db%2F1/query"));
    CHECK_EQ(req.headers.get("authorization").value_or(""), std::string("Bearer ntn_fake_token"));
    CHECK_EQ(req.headers.get("notion-version").value_or(""), std::string("2022-06-28"));
    CHECK_EQ(req.body, std::string("{\"page_size\":100,\"start_cursor\":\"c1\"}"));
    CHECK_EQ(req.max_redirects, 0);
    CHECK_EQ(sleeps->size(), std::size_t{2});
    CHECK_EQ(sleeps->at(0), std::int64_t{2000});  // Retry-After: 2
    CHECK_EQ(sleeps->at(1), std::int64_t{2000});  // 2 回目は指数 (1000 * 2^1)

    client.get_block_children("b1", std::nullopt);
    CHECK_EQ(calls->back().url, std::string("https://api.notion.com/v1/blocks/b1/children?page_size=100"));
    client.retrieve_page("p1");
    CHECK_EQ(calls->back().method, std::string("GET"));

    NotionClientConfig bad = cfg;
    bad.max_retries = 0;
    bad.fetch = [](const http::ClientRequest&) {
        http::ClientResponse res;
        res.transport_ok = true;
        res.status = 401;
        res.body = "unauthorized";
        return res;
    };
    try {
        NotionApiClient(bad).retrieve_page("p1");
        CHECK(false);
    } catch (const std::runtime_error& e) {
        const std::string what = e.what();
        CHECK(what.find("notion GET /pages/p1 failed: 401 unauthorized") != std::string::npos);
        CHECK(what.find("ntn_fake_token") == std::string::npos);  // token をエラー文に出さない
    }
    NotionClientConfig empty = cfg;
    empty.token.clear();
    CHECK_THROWS(NotionApiClient(empty));
}

CI_TEST_MAIN()
