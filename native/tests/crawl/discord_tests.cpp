// Discord の移植テスト (test/discord-source.test.ts・test/discord-guild-source.test.ts と同じ観点 + client)。
// 本物の Discord には繋がない (偽の DiscordApi・偽の取得関数を使う)。
#include "check.hpp"

#include <map>
#include <memory>

#include "canalis/crawl/discord/client.hpp"
#include "canalis/crawl/discord/crawl.hpp"
#include "canalis/crawl/discord/source.hpp"

using namespace canalis;
using namespace canalis::crawl::discord;

namespace {

Json msg(const std::string& id, const std::string& channel, const std::string& content, const std::string& ts,
         const std::string& username = "alice", std::optional<std::string> global = std::nullopt) {
    return Json{{"id", id},
                {"channel_id", channel},
                {"author", Json{{"id", "u-" + username}, {"username", username}, {"global_name", global ? Json(*global) : Json(nullptr)}}},
                {"content", content},
                {"timestamp", ts},
                {"edited_timestamp", nullptr},
                {"attachments", Json::array()},
                {"embeds", Json::array()}};
}

class FakeApi : public DiscordApi {
public:
    std::map<std::string, std::vector<Json>> messages;
    std::vector<Json> channels;
    std::vector<Json> active;
    std::map<std::string, ArchivedThreads> archived;
    std::vector<MessagesQuery> queries;

    std::vector<Json> get_messages(const std::string& channel_id, const MessagesQuery& q) override {
        queries.push_back(q);
        auto all = messages[channel_id];
        // before カーソルなら、その id より後ろ (古い側) から limit 件を返す
        std::size_t start = 0;
        if (q.before)
            for (std::size_t i = 0; i < all.size(); ++i)
                if (json_string(all[i], "id") == *q.before) start = i + 1;
        std::vector<Json> out;
        for (std::size_t i = start; i < all.size() && static_cast<int>(out.size()) < q.limit; ++i) out.push_back(all[i]);
        return out;
    }
    std::vector<Json> get_guild_channels(const std::string&) override { return channels; }
    std::vector<Json> get_active_threads(const std::string&) override { return active; }
    ArchivedThreads get_archived_threads(const std::string& channel_id, const std::optional<std::string>&) override {
        return archived[channel_id];
    }
};

FakeApi guild_api() {
    FakeApi api;
    api.channels = {Json{{"id", "c1"}, {"type", 0}, {"name", "general"}}, Json{{"id", "f1"}, {"type", 15}, {"name", "ideas"}},
                    Json{{"id", "v1"}, {"type", 2}, {"name", "voice"}}};
    api.active = {Json{{"id", "t1"}, {"type", 11}, {"name", "Post A"}, {"parent_id", "f1"}},
                  Json{{"id", "tx"}, {"type", 11}, {"name", "Other"}, {"parent_id", "other"}}};
    api.archived["f1"] = {{Json{{"id", "t1"}, {"name", "Post A"}, {"parent_id", "f1"}}}, false};
    api.messages["c1"] = {msg("m2", "c1", "second", "2026-06-01T10:05:00.000Z", "bob", std::string("Bob")),
                          msg("m1", "c1", "first", "2026-06-01T10:00:00.000Z")};
    api.messages["t1"] = {msg("m3", "t1", "idea body", "2026-06-02T09:00:00.000Z")};
    return api;
}

} // namespace

TEST_CASE("DiscordSource: TS と同じ形の RawRecord を返し、guildId が無ければ @me") {
    FakeApi api;
    api.messages["ch"] = {msg("m1", "ch", "hello", "2026-06-01T00:00:00.000Z", "alice", std::string("Alice"))};
    const auto now = [] { return std::string("2026-06-11T00:00:00.000Z"); };
    DiscordSource source({&api, now});
    DiscordSourceConfig config;
    config.channel_id = "ch";
    config.guild_id = "g1";
    const auto r = source.crawl(config).at(0);
    CHECK_EQ(json_stringify(core::to_json(r)),
             "{\"source\":\"discord\",\"sourceId\":\"m1\",\"fetchedAt\":\"2026-06-11T00:00:00.000Z\","
             "\"url\":\"https://discord.com/channels/g1/ch/m1\",\"text\":\"hello\",\"raw\":" + json_stringify(api.messages["ch"][0]) +
             ",\"meta\":{\"channelId\":\"ch\",\"authorId\":\"u-alice\",\"authorName\":\"Alice\",\"timestamp\":\"2026-06-01T00:00:00.000Z\","
             "\"hasAttachments\":false,\"hasEmbeds\":false,\"isReply\":false,\"truncated\":false}}");
    config.guild_id.reset();
    CHECK_EQ(source.crawl(config).at(0).url.value_or(""), std::string("https://discord.com/channels/@me/ch/m1"));
    config.channel_id.clear();
    CHECK_THROWS(source.crawl(config));
}

TEST_CASE("crawl_channel: maxMessages で打ち切ると truncated、全件なら false、before カーソルで送る") {
    FakeApi api;
    for (int i = 0; i < 250; ++i) api.messages["ch"].push_back(msg("m" + std::to_string(i), "ch", "x", "2026-06-01T00:00:00.000Z"));
    DiscordCrawlOptions o;
    o.max_messages = 150;
    const auto r = crawl_channel(api, "ch", o);
    CHECK_EQ(r.messages.size(), std::size_t{150});
    CHECK(r.truncated);
    CHECK_EQ(api.queries.size(), std::size_t{2});
    CHECK_EQ(api.queries[1].limit, 50);
    CHECK_EQ(api.queries[1].before.value_or(""), std::string("m99"));
    FakeApi small;
    small.messages["ch"] = {msg("a", "ch", "x", "t")};
    const auto all = crawl_channel(small, "ch");
    CHECK_EQ(all.messages.size(), std::size_t{1});
    CHECK(!all.truncated);
}

TEST_CASE("crawl_guild / DiscordGuildSource: チャンネルとフォーラムのスレッドごとに 1 件、除外を守る") {
    FakeApi api = guild_api();
    const auto records = crawl_guild(api, "g1");
    CHECK_EQ(records.size(), std::size_t{2});
    CHECK_EQ(records[0].kind, std::string("text-channel"));
    CHECK_EQ(records[1].kind, std::string("forum-thread"));
    CHECK_EQ(records[1].forum_channel_name.value_or(""), std::string("ideas"));

    const auto now = [] { return std::string("2026-06-11T00:00:00.000Z"); };
    DiscordGuildSource source({&api, now});
    DiscordGuildSourceConfig config;
    config.guild_id = "g1";
    const auto out = source.crawl(config);
    CHECK_EQ(out.size(), std::size_t{2});
    CHECK_EQ(out[0].title.value_or(""), std::string("general"));
    CHECK_EQ(out[0].text.value_or(""), std::string("[2026-06-01 10:00 UTC] alice: first\n[2026-06-01 10:05 UTC] Bob: second"));
    CHECK_EQ(out[1].title.value_or(""), std::string("ideas / Post A"));
    CHECK_EQ(json_stringify(out[1].meta.value()),
             std::string("{\"guildId\":\"g1\",\"channelId\":\"t1\",\"channelName\":\"Post A\",\"kind\":\"forum-thread\","
                         "\"forumChannelName\":\"ideas\",\"messageCount\":1}"));
    CHECK(!out[0].raw.contains("forumChannelName"));

    config.exclude_channel_ids = {"c1"};
    CHECK_EQ(source.crawl(config).size(), std::size_t{1});
    config.guild_id.clear();
    CHECK_THROWS(source.crawl(config));
}

TEST_CASE("client: Bot token・パスの符号化・429 の再試行・失敗の文言に token を出さない") {
    auto calls = std::make_shared<std::vector<http::ClientRequest>>();
    auto sleeps = std::make_shared<std::vector<std::int64_t>>();
    auto statuses = std::make_shared<std::vector<int>>(std::vector<int>{429, 200});
    DiscordApiClient client(
        "fake-bot-token",
        [calls, statuses](const http::ClientRequest& req) {
            calls->push_back(req);
            http::ClientResponse res;
            res.transport_ok = true;
            res.status = statuses->empty() ? 200 : statuses->front();
            if (!statuses->empty()) statuses->erase(statuses->begin());
            if (res.status == 429) res.headers.set("Retry-After", "0.25");
            res.body = "[]";
            return res;
        },
        [sleeps](std::chrono::milliseconds d) { sleeps->push_back(d.count()); });
    MessagesQuery q;
    q.limit = 50;
    q.before = "123";
    client.get_messages("ch/1", q);
    CHECK_EQ(calls->size(), std::size_t{2});
    CHECK_EQ(calls->at(0).url, std::string("https://discord.com/api/v10/channels/ch%2F1/messages?limit=50&before=123"));
    CHECK_EQ(calls->at(0).headers.get("Authorization").value_or(""), std::string("Bot fake-bot-token"));
    CHECK_EQ(sleeps->at(0), std::int64_t{350});  // ceil(0.25 * 1000) + 100

    DiscordApiClient failing("fake-bot-token", [](const http::ClientRequest&) {
        http::ClientResponse res;
        res.transport_ok = true;
        res.status = 403;
        res.reason = "Forbidden";
        res.body = "missing access";
        return res;
    });
    try {
        failing.get_guild_channels("g1");
        CHECK(false);
    } catch (const std::runtime_error& e) {
        const std::string what = e.what();
        CHECK(what.find("DiscordApi: 403 Forbidden") != std::string::npos);
        CHECK(what.find("fake-bot-token") == std::string::npos);
    }
    CHECK_THROWS(DiscordApiClient(""));
}

CI_TEST_MAIN()
