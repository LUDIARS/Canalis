// 本物のブラウザ (導入済みの Edge / Chrome) を CDP pipe で動かす結合テスト。CANALIS_BROWSER_E2E=1 のときだけ動かす。
// - 取るのは loopback に立てた偽のサーバの「Notion 風」ページだけ。
// - ブラウザの外への通信は --proxy-server を使われていないポートへ向けて塞ぐ (loopback は proxy を通らない)。
#include "check.hpp"

#include <atomic>
#include <thread>

#include <boost/asio.hpp>

#include "canalis/base/env.hpp"
#include "canalis/crawl/notion_public/fetcher.hpp"
#include "canalis/crawl/notion_public/source.hpp"
#include "canalis/infra/browser.hpp"

using namespace canalis;
namespace asio = boost::asio;

namespace {

const char* kPage = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>E2E ページ | Notion</title></head>
<body>
<div class="notion-page-content">
  <div data-block-id="b1" class="notion-header-block">見出し</div>
  <div data-block-id="b2" class="notion-text-block">本文の段落です。</div>
  <div data-block-id="b3" class="notion-bulleted_list-block">箇条書き</div>
  <div data-block-id="b4" class="notion-divider-block"></div>
  <div data-block-id="b5" class="notion-text-block">
    <a href="/Child-0123456789abcdef0123456789abcdef?pvs=4">子ページ</a>
    <a href="https://other.example/Ext-0123456789abcdef0123456789abcdef">外部</a>
  </div>
</div>
<script>
  // 遅れて描画されるブロック (lazy-load の代わり)
  setTimeout(() => {
    const el = document.createElement('div');
    el.setAttribute('data-block-id', 'b6');
    el.className = 'notion-quote-block';
    el.textContent = '引用';
    document.querySelector('.notion-page-content').appendChild(el);
  }, 50);
</script>
</body></html>)HTML";

const char* kChallenge = "<!DOCTYPE html><html><head><title>Just a moment...</title></head><body>checking</body></html>";

class PageServer {
public:
    PageServer() : acceptor_(io_, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0)) {
        thread_ = std::thread([this] { serve(); });
    }
    ~PageServer() {
        stopping_ = true;
        boost::system::error_code ec;
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
            const std::string head(asio::buffers_begin(buf.data()), asio::buffers_end(buf.data()));
            const auto sp1 = head.find(' ');
            const std::string path = head.substr(sp1 + 1, head.find(' ', sp1 + 1) - sp1 - 1);
            std::string body = path.rfind("/challenge", 0) == 0 ? kChallenge : kPage;
            std::string status = "200 OK";
            if (path == "/favicon.ico") {
                status = "404 Not Found";
                body.clear();
            }
            const std::string res = "HTTP/1.1 " + status + "\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " +
                                    std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
            asio::write(socket, asio::buffer(res), ec);
            socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        }
    }
    asio::io_context io_;
    asio::ip::tcp::acceptor acceptor_;
    std::atomic<bool> stopping_{false};
    std::thread thread_;
};

bool enabled() {
    if (env::get("CANALIS_BROWSER_E2E").value_or("") != "1") {
        std::cout << "skip: CANALIS_BROWSER_E2E=1 で有効にする\n";
        return false;
    }
    return true;
}

cdp::BrowserOptions offline_options() {
    cdp::BrowserOptions o;
    o.extra_args = {"--proxy-server=http://127.0.0.1:9"};  // 外への通信を塞ぐ
    return o;
}

} // namespace

TEST_CASE("本物のブラウザで Notion 風のページを取り出す") {
    if (!enabled()) return;
    PageServer server;
    crawl::notion_public::BrowserFetcher fetcher(offline_options(), /*allow_private_network=*/true);
    crawl::notion_public::FetchPageOptions options;
    options.timeout = 20'000;
    options.scroll_delay = 200;
    options.max_scrolls = 3;
    const auto page = fetcher.fetch(server.base() + "/Root-ffffffffffffffffffffffffffffffff", options);
    fetcher.close();
    std::cout << page.markdown << "\n";
    CHECK_EQ(page.title, std::string("E2E ページ"));
    CHECK_EQ(page.markdown, std::string("# 見出し\n\n本文の段落です。\n\n- 箇条書き\n\n---\n\n子ページ 外部\n\n> 引用"));
    CHECK(page.raw.links.has_value());
    CHECK_EQ(page.raw.links->size(), std::size_t{1});
    CHECK_EQ(page.raw.links->at(0), server.base() + "/Child-0123456789abcdef0123456789abcdef");
}

TEST_CASE("ボット確認画面は明示のエラーにする") {
    if (!enabled()) return;
    PageServer server;
    crawl::notion_public::BrowserFetcher fetcher(offline_options(), true);
    crawl::notion_public::FetchPageOptions options;
    options.timeout = 1500;
    try {
        fetcher.fetch(server.base() + "/challenge", options);
        CHECK(false);
    } catch (const std::runtime_error& e) {
        CHECK(std::string(e.what()).find("ボット確認画面") != std::string::npos);
    }
}

TEST_CASE("既定では loopback のページを取らない") {
    if (!enabled()) return;
    crawl::notion_public::BrowserFetcher fetcher(offline_options());
    CHECK_THROWS(fetcher.fetch("http://127.0.0.1:1/x", {}));
}

CI_TEST_MAIN()
