// config の移植テスト (test/config-store.test.ts と同じ観点 + TS 版との互換)。
// - 暗号化の往復、master 鍵違いは失敗、平文を含まない
// - TS 版が書いた EncryptedBlob / config.json を C++ 版が読める (fixtures/config/crypto_golden.json)
// - 保存・解決順 (env NOTION_TOKEN 優先)・非シークレットは平文・表示はマスク
#include "check.hpp"

#include <filesystem>
#include <string>

#include "canalis/base/crypto.hpp"
#include "canalis/base/env.hpp"
#include "canalis/base/fsutil.hpp"
#include "canalis/base/json.hpp"
#include "canalis/config/crypto.hpp"
#include "canalis/config/store.hpp"

using namespace canalis;

namespace {

const std::string kMaster = "test-master-key";

Json golden() {
    const auto text = fs::read_file(fs::join(test::fixtures_dir(), "config/crypto_golden.json"));
    CHECK(text.has_value());
    return parse_json_or_throw(*text);
}

/** 一時ディレクトリの config.json を使い、関係する env を消す。終わったら片付ける。 */
struct TempConfig {
    std::string dir;
    std::string path;
    TempConfig() {
        dir = fs::from_path(std::filesystem::temp_directory_path() /
                            fs::to_path("canalis-cfg-" + crypto::hex_encode(crypto::random_bytes(6))));
        path = fs::join(dir, "config.json");
        env::set_for_test("CANALIS_CONFIG_PATH", path);
        env::set_for_test("CANALIS_MASTER_KEY", kMaster);
        for (const char* name : {"NOTION_TOKEN", "NOTION_VERSION", "NOTION_MIN_INTERVAL_MS"}) env::set_for_test(name, std::nullopt);
    }
    ~TempConfig() {
        std::error_code ec;
        std::filesystem::remove_all(fs::to_path(dir), ec);
        for (const char* name : {"CANALIS_CONFIG_PATH", "CANALIS_MASTER_KEY", "NOTION_TOKEN", "NOTION_VERSION",
                                 "NOTION_MIN_INTERVAL_MS"})
            env::set_for_test(name, std::nullopt);
    }
    std::string raw() const { return fs::read_file(path).value_or(""); }
};

} // namespace

TEST_CASE("暗号化→復号で元の値に戻り、blob に平文を含まない") {
    const auto blob = config::encrypt_json(Json("secret_abc123"), kMaster);
    const Json j = config::to_json(blob);
    CHECK(config::is_encrypted_blob(j));
    CHECK(json_stringify(j).find("secret_abc123") == std::string::npos);
    CHECK_EQ(config::decrypt_json(blob, kMaster), Json("secret_abc123"));
    CHECK_EQ(crypto::base64_decode(blob.salt)->size(), std::size_t{16});
    CHECK_EQ(crypto::base64_decode(blob.iv)->size(), std::size_t{12});
    CHECK_EQ(crypto::base64_decode(blob.tag)->size(), std::size_t{16});
}

TEST_CASE("master 鍵違い・改竄は復号に失敗する") {
    auto blob = config::encrypt_json(Json("x"), kMaster);
    CHECK_THROWS(config::decrypt_json(blob, "wrong-key"));
    auto tampered = blob;
    tampered.tag = crypto::base64_encode(std::string(16, '\0'));
    CHECK_THROWS(config::decrypt_json(tampered, kMaster));
    auto broken = blob;
    broken.iv = "!!";
    CHECK_THROWS(config::decrypt_json(broken, kMaster));
}

TEST_CASE("TS 版が暗号化した値を復号できる (golden)") {
    const Json g = golden();
    for (const auto& c : g["cases"]) {
        const auto blob = config::blob_from_json(c["blob"]);
        CHECK(blob.has_value());
        CHECK_EQ(json_stringify(config::decrypt_json(*blob, json_string(g, "master"))), json_stringify(c["value"]));
    }
}

TEST_CASE("TS 版が書いた config.json から token と非シークレットを読む (golden)") {
    TempConfig t;
    CHECK(fs::write_file_atomic(t.path, json_string(golden(), "configJson")));
    CHECK_EQ(config::get_stored_token().value_or(""), std::string("ntn_fake_config_token_1234"));
    const auto s = config::notion_settings();
    CHECK_EQ(s.version.value_or(""), std::string("2022-06-28"));
    CHECK_EQ(s.min_interval_ms.value_or(-1), 350.0);
    // 書き直しても TS と同じ書式 (2 スペース・キー順を保つ・末尾改行) のまま。
    config::write_config(config::read_config());
    CHECK_EQ(t.raw(), json_string(golden(), "configJson"));
}

TEST_CASE("token を暗号化保存し、ファイルに平文を残さない") {
    TempConfig t;
    config::set_notion_token("ntn_topsecret");
    CHECK(t.raw().find("ntn_topsecret") == std::string::npos);
    CHECK(t.raw().find("tokenEnc") != std::string::npos);
    CHECK_EQ(config::get_stored_token().value_or(""), std::string("ntn_topsecret"));
    CHECK_THROWS(config::set_notion_token(""));
    config::clear_notion_token();
    CHECK(!config::get_stored_token().has_value());
}

TEST_CASE("resolve_notion_token は env を優先し、無ければ config") {
    TempConfig t;
    config::set_notion_token("from_config");
    CHECK_EQ(config::resolve_notion_token().value_or(""), std::string("from_config"));
    env::set_for_test("NOTION_TOKEN", "from_env");
    CHECK_EQ(config::resolve_notion_token().value_or(""), std::string("from_env"));
}

TEST_CASE("master 鍵が変わると保存済み token は読めない (nullopt)") {
    TempConfig t;
    config::set_notion_token("ntn_x");
    env::set_for_test("CANALIS_MASTER_KEY", "another-key");
    CHECK(!config::get_stored_token().has_value());
}

TEST_CASE("非シークレットは平文で保存し、env があれば env を優先する") {
    TempConfig t;
    config::set_notion_version("2099-01-01");
    config::set_notion_min_interval(500);
    CHECK(t.raw().find("2099-01-01") != std::string::npos);
    CHECK(t.raw().find("\"minIntervalMs\": 500") != std::string::npos);
    CHECK_EQ(config::notion_settings().version.value_or(""), std::string("2099-01-01"));
    env::set_for_test("NOTION_VERSION", "keep-me");
    env::set_for_test("NOTION_MIN_INTERVAL_MS", "1000");
    CHECK_EQ(config::notion_settings().version.value_or(""), std::string("keep-me"));
    CHECK_EQ(config::notion_settings().min_interval_ms.value_or(-1), 1000.0);
    CHECK_THROWS(config::set_notion_min_interval(-1));
}

TEST_CASE("get_config_status は token をマスクし、出所を示す") {
    TempConfig t;
    config::set_notion_token("ntn_abcd1234");
    const auto s = config::get_config_status();
    CHECK_EQ(s.token_source, std::string("config"));
    CHECK_EQ(s.token_hint.value_or(""), std::string("\xE2\x80\xA6" "1234"));
    const std::string shown = json_stringify(config::to_json(s));
    CHECK(shown.find("ntn_abcd1234") == std::string::npos);
    env::set_for_test("NOTION_TOKEN", "env_token_9999");
    CHECK_EQ(config::get_config_status().token_source, std::string("env"));
}

TEST_CASE("壊れた・大きすぎる・object でない config.json は空として扱う") {
    TempConfig t;
    CHECK(fs::write_file_atomic(t.path, "{ not json"));
    CHECK_EQ(config::read_config(), Json::object());
    CHECK(fs::write_file_atomic(t.path, "[1,2]"));
    CHECK_EQ(config::read_config(), Json::object());
    CHECK(fs::write_file_atomic(t.path, std::string((1u << 20) + 1, ' ')));
    CHECK_EQ(config::read_config(), Json::object());
    CHECK(!config::get_stored_token().has_value());
}

TEST_CASE("壊れた EncryptedBlob は判定で弾く") {
    CHECK(!config::is_encrypted_blob(Json::object()));
    CHECK(!config::is_encrypted_blob(Json{{"v", 2}, {"salt", "a"}, {"data", "b"}}));
    CHECK(!config::is_encrypted_blob(Json{{"v", 1}, {"salt", 1}, {"data", "b"}}));
    CHECK(config::is_encrypted_blob(Json{{"v", 1}, {"salt", "a"}, {"data", "b"}}));
}

CI_TEST_MAIN()
