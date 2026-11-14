// WebsiteSource の移植テスト (test/crawl/website.test.ts と同じ観点 + TS 版の fallbackParser との golden 突き合わせ)。
#include "check.hpp"

#include <memory>

#include "canalis/base/fsutil.hpp"
#include "canalis/crawl/website/source.hpp"

using namespace canalis;
using crawl::website::WebsiteSource;
using crawl::website::WebsiteSourceDeps;

namespace {

const char* kStubHtml = "<!DOCTYPE html>\n<html>\n<head><title>Test Page &amp; More</title></head>\n<body>\n"
                        "<p>Hello <strong>world</strong>!</p>\n<a href=\"/about\">About</a>\n"
                        "<a href=\"https://external.com/page\">External</a>\n<script>alert('skip me')</script>\n</body>\n</html>";

/** 決まった応答を返す取得関数。受けた要求を記録する。 */
struct FakeFetch {
    std::string body;
    int status = 200;
    std::string final_url = "https://example.com/";
    std::shared_ptr<std::vector<http::ClientRequest>> calls = std::make_shared<std::vector<http::ClientRequest>>();
    http::ClientResponse operator()(const http::ClientRequest& req) const {
        calls->push_back(req);
        http::ClientResponse res;
        res.transport_ok = true;
        res.status = status;
        res.body = body;
        res.final_url = final_url;
        return res;
    }
};

WebsiteSourceDeps deps_with(const FakeFetch& fetch) {
    WebsiteSourceDeps deps;
    deps.fetch = fetch;
    deps.now = [] { return std::string("2026-06-11T00:00:00Z"); };
    return deps;
}

class CustomParser final : public crawl::website::HtmlParser {
public:
    int calls = 0;
    crawl::website::ParsedPage parse(const std::string&, const std::string&) override {
        ++calls;
        return {std::string("Custom"), "parsed text", {}};
    }
};

} // namespace

TEST_CASE("タイトルと本文を取り出した RawRecord を返す") {
    FakeFetch fetch{kStubHtml};
    WebsiteSource source(deps_with(fetch));
    const auto records = source.crawl({"https://example.com/", std::nullopt, std::nullopt});
    CHECK_EQ(records.size(), std::size_t{1});
    const auto& r = records[0];
    CHECK_EQ(r.source, std::string("website"));
    CHECK_EQ(r.title.value_or(""), std::string("Test Page & More"));
    CHECK(r.text->find("Hello") != std::string::npos);
    CHECK(r.text->find("<p>") == std::string::npos);
    CHECK(r.text->find("alert") == std::string::npos);
    CHECK_EQ(r.url.value_or(""), std::string("https://example.com/"));
    CHECK_EQ(r.fetched_at, std::string("2026-06-11T00:00:00Z"));
    CHECK_EQ(json_string(r.meta.value(), "originalUrl"), std::string("https://example.com/"));
    CHECK_EQ(json_string(r.raw, "html"), std::string(kStubHtml));
    // 既定の User-Agent・期限・SSRF の既定
    const auto& req = fetch.calls->at(0);
    CHECK_EQ(req.headers.get("User-Agent").value_or(""),
             std::string("Canalis-Crawler/1.0 (LUDIARS; +https://github.com/LUDIARS)"));
    CHECK_EQ(req.timeout.count(), std::int64_t{10'000});
    CHECK(!req.allow_private_network);
}

TEST_CASE("同一ドメインのリンクだけを取り出す") {
    WebsiteSource source(deps_with(FakeFetch{kStubHtml}));
    const auto r = source.crawl({"https://example.com/", std::nullopt, std::nullopt}).at(0);
    const Json& links = json_array(r.meta.value(), "links");
    bool about = false, external = false;
    for (const auto& l : links) {
        if (l.get<std::string>().find("example.com/about") != std::string::npos) about = true;
        if (l.get<std::string>().find("external.com") != std::string::npos) external = true;
    }
    CHECK(about);
    CHECK(!external);
}

TEST_CASE("注入した HtmlParser を使う") {
    auto parser = std::make_shared<CustomParser>();
    auto deps = deps_with(FakeFetch{"<html><body>raw</body></html>"});
    deps.html_parser = parser;
    WebsiteSource source(deps);
    const auto r = source.crawl({"https://example.com/", std::nullopt, std::nullopt}).at(0);
    CHECK_EQ(r.title.value_or(""), std::string("Custom"));
    CHECK_EQ(r.text.value_or(""), std::string("parsed text"));
    CHECK_EQ(parser->calls, 1);
}

TEST_CASE("HTTP エラー・url 未指定・通信の失敗は例外") {
    FakeFetch not_found{"", 404};
    WebsiteSource source(deps_with(not_found));
    try {
        source.crawl({"https://example.com/", std::nullopt, std::nullopt});
        CHECK(false);
    } catch (const std::runtime_error& e) {
        CHECK(std::string(e.what()).find("404") != std::string::npos);
    }
    CHECK_THROWS(source.crawl({"", std::nullopt, std::nullopt}));

    WebsiteSourceDeps broken;
    broken.fetch = [](const http::ClientRequest&) {
        http::ClientResponse res;
        res.error = "connect failed";
        return res;
    };
    CHECK_THROWS(WebsiteSource(broken).crawl({"https://example.com/", std::nullopt, std::nullopt}));
}

TEST_CASE("User-Agent と期限の指定、転送後の URL を使う") {
    FakeFetch fetch{"<title>t</title>"};
    fetch.final_url = "https://example.com/moved/page";
    WebsiteSource source(deps_with(fetch));
    const auto r = source.crawl({"https://example.com/old", std::string("UA/1"), 2500}).at(0);
    CHECK_EQ(fetch.calls->at(0).headers.get("User-Agent").value_or(""), std::string("UA/1"));
    CHECK_EQ(fetch.calls->at(0).timeout.count(), std::int64_t{2500});
    CHECK_EQ(r.url.value_or(""), std::string("https://example.com/moved/page"));
    CHECK_EQ(r.source_id, std::string("example.com/moved/page"));
    CHECK_EQ(json_string(r.meta.value(), "originalUrl"), std::string("https://example.com/old"));
}

TEST_CASE("既定の取得関数は private network へ出ない (C++ 版で足した SSRF 対策)") {
    WebsiteSource source;
    CHECK_THROWS(source.crawl({"http://127.0.0.1:1/", std::nullopt, 1000}));
}

TEST_CASE("fallback_parse と to_source_id が TS 版と同じ結果を返す (golden)") {
    const auto text = fs::read_file(fs::join(test::fixtures_dir(), "crawl/website_fallback_golden.json"));
    CHECK(text.has_value());
    const Json golden = parse_json_or_throw(*text);
    for (const auto& c : golden["cases"]) {
        const std::string name = json_string(c, "name");
        const auto page = crawl::website::fallback_parse(json_string(c, "html"), json_string(c, "url"));
        const Json& expected = c["expected"];
        CHECK_EQ(name + ":" + page.title.value_or("<none>"), name + ":" + json_opt_string(expected, "title").value_or("<none>"));
        CHECK_EQ(name + ":" + page.text, name + ":" + json_string(expected, "text"));
        Json links = Json::array();
        for (const auto& l : page.links) links.push_back(l);
        CHECK_EQ(name + ":" + json_stringify(links), name + ":" + json_stringify(json_array(expected, "links")));
        CHECK_EQ(name + ":" + crawl::website::to_source_id(json_string(c, "url")), name + ":" + json_string(expected, "sourceId"));
    }
}

CI_TEST_MAIN()
