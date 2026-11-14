#pragma once
// 導入済みのブラウザ (CDP) を使った公開 Notion ページの取得 (src/crawl/notion-public/fetcher.ts の PlaywrightFetcher の代わり)。
// ブラウザは初回の fetch で起動して使い回す (複数ページの取得を速くする)。使い終わったら close()。
// テストでは PageFetcher を差し替えて、ブラウザを起動しない。
#include <memory>
#include <string>
#include <string_view>

#include "canalis/crawl/notion_public/types.hpp"
#include "canalis/infra/browser.hpp"

namespace canalis::crawl::notion_public {

/** Cloudflare のボット確認画面のタイトルか (先頭一致、英字は大文字小文字を無視)。 */
bool is_bot_challenge_title(std::string_view title);

/**
 * 取得してよい URL か: http / https で、ホストが localhost・private や loopback の IP アドレスでないこと。
 * TS 版には無い検査 (C++ 版で足した SSRF 対策)。名前解決した先の検査はブラウザに任せるので、ここでは文字列で分かる範囲だけを見る。
 */
bool is_allowed_page_url(std::string_view url);

class BrowserFetcher final : public PageFetcher {
public:
    explicit BrowserFetcher(cdp::BrowserOptions options = {}, bool allow_private_network = false);
    ~BrowserFetcher() override;
    FetchedPage fetch(const std::string& url, const FetchPageOptions& options) override;
    void close() override;

private:
    cdp::Browser& browser();
    cdp::BrowserOptions options_;
    bool allow_private_network_;
    std::unique_ptr<cdp::Browser> browser_;
};

} // namespace canalis::crawl::notion_public
