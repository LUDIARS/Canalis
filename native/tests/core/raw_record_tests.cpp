// RawRecord の JSON (TS の adapter が組むオブジェクトと同じキー順・値の無い項目は出さない)。
#include "check.hpp"

#include "canalis/core/raw_record.hpp"

using namespace canalis;

TEST_CASE("キーは TS と同じ順で、値の無い項目は出さない") {
    core::RawRecord r;
    r.source = "website";
    r.source_id = "example.com/";
    r.fetched_at = "2026-06-11T00:00:00Z";
    r.url = "https://example.com/";
    r.raw = Json{{"html", "<p>x</p>"}};
    CHECK_EQ(json_stringify(core::to_json(r)),
             std::string("{\"source\":\"website\",\"sourceId\":\"example.com/\",\"fetchedAt\":\"2026-06-11T00:00:00Z\","
                         "\"url\":\"https://example.com/\",\"raw\":{\"html\":\"<p>x</p>\"}}"));
    r.title = "T";
    r.text = "";
    r.meta = Json{{"k", 1}};
    CHECK_EQ(json_stringify(core::to_json(r)),
             std::string("{\"source\":\"website\",\"sourceId\":\"example.com/\",\"fetchedAt\":\"2026-06-11T00:00:00Z\","
                         "\"url\":\"https://example.com/\",\"title\":\"T\",\"text\":\"\",\"raw\":{\"html\":\"<p>x</p>\"},"
                         "\"meta\":{\"k\":1}}"));
}

TEST_CASE("JSON から読み戻せる。必須の項目が無ければ nullopt") {
    const Json j = parse_json_or_throw(
        "{\"source\":\"notion-public\",\"sourceId\":\"u\",\"fetchedAt\":\"t\",\"title\":\"x\",\"raw\":[1],\"meta\":{\"a\":true}}");
    const auto r = core::raw_record_from_json(j);
    CHECK(r.has_value());
    CHECK_EQ(r->title.value_or(""), std::string("x"));
    CHECK(!r->url.has_value());
    CHECK_EQ(json_stringify(core::to_json(*r)), json_stringify(j));
    CHECK(!core::raw_record_from_json(Json{{"source", "x"}}).has_value());
    CHECK(!core::raw_record_from_json(Json::array()).has_value());
}

CI_TEST_MAIN()
