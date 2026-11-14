// manifest の読み込み (test/core/manifest-loader.test.ts の観点、D3 で JSON のみ) と runner (crawl → raw 保存 → transform → save)。
#include "check.hpp"

#include <filesystem>
#include <memory>

#include "canalis/base/crypto.hpp"
#include "canalis/base/fsutil.hpp"
#include "canalis/core/manifest.hpp"
#include "canalis/runner/runner.hpp"

using namespace canalis;

namespace {

struct TempDir {
    std::string path = fs::from_path(std::filesystem::temp_directory_path() /
                                     fs::to_path("canalis-runner-" + crypto::hex_encode(crypto::random_bytes(6))));
    TempDir() { fs::ensure_dir(path); }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(fs::to_path(path), ec);
    }
};

class FakeSource final : public core::JsonSource {
public:
    explicit FakeSource(bool fail = false) : fail_(fail) {}
    std::string_view name() const override { return "fake"; }
    std::vector<core::RawRecord> crawl(const Json& config) override {
        if (fail_) throw std::runtime_error("source down");
        std::vector<core::RawRecord> out;
        for (int i = 0; i < json_int(config, "n", 1); ++i) {
            core::RawRecord r;
            r.source = "fake";
            r.source_id = std::to_string(i);
            r.fetched_at = "t";
            r.text = "text " + std::to_string(i);
            out.push_back(r);
        }
        return out;
    }

private:
    bool fail_;
};

class FakeTransform final : public core::Transform {
public:
    std::string_view name() const override { return "t"; }
    std::vector<core::SinkBatch> run(const std::vector<core::RawRecord>& records) override {
        Json nodes = Json::array();
        for (const auto& r : records) nodes.push_back(Json{{"label", "Doc"}, {"key", r.source_id}});
        return {Json{{"kind", "kg"}, {"nodes", nodes}, {"edges", Json::array()}},
                Json{{"kind", "rdb"}, {"upserts", Json::array()}}};
    }
};

class RecordingSink final : public core::Sink {
public:
    RecordingSink(std::string name, std::string accepts, bool fail = false) : name_(std::move(name)), accepts_(std::move(accepts)), fail_(fail) {}
    std::string_view name() const override { return name_; }
    std::string_view accepts() const override { return accepts_; }
    void write_raw(const std::vector<core::RawRecord>& records) override { raw_count += records.size(); }
    void write_batch(const core::SinkBatch& batch) override {
        if (fail_) throw std::runtime_error("db down");
        batches.push_back(batch);
    }
    std::size_t raw_count = 0;
    std::vector<core::SinkBatch> batches;

private:
    std::string name_, accepts_;
    bool fail_;
};

core::Manifest manifest(const std::string& json) { return core::manifest_from_json(parse_json_or_throw(json), "test"); }

} // namespace

TEST_CASE("JSON の manifest を読み、YAML・pipeline 無し・無いファイルは失敗") {
    TempDir dir;
    const std::string path = fs::join(dir.path, "m.json");
    CHECK(fs::write_file_atomic(path, "{\"pipeline\":\"test-json\",\"crawl\":{\"sources\":[{\"adapter\":\"youtube\",\"config\":{\"videoId\":\"vid1\"}}],"
                                      "\"rawSave\":{\"adapter\":\"jsonl\",\"accepts\":\"raw\",\"config\":{\"dir\":\"d\"}}},"
                                      "\"transform\":\"di:x\",\"save\":[{\"adapter\":\"kuzu\",\"accepts\":\"kg\"}]}"));
    const auto m = core::load_manifest(path);
    CHECK_EQ(m.pipeline, std::string("test-json"));
    CHECK_EQ(m.sources.size(), std::size_t{1});
    CHECK_EQ(json_string(m.sources[0].config, "videoId"), std::string("vid1"));
    CHECK_EQ(m.raw_save->accepts, std::string("raw"));
    CHECK_EQ(m.transform.value_or(""), std::string("di:x"));
    CHECK_EQ(m.save.at(0).adapter, std::string("kuzu"));

    const std::string yaml = fs::join(dir.path, "m.yaml");
    CHECK(fs::write_file_atomic(yaml, "pipeline: x\n"));
    try {
        core::load_manifest(yaml);
        CHECK(false);
    } catch (const std::runtime_error& e) {
        CHECK(std::string(e.what()).find("JSON") != std::string::npos);
    }
    CHECK_THROWS(core::load_manifest(fs::join(dir.path, "m.yml")));
    const std::string no_pipeline = fs::join(dir.path, "bad.json");
    CHECK(fs::write_file_atomic(no_pipeline, "{\"crawl\":{}}"));
    try {
        core::load_manifest(no_pipeline);
        CHECK(false);
    } catch (const std::runtime_error& e) {
        CHECK(std::string(e.what()).find("pipeline") != std::string::npos);
    }
    CHECK_THROWS(core::load_manifest(fs::join(dir.path, "nonexistent.json")));
    CHECK_THROWS(manifest("{\"pipeline\":\"p\"}"));                                       // crawl 無し
    CHECK_THROWS(manifest("{\"pipeline\":\"p\",\"crawl\":{},\"save\":[{\"adapter\":\"x\"}]}"));  // accepts 無し
}

TEST_CASE("runner: crawl → raw 保存 → transform → kind ごとに sink へ振り分ける") {
    FakeSource source;
    FakeTransform transform;
    RecordingSink raw("jsonl", "raw"), kg("kuzu", "kg"), rdb("postgres", "rdb");
    runner::PipelineDeps deps;
    deps.sources["fake"] = &source;
    deps.transforms["t"] = &transform;
    deps.sinks = {{"jsonl", &raw}, {"kuzu", &kg}, {"postgres", &rdb}};
    const auto m = manifest("{\"pipeline\":\"p\",\"crawl\":{\"sources\":[{\"adapter\":\"fake\",\"config\":{\"n\":3}}],"
                            "\"rawSave\":{\"adapter\":\"jsonl\",\"accepts\":\"raw\"}},\"transform\":\"t\","
                            "\"save\":[{\"adapter\":\"kuzu\",\"accepts\":\"kg\"},{\"adapter\":\"postgres\",\"accepts\":\"rdb\"}]}");
    const auto report = runner::run_pipeline(m, deps);
    CHECK_EQ(raw.raw_count, std::size_t{3});
    CHECK_EQ(kg.batches.size(), std::size_t{1});
    CHECK_EQ(json_array(kg.batches[0], "nodes").size(), std::size_t{3});
    CHECK_EQ(rdb.batches.size(), std::size_t{1});
    CHECK_EQ(json_stringify(runner::to_json(report)),
             std::string("{\"pipeline\":\"p\",\"mode\":\"crawl\",\"recordCount\":3,\"rawSaved\":true,"
                         "\"written\":[{\"sink\":\"kuzu\",\"kind\":\"kg\",\"batches\":1},{\"sink\":\"postgres\",\"kind\":\"rdb\",\"batches\":1}],"
                         "\"errors\":[]}"));
}

TEST_CASE("runner: 段ごとの失敗は errors に積んで続け、manifest の誤りは止める") {
    FakeSource broken(true);
    FakeTransform transform;
    RecordingSink kg("kuzu", "kg", /*fail=*/true);
    runner::PipelineDeps deps;
    deps.sources["fake"] = &broken;
    deps.transforms["t"] = &transform;
    deps.sinks["kuzu"] = &kg;
    const auto m = manifest("{\"pipeline\":\"p\",\"crawl\":{\"sources\":[{\"adapter\":\"fake\"}]},\"transform\":\"t\","
                            "\"save\":[{\"adapter\":\"kuzu\",\"accepts\":\"kg\"}]}");
    const auto report = runner::run_pipeline(m, deps);
    CHECK_EQ(report.errors.size(), std::size_t{2});
    CHECK_EQ(report.errors[0].stage, std::string("crawl:fake"));
    CHECK_EQ(report.errors[1].stage, std::string("save:kuzu"));
    CHECK_EQ(report.written.at(0).batches, 0);

    CHECK_THROWS(runner::run_pipeline(manifest("{\"pipeline\":\"p\",\"crawl\":{\"sources\":[{\"adapter\":\"nope\"}]}}"), deps));
    CHECK_THROWS(runner::run_pipeline(manifest("{\"pipeline\":\"p\",\"crawl\":{},\"save\":[{\"adapter\":\"kuzu\",\"accepts\":\"rdb\"}]}"), deps));
    CHECK_THROWS(runner::run_pipeline(manifest("{\"pipeline\":\"p\",\"crawl\":{},\"transform\":\"nope\"}"), deps));
}

TEST_CASE("runner: replay は raw store から読み直し、raw 保存を上書きしない") {
    RecordingSink raw("jsonl", "raw");
    runner::PipelineDeps deps;
    deps.sinks["jsonl"] = &raw;
    deps.replay_loader = [](const core::SinkSpec& spec) {
        CHECK_EQ(spec.adapter, std::string("jsonl"));
        core::RawRecord r;
        r.source = "x";
        r.source_id = "1";
        r.fetched_at = "t";
        return std::vector<core::RawRecord>{r, r};
    };
    const auto m = manifest("{\"pipeline\":\"p\",\"crawl\":{\"sources\":[{\"adapter\":\"never\"}],"
                            "\"rawSave\":{\"adapter\":\"jsonl\",\"accepts\":\"raw\"}},"
                            "\"replayFrom\":{\"adapter\":\"jsonl\",\"accepts\":\"raw\"}}");
    const auto report = runner::run_pipeline(m, deps);
    CHECK_EQ(report.mode, std::string("replay"));
    CHECK_EQ(report.record_count, std::size_t{2});
    CHECK(!report.raw_saved);
    CHECK_EQ(raw.raw_count, std::size_t{0});
}

CI_TEST_MAIN()
