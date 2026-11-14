// JsonlRawSink と読み戻し (src/save/raw-writer.ts と同じ書式: 1 行 1 件・末尾改行・stamp の ":" は "-")。
#include "check.hpp"

#include <filesystem>

#include "canalis/base/crypto.hpp"
#include "canalis/base/fsutil.hpp"
#include "canalis/base/strings.hpp"
#include "canalis/save/raw_writer.hpp"

using namespace canalis;

namespace {

struct TempDir {
    std::string path = fs::from_path(std::filesystem::temp_directory_path() /
                                     fs::to_path("canalis-raw-" + crypto::hex_encode(crypto::random_bytes(6))));
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(fs::to_path(path), ec);
    }
};

core::RawRecord record(const std::string& id) {
    core::RawRecord r;
    r.source = "website";
    r.source_id = id;
    r.fetched_at = "2026-06-11T00:00:00Z";
    r.text = "本文 " + id;
    r.raw = Json{{"html", "<p>" + id + "</p>"}};
    return r;
}

} // namespace

TEST_CASE("1 行 1 件の JSONL で書き、読み戻すと同じ JSON になる") {
    TempDir dir;
    const auto file = save::JsonlRawSink({fs::join(dir.path, "notion/db"), std::string("2026-06-11T00:00:00.000Z")})
                          .write({record("a"), record("b")});
    CHECK(str::ends_with(file, "2026-06-11T00-00-00.000Z.jsonl"));
    const auto text = fs::read_file(file).value_or("");
    CHECK_EQ(text, json_stringify(core::to_json(record("a"))) + "\n" + json_stringify(core::to_json(record("b"))) + "\n");
    const auto back = save::load_raw_records_from_file(file);
    CHECK_EQ(back.size(), std::size_t{2});
    CHECK_EQ(json_stringify(core::to_json(back[1])), json_stringify(core::to_json(record("b"))));
}

TEST_CASE("空の書き出しは空ファイル、ディレクトリは名前順に連結する") {
    TempDir dir;
    save::JsonlRawSink({dir.path, std::string("2026-01-02")}).write({record("2")});
    save::JsonlRawSink({dir.path, std::string("2026-01-01")}).write({record("1")});
    const auto empty = save::JsonlRawSink({dir.path, std::string("2026-01-03")}).write({});
    CHECK_EQ(fs::read_file(empty).value_or("x"), std::string(""));
    CHECK(fs::write_file_atomic(fs::join(dir.path, "note.txt"), "ignored"));
    const auto all = save::load_raw_records_from_dir(dir.path);
    CHECK_EQ(all.size(), std::size_t{2});
    CHECK_EQ(all[0].source_id, std::string("1"));
    CHECK_EQ(all[1].source_id, std::string("2"));
}

TEST_CASE("壊れた行は場所つきで失敗する") {
    TempDir dir;
    const std::string file = fs::join(dir.path, "bad.jsonl");
    CHECK(fs::ensure_dir(dir.path));
    CHECK(fs::write_file_atomic(file, json_stringify(core::to_json(record("ok"))) + "\n\n{broken\n"));
    try {
        save::load_raw_records_from_file(file);
        CHECK(false);
    } catch (const std::runtime_error& e) {
        CHECK(std::string(e.what()).find(":3:") != std::string::npos);
    }
}

CI_TEST_MAIN()
