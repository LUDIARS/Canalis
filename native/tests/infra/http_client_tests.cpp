// 外部への HTTP クライアント (canalis::http::fetch) のテスト。
// 本物の外部サービスには繋がない。loopback に偽のサーバ (固定の応答を返すだけ) を立てて確かめる。
// 出自: Discutere native tests/infra/http_tests.cpp のクライアント側の観点 (サーバ側は Canalis に無いので Asio で偽のサーバを書いた)。
#include "check.hpp"

#include <atomic>
#include <string>
#include <thread>

#include <boost/asio.hpp>

#include "canalis/infra/http_client.hpp"

using namespace canalis;
namespace asio = boost::asio;

namespace {

/** 受けた要求の 1 行目でパスを見て、決まった応答を返す偽のサーバ (要求ごとに接続を閉じる)。 */
class FakeServer {
public:
    FakeServer() : acceptor_(io_, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0)) {
        thread_ = std::thread([this] { serve(); });
    }
    ~FakeServer() {
        stopping_ = true;
        boost::system::error_code ec;
        // accept 待ちを起こすために自分へ 1 回つなぐ。
        asio::ip::tcp::socket wake(io_);
        wake.connect(acceptor_.local_endpoint(), ec);
        thread_.join();
    }
    std::string base() const { return "http://127.0.0.1:" + std::to_string(acceptor_.local_endpoint().port()); }

private:
    void serve() {
        while (!stopping_) {
            asio::ip::tcp::socket socket(io_);
            boost::system::error_code ec;
            acceptor_.accept(socket, ec);
            if (ec || stopping_) return;
            asio::streambuf buf;
            asio::read_until(socket, buf, "\r\n\r\n", ec);
            if (ec) continue;
            std::string head(asio::buffers_begin(buf.data()), asio::buffers_end(buf.data()));
            const auto sp1 = head.find(' ');
            const auto sp2 = head.find(' ', sp1 + 1);
            const std::string path = head.substr(sp1 + 1, sp2 - sp1 - 1);
            std::string status = "200 OK";
            std::string body = "{\"status\":\"ok\"}";
            if (path == "/missing") {
                status = "404 Not Found";
                body = "nope";
            }
            const std::string response = "HTTP/1.1 " + status + "\r\nContent-Type: application/json\r\nContent-Length: " +
                                         std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
            asio::write(socket, asio::buffer(response), ec);
            socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        }
    }

    asio::io_context io_;
    asio::ip::tcp::acceptor acceptor_;
    std::atomic<bool> stopping_{false};
    std::thread thread_;
};

http::ClientRequest local(const std::string& url) {
    http::ClientRequest r;
    r.url = url;
    r.allow_private_network = true;
    r.timeout = std::chrono::milliseconds(5000);
    r.connect_timeout = std::chrono::milliseconds(2000);
    return r;
}

} // namespace

TEST_CASE("既定では loopback へ出ない (SSRF 対策)") {
    FakeServer server;
    http::ClientRequest blocked;
    blocked.url = server.base() + "/health";
    const auto b = http::fetch(blocked);
    CHECK(!b.transport_ok);
    CHECK(b.error_kind == http::FetchErrorKind::blocked);
}

TEST_CASE("許可したときは取得でき、状態・本文・上限を扱う") {
    FakeServer server;
    const auto g = http::fetch(local(server.base() + "/health"));
    CHECK(g.ok());
    CHECK_EQ(g.body, std::string("{\"status\":\"ok\"}"));
    CHECK_EQ(g.headers.get("content-type").value_or(""), std::string("application/json"));

    const auto missing = http::fetch(local(server.base() + "/missing"));
    CHECK(missing.transport_ok);
    CHECK_EQ(missing.status, 404);
    CHECK_EQ(missing.reason, std::string("Not Found"));

    auto limited = local(server.base() + "/health");
    limited.max_response_bytes = 5;
    const auto l = http::fetch(limited);
    CHECK(!l.transport_ok);
    CHECK(l.error_kind == http::FetchErrorKind::too_large);
}

TEST_CASE("壊れた URL・userinfo つきの URL は送らない") {
    CHECK(http::fetch(local("not a url")).error_kind == http::FetchErrorKind::invalid_url);
    CHECK(http::fetch(local("ftp://example.com/")).error_kind == http::FetchErrorKind::invalid_url);
    CHECK(http::fetch(local("http://user:pass@127.0.0.1/")).error_kind == http::FetchErrorKind::invalid_url);
}

TEST_CASE("公開アドレスの判定") {
    CHECK(!http::is_public_address("127.0.0.1"));
    CHECK(!http::is_public_address("10.1.2.3"));
    CHECK(!http::is_public_address("172.20.0.1"));
    CHECK(!http::is_public_address("192.168.0.1"));
    CHECK(!http::is_public_address("169.254.169.254"));
    CHECK(!http::is_public_address("100.64.0.1"));
    CHECK(!http::is_public_address("::1"));
    CHECK(!http::is_public_address("fd00::1"));
    CHECK(!http::is_public_address("::ffff:10.0.0.1"));
    CHECK(http::is_public_address("8.8.8.8"));
    CHECK(http::is_public_address("2606:4700::1111"));
}

CI_TEST_MAIN()
