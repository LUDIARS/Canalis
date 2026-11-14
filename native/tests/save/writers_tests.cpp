// Postgres / Kuzu / FT の writer (test/ft-writer.test.ts の観点 + Postgres・Kuzu の文の形)。DB には繋がない (実行関数を記録する)。
#include "check.hpp"

#include <filesystem>

#include "canalis/base/crypto.hpp"
#include "canalis/base/env.hpp"
#include "canalis/base/fsutil.hpp"
#include "canalis/save/ft_writer.hpp"
#include "canalis/save/kuzu_writer.hpp"
#include "canalis/save/postgres_writer.hpp"

using namespace canalis;

namespace {

struct TempDir {
    std::string path = fs::from_path(std::filesystem::temp_directory_path() /
                                     fs::to_path("canalis-ft-" + crypto::hex_encode(crypto::random_bytes(6))));
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(fs::to_path(path), ec);
    }
};

} // namespace

TEST_CASE("Postgres: 行ごとの INSERT ... ON CONFLICT と、値はパラメタ") {
    std::vector<std::pair<std::string, std::vector<Json>>> calls;
    save::PostgresSink sink([&](const std::string& q, const std::vector<Json>& p) { calls.emplace_back(q, p); });
    sink.write_batch(parse_json_or_throw(
        "{\"kind\":\"rdb\",\"upserts\":[{\"table\":\"companies\",\"conflictKey\":[\"id\"],"
        "\"rows\":[{\"id\":1,\"name\":\"Acme\"},{\"id\":2}]},{\"table\":\"empty\",\"conflictKey\":[\"id\"],\"rows\":[]}]}"));
    CHECK_EQ(calls.size(), std::size_t{2});
    CHECK_EQ(calls[0].first, std::string("INSERT INTO \"companies\" (\"id\", \"name\") VALUES ($1, $2) ON CONFLICT (\"id\") DO UPDATE SET \"name\" = EXCLUDED.\"name\""));
    CHECK_EQ(json_stringify(Json(calls[0].second)), std::string("[1,\"Acme\"]"));
    CHECK_EQ(calls[1].first, std::string("INSERT INTO \"companies\" (\"id\") VALUES ($1) ON CONFLICT (\"id\") DO NOTHING"));
    CHECK_THROWS(save::build_upsert_statements(parse_json_or_throw("{\"table\":\"a\\\"b\",\"conflictKey\":[],\"rows\":[{\"x\":1}]}")));
    CHECK_EQ(save::PostgresSink([](const std::string&, const std::vector<Json>&) {}).accepts(), std::string_view("rdb"));
}

TEST_CASE("Kuzu: ノードとエッジの MERGE・props の SET、危ない名前は拒否") {
    std::vector<std::pair<std::string, Json>> calls;
    save::KuzuSink sink([&](const std::string& q, const Json& p) { calls.emplace_back(q, p); });
    sink.write_batch(parse_json_or_throw(
        "{\"kind\":\"kg\",\"nodes\":[{\"label\":\"Game\",\"key\":\"g1\",\"props\":{\"title\":\"T\"}},{\"label\":\"Tag\",\"key\":\"t1\"}],"
        "\"edges\":[{\"label\":\"HAS_TAG\",\"from\":{\"label\":\"Game\",\"key\":\"g1\"},\"to\":{\"label\":\"Tag\",\"key\":\"t1\"},\"props\":{\"w\":2}}]}"));
    CHECK_EQ(calls.size(), std::size_t{3});
    CHECK_EQ(calls[0].first, std::string("MERGE (n:Game {key: $key}) SET n.title = $p_title"));
    CHECK_EQ(json_stringify(calls[0].second), std::string("{\"key\":\"g1\",\"p_title\":\"T\"}"));
    CHECK_EQ(calls[1].first, std::string("MERGE (n:Tag {key: $key})"));
    CHECK_EQ(calls[2].first, std::string("MATCH (a:Game {key: $fromKey}), (b:Tag {key: $toKey}) MERGE (a)-[r:HAS_TAG]->(b) SET r.w = $p_w"));
    CHECK_EQ(json_stringify(calls[2].second), std::string("{\"fromKey\":\"g1\",\"toKey\":\"t1\",\"p_w\":2}"));
    CHECK_THROWS(sink.write_batch(parse_json_or_throw("{\"nodes\":[{\"label\":\"Bad Label\",\"key\":\"x\"}]}")));
    CHECK_THROWS(sink.write_batch(parse_json_or_throw("{\"nodes\":[{\"label\":\"Ok\",\"key\":\"x\",\"props\":{\"a}) DELETE n //\":1}}]}")));
    CHECK_THROWS(save::KuzuSink([](const std::string&, const Json&) {}, {std::string("bad key")}));
}

TEST_CASE("FT: classification を JSONL にして runner を起動する") {
    TempDir dir;
    std::vector<save::FtJob> seen;
    save::FtSink sink({dir.path, [&](const save::FtJob& job) { seen.push_back(job); }, [] { return std::string("2026-06-11T00:00:00.000Z"); }});
    sink.write_batch(parse_json_or_throw(
        "{\"kind\":\"ft\",\"task\":\"classification\",\"dataset\":\"sentiment\",\"model\":{\"base\":\"distilbert-multilingual\",\"method\":\"lora\"},"
        "\"examples\":[{\"input\":\"good\",\"label\":\"pos\",\"extra\":1},{\"input\":\"bad\",\"label\":\"neg\"}]}"));
    const std::string base = fs::join(fs::join(dir.path, "sentiment"), "2026-06-11T00-00-00.000Z");
    CHECK_EQ(fs::read_file(fs::join(base, "data.jsonl")).value_or(""),
             std::string("{\"input\":\"good\",\"label\":\"pos\"}\n{\"input\":\"bad\",\"label\":\"neg\"}\n"));
    const Json job = parse_json_or_throw(fs::read_file(fs::join(base, "job.json")).value_or("{}"));
    CHECK_EQ(json_string(job, "task"), std::string("classification"));
    CHECK_EQ(json_int(job, "count"), std::int64_t{2});
    CHECK_EQ(json_stringify(job["model"]), std::string("{\"base\":\"distilbert-multilingual\",\"method\":\"lora\"}"));
    CHECK_EQ(seen.size(), std::size_t{1});
    CHECK_EQ(seen[0].dataset, std::string("sentiment"));
}

TEST_CASE("FT: causal-lm の chat / prompt-completion、合わない例と危ない dataset 名は失敗") {
    TempDir dir;
    save::FtSink sink({dir.path, {}, [] { return std::string("2026-06-11T00:00:00.000Z"); }});
    sink.write_batch(parse_json_or_throw(
        "{\"kind\":\"ft\",\"task\":\"causal-lm\",\"examples\":[{\"messages\":[{\"role\":\"user\",\"content\":\"q\"},{\"role\":\"assistant\",\"content\":\"a\"}]},"
        "{\"prompt\":\"p\",\"completion\":\"c\"}]}"));
    const std::string data = fs::read_file(fs::join(fs::join(fs::join(dir.path, "default"), "2026-06-11T00-00-00.000Z"), "data.jsonl")).value_or("");
    CHECK_EQ(data, std::string("{\"messages\":[{\"role\":\"user\",\"content\":\"q\"},{\"role\":\"assistant\",\"content\":\"a\"}]}\n"
                               "{\"prompt\":\"p\",\"completion\":\"c\"}\n"));
    try {
        sink.write_batch(parse_json_or_throw("{\"task\":\"classification\",\"examples\":[{\"prompt\":\"p\"}]}"));
        CHECK(false);
    } catch (const std::invalid_argument& e) {
        CHECK(std::string(e.what()).find("classification example requires") != std::string::npos);
    }
    CHECK_THROWS(sink.write_batch(parse_json_or_throw("{\"task\":\"causal-lm\",\"dataset\":\"../escape\",\"examples\":[]}")));
    CHECK_THROWS(sink.write_batch(parse_json_or_throw("{\"task\":\"causal-lm\",\"dataset\":\"a/b\",\"examples\":[]}")));
}

TEST_CASE("command_runner: 引数の配列で起動し、--job を足す。0 以外の終了は失敗") {
    const auto exe = env::get("CI_CANALIS_EXE");
    CHECK(exe.has_value());
    save::FtJob job;
    job.job_path = "job.json";
    // canalis website-parse は --job を無視して 0 で終わる (標準入力は空)。
    save::command_runner(*exe, {"website-parse", "--url", "https://example.com/"})(job);
    // 未知のコマンドは exit 2。
    try {
        save::command_runner(*exe, {"nope"})(job);
        CHECK(false);
    } catch (const std::runtime_error& e) {
        CHECK(std::string(e.what()).find("exited with code 2") != std::string::npos);
    }
}

CI_TEST_MAIN()
