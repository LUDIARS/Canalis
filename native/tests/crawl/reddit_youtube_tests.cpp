// Reddit・YouTube の移植テスト (test/crawl/reddit.test.ts・test/crawl/youtube.test.ts と同じ観点)。偽の取得関数を使う。
#include "check.hpp"

#include <memory>

#include "canalis/crawl/reddit/source.hpp"
#include "canalis/crawl/youtube/source.hpp"

using namespace canalis;

namespace {

struct Fake {
    std::vector<std::pair<std::string, std::string>> routes;
    int status = 200;
    std::shared_ptr<std::vector<http::ClientRequest>> seen = std::make_shared<std::vector<http::ClientRequest>>();
    http::ClientResponse operator()(const http::ClientRequest& req) const {
        seen->push_back(req);
        http::ClientResponse res;
        res.transport_ok = true;
        res.status = status;
        for (const auto& [needle, body] : routes)
            if (req.url.find(needle) != std::string::npos) {
                res.body = body;
                return res;
            }
        res.body = "{}";
        return res;
    }
};

const auto kNow = [] { return std::string("2026-06-11T00:00:00Z"); };

Json thread(const std::string& id, const std::string& text) {
    return Json{{"kind", "youtube#commentThread"},
                {"etag", "e"},
                {"id", id},
                {"snippet", Json{{"videoId", "v1"},
                                 {"topLevelComment", Json{{"id", id + "-c"},
                                                          {"snippet", Json{{"videoId", "v1"},
                                                                           {"textDisplay", text},
                                                                           {"authorDisplayName", "A"},
                                                                           {"authorChannelId", Json{{"value", "UC1"}}},
                                                                           {"likeCount", 2},
                                                                           {"publishedAt", "p"},
                                                                           {"updatedAt", "u"}}}}},
                                 {"replyCount", 0},
                                 {"totalReplyCount", 1}}}};
}

} // namespace

TEST_CASE("YouTube: コメントスレッドを RawRecord にする") {
    Fake fake{{{"commentThreads", json_stringify(Json{{"items", Json::array({thread("t1", "nice")})}})}}};
    crawl::youtube::YouTubeSource source({fake, kNow});
    const auto r = source.crawl({"v1", "fake-key", std::nullopt, std::nullopt}).at(0);
    CHECK_EQ(r.source, std::string("youtube"));
    CHECK_EQ(r.source_id, std::string("t1"));
    CHECK_EQ(r.url.value_or(""), std::string("https://www.youtube.com/watch?v=v1"));
    CHECK_EQ(r.text.value_or(""), std::string("nice"));
    CHECK(!r.title.has_value());
    CHECK_EQ(json_stringify(r.meta.value()),
             std::string("{\"videoId\":\"v1\",\"authorDisplayName\":\"A\",\"authorChannelId\":\"UC1\",\"likeCount\":2,\"replyCount\":1,"
                         "\"publishedAt\":\"p\",\"updatedAt\":\"u\"}"));
    const std::string& url = fake.seen->at(0).url;
    CHECK(url.find("part=snippet") != std::string::npos && url.find("maxResults=100") != std::string::npos &&
          url.find("textFormat=plainText") != std::string::npos);
}

TEST_CASE("YouTube: maxResults に達するまでページを送り、API の失敗・必須の欠落は例外") {
    Json p1 = Json{{"items", Json::array({thread("a", "1"), thread("b", "2")})}, {"nextPageToken", "n2"}};
    Json p2 = Json{{"items", Json::array({thread("c", "3")})}, {"nextPageToken", "n3"}};
    Fake fake{{{"pageToken=n2", json_stringify(p2)}, {"commentThreads", json_stringify(p1)}}};
    crawl::youtube::YouTubeSource source({fake, kNow});
    const auto got = source.crawl({"v1", "k", 3, std::nullopt});
    CHECK_EQ(got.size(), std::size_t{3});
    CHECK(fake.seen->at(1).url.find("maxResults=1") != std::string::npos);
    CHECK_THROWS(source.crawl({"", "k", std::nullopt, std::nullopt}));
    CHECK_THROWS(source.crawl({"v1", "", std::nullopt, std::nullopt}));
    Fake failing;
    failing.status = 403;
    try {
        crawl::youtube::YouTubeSource({failing, kNow}).crawl({"v1", "secret-key", std::nullopt, std::nullopt});
        CHECK(false);
    } catch (const std::runtime_error& e) {
        CHECK(std::string(e.what()).find("API error 403") != std::string::npos);
        CHECK(std::string(e.what()).find("secret-key") == std::string::npos);
    }
}

TEST_CASE("Reddit: subreddit の投稿を取る (User-Agent 必須)") {
    const Json listing = Json{{"data", Json{{"children", Json::array({Json{{"data", Json{{"id", "p1"},
                                                                                          {"name", "t3_p1"},
                                                                                          {"title", "Title"},
                                                                                          {"selftext", ""},
                                                                                          {"permalink", "/r/x/comments/p1/"},
                                                                                          {"author", "bob"},
                                                                                          {"subreddit", "x"},
                                                                                          {"score", 10},
                                                                                          {"upvote_ratio", 0.9},
                                                                                          {"num_comments", 2},
                                                                                          {"created_utc", 1.0}}}}})}}}};
    Fake fake{{{"/r/x/hot.json", json_stringify(listing)}}};
    crawl::reddit::RedditSourceConfig c;
    c.subreddit = "x";
    const auto r = crawl::reddit::RedditSource({fake, kNow}).crawl(c).at(0);
    CHECK_EQ(r.source_id, std::string("t3_p1"));
    CHECK_EQ(r.url.value_or(""), std::string("https://www.reddit.com/r/x/comments/p1/"));
    CHECK_EQ(r.text.value_or(""), std::string("Title"));  // selftext が空ならタイトル
    CHECK_EQ(json_stringify(r.meta.value()),
             std::string("{\"subreddit\":\"x\",\"author\":\"bob\",\"score\":10,\"upvote_ratio\":0.9,\"num_comments\":2,\"created_utc\":1}"));
    CHECK_EQ(fake.seen->at(0).url, std::string("https://www.reddit.com/r/x/hot.json?limit=100&raw_json=1"));
    CHECK(fake.seen->at(0).headers.get("User-Agent").has_value());
}

TEST_CASE("Reddit: 投稿のコメントを返信ごと平らにし、必須の欠落・API の失敗は例外") {
    const Json reply = Json{{"kind", "t1"}, {"data", Json{{"name", "t1_c2"}, {"body", "reply"}, {"permalink", "/c2"}, {"replies", ""}}}};
    const Json top = Json{{"kind", "t1"},
                          {"data", Json{{"name", "t1_c1"},
                                        {"body", "top"},
                                        {"permalink", "/c1"},
                                        {"replies", Json{{"data", Json{{"children", Json::array({reply})}}}}}}}};
    const Json more = Json{{"kind", "more"}, {"data", Json::object()}};
    const Json body = Json::array({Json{{"data", Json{{"children", Json::array()}}}},
                                   Json{{"data", Json{{"children", Json::array({top, more})}}}}});
    Fake fake{{{"/comments/p1.json", json_stringify(body)}}};
    crawl::reddit::RedditSourceConfig c;
    c.mode = "comments";
    c.post_id = "p1";
    c.subreddit_for_post = "x";
    const auto got = crawl::reddit::RedditSource({fake, kNow}).crawl(c);
    CHECK_EQ(got.size(), std::size_t{2});
    CHECK_EQ(got[0].source_id, std::string("t1_c1"));
    CHECK_EQ(got[1].text.value_or(""), std::string("reply"));
    CHECK_EQ(fake.seen->at(0).url, std::string("https://www.reddit.com/r/x/comments/p1.json?limit=500&raw_json=1"));

    crawl::reddit::RedditSourceConfig none;
    CHECK_THROWS(crawl::reddit::RedditSource({fake, kNow}).crawl(none));
    crawl::reddit::RedditSourceConfig no_post;
    no_post.mode = "comments";
    CHECK_THROWS(crawl::reddit::RedditSource({fake, kNow}).crawl(no_post));
    Fake failing;
    failing.status = 500;
    crawl::reddit::RedditSourceConfig sub;
    sub.subreddit = "x";
    CHECK_THROWS(crawl::reddit::RedditSource({failing, kNow}).crawl(sub));
}

CI_TEST_MAIN()
