// CLI の引数・出力・終了コード (spec/interface/cli.md)。コマンドは関数として呼び、最後に実行ファイルも起動して確かめる。
// 外部には繋がない (取得を伴うコマンドは、取得しない経路 = 引数の誤り・SSRF の遮断・token 無しだけを見る)。
#include "check.hpp"

#include <filesystem>

#include "canalis/base/crypto.hpp"
#include "canalis/base/env.hpp"
#include "canalis/base/fsutil.hpp"
#include "canalis/base/json.hpp"
#include "canalis/base/process.hpp"
#include "cli/commands.hpp"

using namespace canalis;

namespace {

struct Captured {
    std::string out;
    std::string err;
    std::string stdin_data;
    bool tty = false;
    cli::Io io() {
        cli::Io io;
        io.out = [this](std::string_view s) { out += s; };
        io.err = [this](std::string_view s) { err += s; };
        io.read_stdin = [this](std::size_t max) -> std::optional<std::string> {
            if (stdin_data.size() > max) return std::nullopt;
            return stdin_data;
        };
        io.stdin_is_tty = [this] { return tty; };
        io.prompt_hidden = [](std::string_view) { return std::string("from_prompt"); };
        return io;
    }
};

int run(Captured& c, std::vector<std::string> args) { return cli::run(args, c.io()); }

struct TempConfig {
    std::string dir = fs::from_path(std::filesystem::temp_directory_path() /
                                    fs::to_path("canalis-cli-" + crypto::hex_encode(crypto::random_bytes(6))));
    TempConfig() {
        env::set_for_test("CANALIS_CONFIG_PATH", fs::join(dir, "config.json"));
        env::set_for_test("CANALIS_MASTER_KEY", "test-master-key");
        env::set_for_test("NOTION_TOKEN", std::nullopt);
    }
    ~TempConfig() {
        std::error_code ec;
        std::filesystem::remove_all(fs::to_path(dir), ec);
        for (const char* n : {"CANALIS_CONFIG_PATH", "CANALIS_MASTER_KEY", "NOTION_TOKEN"}) env::set_for_test(n, std::nullopt);
    }
};

} // namespace

TEST_CASE("未知のコマンドは使えるコマンドの一覧を出して exit 2") {
    Captured c;
    CHECK_EQ(run(c, {}), 2);
    CHECK_EQ(c.err, std::string("commands: notion-crawl, config, crawl, notion-public-crawl, website-parse\n"));
    Captured d;
    CHECK_EQ(run(d, {"nope"}), 2);
}

TEST_CASE("notion-crawl: databaseId 無し・数でない --max-depth・token 無しは exit 2") {
    TempConfig t;
    Captured a;
    CHECK_EQ(run(a, {"notion-crawl"}), 2);
    CHECK(a.err.find("usage: canalis notion-crawl") != std::string::npos);
    Captured b;
    CHECK_EQ(run(b, {"notion-crawl", "db", "--max-depth", "x"}), 2);
    Captured c;
    CHECK_EQ(run(c, {"notion-crawl", "db"}), 2);
    CHECK(c.err.find("Notion token is not set") != std::string::npos);
}

TEST_CASE("config: set / show / path / unset と、誤りの exit 2") {
    TempConfig t;
    Captured set;
    CHECK_EQ(run(set, {"config", "set", "notion.token", "ntn_cli_abcd9876"}), 0);
    CHECK(set.out.find("saved notion.token (encrypted)") != std::string::npos);
    CHECK(set.out.find("ntn_cli_abcd9876") == std::string::npos);
    Captured version;
    CHECK_EQ(run(version, {"config", "set", "notion.version", "2099-01-01"}), 0);
    CHECK_EQ(version.out, std::string("saved notion.version=2099-01-01\n"));
    Captured interval;
    CHECK_EQ(run(interval, {"config", "set", "notion.minIntervalMs", "500"}), 0);
    CHECK_EQ(interval.out, std::string("saved notion.minIntervalMs=500\n"));
    Captured show;
    CHECK_EQ(run(show, {"config", "show"}), 0);
    CHECK(show.out.find("notion.token   : \xE2\x80\xA6" "9876 [source: config]") != std::string::npos);
    CHECK(show.out.find("notion.version : 2099-01-01") != std::string::npos);
    CHECK(show.out.find("notion.minIntervalMs : 500") != std::string::npos);
    CHECK(show.out.find("ntn_cli_abcd9876") == std::string::npos);
    Captured path;
    CHECK_EQ(run(path, {"config", "path"}), 0);
    CHECK_EQ(path.out, fs::join(t.dir, "config.json") + "\n");
    Captured unset;
    CHECK_EQ(run(unset, {"config", "unset", "notion.token"}), 0);
    Captured after;
    run(after, {"config", "show"});
    CHECK(after.out.find("(unset) [source: none]") != std::string::npos);

    Captured bad_key;
    CHECK_EQ(run(bad_key, {"config", "set", "notion.nope", "x"}), 2);
    CHECK(bad_key.err.find("unknown key: notion.nope") != std::string::npos);
    Captured bad_ms;
    CHECK_EQ(run(bad_ms, {"config", "set", "notion.minIntervalMs", "-1"}), 2);
    Captured bad_sub;
    CHECK_EQ(run(bad_sub, {"config"}), 2);
    CHECK(bad_sub.err.find("unknown subcommand: (none)") != std::string::npos);
}

TEST_CASE("config set notion.token は値を省くとパイプ・端末から読む") {
    TempConfig t;
    Captured piped;
    piped.stdin_data = "  ntn_from_pipe_1111\n";
    CHECK_EQ(run(piped, {"config", "set", "notion.token"}), 0);
    Captured show;
    run(show, {"config", "show"});
    CHECK(show.out.find("\xE2\x80\xA6" "1111") != std::string::npos);
    Captured tty;
    tty.tty = true;
    CHECK_EQ(run(tty, {"config", "set", "notion.token"}), 0);
    Captured empty;
    CHECK_EQ(run(empty, {"config", "set", "notion.token"}), 2);
    CHECK_EQ(empty.err, std::string("error: token is empty\n"));
}

TEST_CASE("website-parse は標準入力の HTML を解析して 1 行の JSON を出す") {
    Captured c;
    c.stdin_data = "<title>T &amp; U</title><p>hi</p><a href=\"/a\">a</a><a href=\"https://x.example/\">x</a>";
    CHECK_EQ(run(c, {"website-parse", "--url", "https://example.com/dir/"}), 0);
    CHECK_EQ(c.out, std::string("{\"title\":\"T & U\",\"text\":\"T & U hi a x\",\"links\":[\"https://example.com/a\"]}\n"));
    Captured missing;
    CHECK_EQ(run(missing, {"website-parse"}), 2);
}

TEST_CASE("crawl: 引数の誤りは exit 2、loopback の website は取得しない (例外)") {
    Captured a;
    CHECK_EQ(run(a, {"crawl", "website"}), 2);
    Captured b;
    CHECK_EQ(run(b, {"crawl", "nope", "--config", "{}"}), 2);
    Captured c;
    CHECK_THROWS(run(c, {"crawl", "website", "--config", "[1]"}));
    Captured d;
    CHECK_THROWS(run(d, {"crawl", "website", "--config", "{\"url\":\"http://127.0.0.1:1/\",\"timeoutMs\":1000}"}));
}

TEST_CASE("notion-public-crawl: 引数の誤りは exit 2、loopback は取得せず errors に積む") {
    Captured a;
    CHECK_EQ(run(a, {"notion-public-crawl"}), 2);
    Captured b;
    CHECK_EQ(run(b, {"notion-public-crawl", "https://x.notion.site/a", "--timeout", "0"}), 2);
    Captured c;
    CHECK_EQ(run(c, {"notion-public-crawl", "http://127.0.0.1:1/x"}), 0);
    const Json out = parse_json_or_throw(c.out);
    CHECK(json_array(out, "pages").empty());
    CHECK(json_string(json_array(out, "errors").at(0), "message").find("取得しません") != std::string::npos);
}

TEST_CASE("実行ファイル: 終了コードと標準入出力") {
    const auto exe = env::get("CI_CANALIS_EXE");
    CHECK(exe.has_value());
    proc::Options unknown;
    unknown.argv = {*exe, "nope"};
    unknown.timeout = std::chrono::milliseconds(30'000);
    const auto r1 = proc::run(unknown);
    CHECK(r1.started);
    CHECK_EQ(r1.exit_code, 2);
    CHECK(r1.err.find("commands: notion-crawl") != std::string::npos);

    proc::Options parse;
    parse.argv = {*exe, "website-parse", "--url", "https://example.com/日本/"};
    parse.stdin_data = "<title>日本語</title><a href=\"a b\">x</a>";
    parse.timeout = std::chrono::milliseconds(30'000);
    const auto r2 = proc::run(parse);
    CHECK_EQ(r2.exit_code, 0);
    CHECK_EQ(r2.out, std::string("{\"title\":\"日本語\",\"text\":\"日本語 x\",\"links\":[\"https://example.com/%E6%97%A5%E6%9C%AC/a%20b\"]}\n"));

    proc::Options broken;
    broken.argv = {*exe, "crawl", "website", "--config", "@does-not-exist.json"};
    broken.timeout = std::chrono::milliseconds(30'000);
    const auto r3 = proc::run(broken);
    CHECK_EQ(r3.exit_code, 1);
    CHECK(r3.err.find("canalis: cannot read config file") != std::string::npos);
}

CI_TEST_MAIN()
