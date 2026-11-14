#include "canalis/crawl/notion_public/fetcher.hpp"

#include <algorithm>
#include <stdexcept>
#include <thread>

#include "canalis/base/strings.hpp"
#include "canalis/base/time.hpp"
#include "canalis/base/url.hpp"
#include "canalis/crawl/notion_public/extract.hpp"
#include "canalis/infra/http_client.hpp"

namespace canalis::crawl::notion_public {
namespace {

using std::chrono::milliseconds;

constexpr std::string_view kContentSelectorCheck = "!!document.querySelector('.notion-page-content')";
constexpr std::int64_t kSelectorPollMs = 100;
constexpr std::int64_t kFinalRenderWaitMs = 500;

void sleep_ms(std::int64_t ms) {
    if (ms > 0) std::this_thread::sleep_for(milliseconds(ms));
}

bool istarts_with(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && str::iequals_ascii(s.substr(0, prefix.size()), prefix);
}

/** Playwright の waitForSelector(state: 'attached') と同じく、要素が DOM に現れるまで待つ。 */
bool wait_for_content(cdp::Page& page, std::int64_t timeout_ms) {
    const auto deadline = time::steady_ms() + timeout_ms;
    while (true) {
        const auto left = deadline - time::steady_ms();
        if (left <= 0) return false;
        if (page.evaluate(std::string(kContentSelectorCheck), milliseconds(left)) == Json(true)) return true;
        sleep_ms(std::min(kSelectorPollMs, deadline - time::steady_ms()));
    }
}

/** スクロールで lazy-load のブロックを展開する (高さが変わらなくなるか max_scrolls 回まで)。最後に先頭へ戻す。 */
void scroll_to_bottom(cdp::Page& page, const FetchPageOptions& options) {
    const milliseconds timeout(options.timeout);
    const std::string height = "document.documentElement.scrollHeight";
    for (int i = 0; i < options.max_scrolls; ++i) {
        const Json prev = page.evaluate(height, timeout);
        page.evaluate("window.scrollTo(0, document.documentElement.scrollHeight)", timeout);
        sleep_ms(options.scroll_delay);
        if (page.evaluate(height, timeout) == prev) break;
    }
    page.evaluate("window.scrollTo(0, 0)", timeout);
}

} // namespace

bool is_bot_challenge_title(std::string_view title) {
    const std::string t = str::trim(title);
    return istarts_with(t, "just a moment") || istarts_with(t, "attention required") ||
           str::starts_with(t, "しばらくお待ちください");
}

bool is_allowed_page_url(std::string_view text) {
    const auto u = url::parse(text);
    if (!u || (u->scheme != "http" && u->scheme != "https")) return false;
    if (u->host == "localhost" || str::ends_with(u->host, ".localhost")) return false;
    // IP アドレスの文字列なら公開アドレスか見る。ホスト名ならブラウザに任せる。
    const bool looks_ip = u->host.find(':') != std::string::npos ||
                          u->host.find_first_not_of("0123456789.") == std::string::npos;
    return !looks_ip || http::is_public_address(u->host);
}

BrowserFetcher::BrowserFetcher(cdp::BrowserOptions options, bool allow_private_network)
    : options_(std::move(options)), allow_private_network_(allow_private_network) {}

BrowserFetcher::~BrowserFetcher() { close(); }

cdp::Browser& BrowserFetcher::browser() {
    if (!browser_) browser_ = std::make_unique<cdp::Browser>(options_);
    return *browser_;
}

FetchedPage BrowserFetcher::fetch(const std::string& url, const FetchPageOptions& options) {
    if (!allow_private_network_ && !is_allowed_page_url(url))
        throw std::runtime_error("notion-public: この URL は取得しません (http/https の公開ホストのみ): " + url);
    const milliseconds timeout(options.timeout);
    auto page = browser().new_page(timeout);

    // Notion は通信を張り続けるので、ネットワークが静かになるのは待たない。HTML の読み込み後に本文要素を待つ。
    page->navigate(url, timeout);
    if (!wait_for_content(*page, options.timeout)) {
        // Cloudflare のボット確認画面に当たったときは、タイムアウトと区別できるよう明示する。
        std::string title;
        try {
            const Json t = page->evaluate("document.title", milliseconds(5000));
            if (t.is_string()) title = t.get<std::string>();
        } catch (const std::exception&) {
            // タイトルが取れなくても、元の期限切れとして報告する
        }
        if (is_bot_challenge_title(title))
            throw std::runtime_error("Notion のボット確認画面で止められました (title=\"" + title + "\"): " + url);
        throw std::runtime_error("waiting for .notion-page-content timed out (" + std::to_string(options.timeout) +
                                 "ms): " + url);
    }

    scroll_to_bottom(*page, options);
    sleep_ms(kFinalRenderWaitMs);  // 最終レンダリング待ち

    ExtractionResult result = extraction_from_json(page->evaluate(std::string(extract_page_content_expression()), timeout));
    FetchedPage fetched;
    fetched.url = url;
    fetched.title = result.title;
    fetched.markdown = blocks_to_markdown(result.blocks);
    fetched.raw = std::move(result);
    return fetched;
}

void BrowserFetcher::close() {
    if (!browser_) return;
    browser_->close();
    browser_.reset();
}

} // namespace canalis::crawl::notion_public
