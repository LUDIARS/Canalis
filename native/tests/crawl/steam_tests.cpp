// Steam adapter の移植テスト (test/steam-source.test.ts と同じ観点 + TS の正規表現との golden 突き合わせ)。
// 本物の Steam には繋がない (URL → 本文の対応で応える偽の取得関数を使う)。
#include "check.hpp"

#include <memory>
#include <utility>

#include "canalis/base/fsutil.hpp"
#include "canalis/crawl/steam/source.hpp"

using namespace canalis;
using namespace canalis::crawl::steam;

namespace {

const std::string kSteamId = "76561198000000001";

/** URL に needle を含む最初の経路の本文を 200 で返す。どれにも当たらなければ 404。 */
struct Routes {
    std::vector<std::pair<std::string, std::string>> routes;
    std::shared_ptr<std::vector<http::ClientRequest>> seen = std::make_shared<std::vector<http::ClientRequest>>();
    http::ClientResponse operator()(const http::ClientRequest& req) const {
        seen->push_back(req);
        http::ClientResponse res;
        res.transport_ok = true;
        for (const auto& [needle, body] : routes) {
            if (req.url.find(needle) != std::string::npos) {
                res.status = 200;
                res.body = body;
                return res;
            }
        }
        res.status = 404;
        res.body = "not found";
        return res;
    }
};

void no_sleep(std::chrono::milliseconds) {}

// 名前付きの局所変数を返すと、clang の解析器が std::function の中の番地を「局所の番地を返す」と誤検知するので、直接組み立てて返す。
SteamHttpOptions http_with(crawl::FetchFn fetch) { return SteamHttpOptions{std::move(fetch), std::nullopt, no_sleep}; }

Json review(const std::string& id, const std::string& steam_id) {
    return Json{{"recommendationid", id},
                {"author", Json{{"steamid", steam_id}, {"playtime_forever", 120}}},
                {"review", "good game"},
                {"language", "japanese"},
                {"timestamp_created", 1754956800},
                {"voted_up", true},
                {"votes_up", 3}};
}

std::string page(std::vector<Json> reviews, const std::string& cursor) {
    Json arr = Json::array();
    for (auto& r : reviews) arr.push_back(r);
    return json_stringify(Json{{"success", 1}, {"reviews", arr}, {"cursor", cursor}});
}

http::ClientResponse redirect_to(const std::string& location) {
    http::ClientResponse res;
    res.transport_ok = true;
    res.status = 302;
    res.headers.set("location", location);
    return res;
}

template <class Fn>
std::string error_of(Fn&& fn) {
    try {
        fn();
    } catch (const std::exception& e) {
        return e.what();
    }
    return "<no error>";
}

} // namespace

TEST_CASE("fetch_app_reviews: cursor を進めて全ページを集め、増分取得で打ち切る") {
    Routes r{{{"cursor=%2A", page({review("1", "a"), review("2", "b")}, "c2")},
              {"cursor=c2", page({review("3", "c")}, "c3")},
              {"cursor=c3", page({}, "c3")}}};
    FetchAppReviewsOptions o;
    o.http = http_with(r);
    o.app_id = 42;
    const auto all = fetch_app_reviews(o);
    CHECK_EQ(all.size(), std::size_t{3});
    o.stop_at_recommendation_id = "2";
    CHECK_EQ(fetch_app_reviews(o).size(), std::size_t{1});
    o.stop_at_recommendation_id.reset();
    o.max_reviews = 2;
    CHECK_EQ(fetch_app_reviews(o).size(), std::size_t{2});
}

TEST_CASE("fetch_app_reviews: success!=1 は失敗、appId / maxReviews / filter の不正はリクエスト前に失敗") {
    FetchAppReviewsOptions o;
    o.http = http_with(Routes{{{"appreviews", "{\"success\":2}"}}});
    o.app_id = 42;
    CHECK(error_of([&] { fetch_app_reviews(o); }).find("success=2 appId=42") != std::string::npos);
    auto calls = std::make_shared<int>(0);
    o.http = http_with([calls](const http::ClientRequest&) {
        ++*calls;
        return http::ClientResponse{};
    });
    o.app_id = 0;
    CHECK(error_of([&] { fetch_app_reviews(o); }).find("appId") != std::string::npos);
    o.app_id = 42;
    o.max_reviews = -1;
    CHECK(error_of([&] { fetch_app_reviews(o); }).find("maxReviews") != std::string::npos);
    o.max_reviews.reset();
    o.filter = "bogus";
    CHECK(error_of([&] { fetch_app_reviews(o); }).find("filter") != std::string::npos);
    CHECK_EQ(*calls, 0);
}

TEST_CASE("fetch_app_review_summary: num_per_page=0 で query_summary を返す") {
    Routes r{{{"num_per_page=0", "{\"success\":1,\"query_summary\":{\"total_reviews\":321,\"total_positive\":300}}"}}};
    CHECK_EQ(json_int(fetch_app_review_summary(http_with(r), 42), "total_reviews"), std::int64_t{321});
    CHECK_EQ(build_reviews_url(42, "recent", {"japanese", "english"}, "AoJ4*x", 100),
             std::string("https://store.steampowered.com/appreviews/42?json=1&num_per_page=100&filter=recent&language=japanese%2Cenglish"
                         "&review_type=all&purchase_type=all&cursor=AoJ4%2Ax"));
}

TEST_CASE("parse_search_results_html と parse_user_reviews_html が TS と同じ結果 (golden)") {
    const auto text = fs::read_file(fs::join(test::fixtures_dir(), "crawl/steam_parse_golden.json"));
    CHECK(text.has_value());
    const Json g = parse_json_or_throw(*text);
    for (const auto& c : g["search"]) {
        Json got = Json::array();
        for (const auto& e : parse_search_results_html(json_string(c, "html"))) got.push_back(to_json(e));
        CHECK_EQ(json_string(c, "name") + ":" + json_stringify(got), json_string(c, "name") + ":" + json_stringify(c["expected"]));
    }
    for (const auto& c : g["user"]) {
        Json got = Json::array();
        for (const auto& e : parse_user_reviews_html(json_string(c, "html"), json_string(g, "steamId"))) got.push_back(to_json(e));
        CHECK_EQ(json_string(c, "name") + ":" + json_stringify(got), json_string(c, "name") + ":" + json_stringify(c["expected"]));
    }
}

TEST_CASE("fetch_new_releases: total_count までページを送り、maxApps の不正はリクエスト前に失敗") {
    Routes r{{{"start=0", "{\"success\":1,\"results_html\":\"<a data-ds-appid=\\\"111\\\"><span class=\\\"title\\\">A</span></a>\",\"total_count\":1}"}}};
    FetchNewReleasesOptions o;
    o.http = http_with(r);
    o.max_apps = 10;
    const auto got = fetch_new_releases(o);
    CHECK_EQ(got.size(), std::size_t{1});
    CHECK_EQ(got[0].app_id, std::int64_t{111});
    CHECK(r.seen->at(0).url.find("sort_by=Released_DESC") != std::string::npos);
    CHECK(r.seen->at(0).url.find("cc=jp&l=japanese") != std::string::npos);
    o.max_apps = -1;
    CHECK(error_of([&] { fetch_new_releases(o); }).find("maxApps") != std::string::npos);
}

TEST_CASE("fetch_user_reviews: 尽きるまでページを送り、非公開は空、不正な id・maxPages は失敗") {
    const auto g = parse_json_or_throw(*fs::read_file(fs::join(test::fixtures_dir(), "crawl/steam_parse_golden.json")));
    const std::string user_html = json_string(g["user"][0], "html");
    FetchUserReviewsOptions o;
    o.http = http_with(Routes{{{"p=1", user_html}, {"p=2", "<html>no more</html>"}}});
    o.steam_id = kSteamId;
    const auto got = fetch_user_reviews(o);
    CHECK_EQ(got.size(), std::size_t{2});
    CHECK_EQ(got[1].app_id, std::int64_t{222});

    o.http = http_with(Routes{{{"p=1", "<div class=\"profile_private_info\">private</div>"}}});
    CHECK(fetch_user_reviews(o).empty());

    o.steam_id = "untrusted-input";
    const std::string err = error_of([&] { fetch_user_reviews(o); });
    CHECK(err.find("SteamID64") != std::string::npos);
    CHECK(err.find("untrusted-input") == std::string::npos);
    o.steam_id = kSteamId;
    o.max_pages = -1;
    CHECK(error_of([&] { fetch_user_reviews(o); }).find("maxPages") != std::string::npos);
}

TEST_CASE("Steam HTTP: 自動の転送をさせず、エラーに個人の識別子を含む URL を出さない") {
    auto seen = std::make_shared<std::vector<http::ClientRequest>>();
    SteamHttp http(http_with([seen](const http::ClientRequest& req) {
        seen->push_back(req);
        http::ClientResponse res;
        res.transport_ok = true;
        res.status = 404;
        res.body = "no";
        return res;
    }));
    const std::string err = error_of([&] { http.get_text("https://steamcommunity.com/profiles/" + kSteamId + "/recommended/42/"); });
    CHECK_EQ(err, std::string("steam fetch HTTP 404"));
    CHECK_EQ(seen->at(0).max_redirects, 0);
    CHECK_EQ(seen->at(0).headers.get("User-Agent").value_or(""), std::string(kSteamUserAgent));
}

TEST_CASE("Steam HTTP: 同じホストへの転送はたどり、別ホスト・http・繰り返しは拒否する") {
    auto seen = std::make_shared<std::vector<std::string>>();
    SteamHttp same(http_with([seen](const http::ClientRequest& req) {
        seen->push_back(req.url);
        if (req.url.find("/profiles/") != std::string::npos) return redirect_to("/id/someone/recommended?p=1");
        http::ClientResponse ok;
        ok.transport_ok = true;
        ok.status = 200;
        ok.body = "ok";
        return ok;
    }));
    CHECK_EQ(same.get_text("https://steamcommunity.com/profiles/" + kSteamId + "/recommended/?p=1"), std::string("ok"));
    CHECK_EQ(seen->at(1), std::string("https://steamcommunity.com/id/someone/recommended?p=1"));

    const std::string start = "https://steamcommunity.com/profiles/" + kSteamId + "/recommended/";
    auto to = [](std::string location) {
        return http_with([location](const http::ClientRequest&) { return redirect_to(location); });
    };
    const std::string evil = error_of([&] { SteamHttp(to("https://evil.example/x")).get_text(start); });
    CHECK(evil.find("another host") != std::string::npos);
    CHECK(evil.find("evil") == std::string::npos && evil.find("profiles") == std::string::npos);
    CHECK(error_of([&] { SteamHttp(to("http://steamcommunity.com/x")).get_text(start); }).find("another host") != std::string::npos);
    CHECK(error_of([&] { SteamHttp(to("/loop")).get_text(start); }).find("too many redirects") != std::string::npos);
    SteamHttpOptions bad;
    bad.polite_delay_ms = -1;
    CHECK(error_of([&] { SteamHttp{bad}; }).find("politeDelayMs") != std::string::npos);
}

TEST_CASE("Source 契約: レビュー・検索エントリ・ユーザのレビューを RawRecord にする") {
    const auto now = [] { return std::string("2026-08-12T00:00:00.000Z"); };
    FetchAppReviewsOptions ro;
    ro.http = http_with(Routes{{{"cursor=%2A", page({review("9", kSteamId)}, "end")}, {"cursor=end", page({}, "end")}}});
    ro.app_id = 42;
    const auto reviews = SteamAppReviewsSource({now}).crawl(ro);
    CHECK_EQ(reviews.size(), std::size_t{1});
    CHECK_EQ(json_stringify(core::to_json(reviews[0])),
             "{\"source\":\"steam-app-reviews\",\"sourceId\":\"steam-review:9\",\"fetchedAt\":\"2026-08-12T00:00:00.000Z\","
             "\"url\":\"https://steamcommunity.com/profiles/" + kSteamId + "/recommended/42/\",\"text\":\"good game\","
             "\"raw\":" + json_stringify(review("9", kSteamId)) + ","
             "\"meta\":{\"appId\":42,\"steamId\":\"" + kSteamId + "\",\"votedUp\":true,\"votesUp\":3,\"language\":\"japanese\","
             "\"timestampCreated\":1754956800,\"playtimeForever\":120}}");

    FetchNewReleasesOptions no;
    no.http = http_with(Routes{{{"start=0", "{\"success\":1,\"results_html\":\"<a data-ds-appid=\\\"5\\\"><span class=\\\"title\\\">T</span></a>\",\"total_count\":1}"}}});
    const auto releases = SteamNewReleasesSource({now}).crawl(no);
    CHECK_EQ(json_stringify(core::to_json(releases.at(0))),
             std::string("{\"source\":\"steam-new-releases\",\"sourceId\":\"steam-app:5\",\"fetchedAt\":\"2026-08-12T00:00:00.000Z\","
                         "\"url\":\"https://store.steampowered.com/app/5/\",\"title\":\"T\","
                         "\"raw\":{\"appId\":5,\"title\":\"T\",\"url\":\"https://store.steampowered.com/app/5/\"},\"meta\":{\"appId\":5}}"));

    const auto g = parse_json_or_throw(*fs::read_file(fs::join(test::fixtures_dir(), "crawl/steam_parse_golden.json")));
    FetchUserReviewsOptions uo;
    uo.http = http_with(Routes{{{"p=1", json_string(g["user"][1], "html")}}});
    uo.steam_id = kSteamId;
    uo.max_pages = 1;
    const auto users = SteamUserReviewsSource({now}).crawl(uo);
    CHECK_EQ(users.at(0).source_id, "steam-user-review:" + kSteamId + ":333");
    CHECK_EQ(json_stringify(users.at(0).meta.value()),
             "{\"appId\":333,\"steamId\":\"" + kSteamId + "\",\"recommended\":true,\"hoursText\":\"12.0 hrs on record\",\"postedText\":\"Posted 3 October.\"}");

    FetchAppReviewsOptions missing;
    CHECK_THROWS(SteamAppReviewsSource({now}).crawl(missing));
    CHECK_THROWS(SteamUserReviewsSource({now}).crawl(FetchUserReviewsOptions{}));
}

CI_TEST_MAIN()
