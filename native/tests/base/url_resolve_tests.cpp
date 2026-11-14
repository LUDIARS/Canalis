// canalis::url::resolve (new URL(ref, base).href 相当) のテスト。期待値は Node の new URL(ref, base).href で確かめた形。
#include "check.hpp"

#include "canalis/base/url.hpp"

using namespace canalis;

namespace {
std::string r(const char* base, const char* ref) { return url::resolve(base, ref).value_or("<nullopt>"); }
} // namespace

TEST_CASE("絶対パス・相対パス・ドット区間を解決する") {
    CHECK_EQ(r("https://example.com/a/b/c", "/about"), std::string("https://example.com/about"));
    CHECK_EQ(r("https://example.com/a/b/c", "d"), std::string("https://example.com/a/b/d"));
    CHECK_EQ(r("https://example.com/a/b/c", "./d"), std::string("https://example.com/a/b/d"));
    CHECK_EQ(r("https://example.com/a/b/c", "../d"), std::string("https://example.com/a/d"));
    CHECK_EQ(r("https://example.com/a/b/c", "../../../../d"), std::string("https://example.com/d"));
    CHECK_EQ(r("https://example.com/a/b/", "."), std::string("https://example.com/a/b/"));
    CHECK_EQ(r("https://example.com/", "a/./b/../c"), std::string("https://example.com/a/c"));
}

TEST_CASE("クエリ・ハッシュ・プロトコル相対・絶対 URL") {
    CHECK_EQ(r("https://example.com/a?x=1", "?y=2"), std::string("https://example.com/a?y=2"));
    CHECK_EQ(r("https://example.com/a?x=1", "#top"), std::string("https://example.com/a?x=1#top"));
    CHECK_EQ(r("https://example.com/a", "//cdn.example.com/x.js"), std::string("https://cdn.example.com/x.js"));
    CHECK_EQ(r("https://example.com/a", "HTTP://Other.COM:80/p"), std::string("http://other.com/p"));
    CHECK_EQ(r("https://example.com:8443/a", "b"), std::string("https://example.com:8443/b"));
}

TEST_CASE("http/https 以外・壊れた入力は nullopt") {
    CHECK(!url::resolve("https://example.com/", "mailto:a@example.com").has_value());
    CHECK(!url::resolve("https://example.com/", "javascript:alert(1)").has_value());
    CHECK(!url::resolve("not a url", "/x").has_value());
}

TEST_CASE("空白と非 ASCII は %XX にする") {
    CHECK_EQ(r("https://example.com/", "a b"), std::string("https://example.com/a%20b"));
    CHECK_EQ(r("https://example.com/", "/日本"), std::string("https://example.com/%E6%97%A5%E6%9C%AC"));
}

CI_TEST_MAIN()
