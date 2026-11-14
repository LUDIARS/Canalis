#pragma once
// Notion 公式 REST API クライアント (src/crawl/notion/client.ts の移植)。
// databases.query / blocks.children / pages.retrieve を叩く。Notion のレート制限 (~3 req/s) に合わせた最小間隔と、
// 429 / 5xx のリトライ (Retry-After を優先、無ければ 1s, 2s, 4s … 最大 10s)。
// token はログ・エラー文に出さない。
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "canalis/crawl/notion/types.hpp"
#include "canalis/infra/http_client.hpp"

namespace canalis::crawl::notion {

struct NotionClientConfig {
    /** Notion integration token (secret_xxx / ntn_xxx)。空は std::invalid_argument。 */
    std::string token;
    /** Notion-Version ヘッダ。既定 2022-06-28。 */
    std::optional<std::string> notion_version;
    /** 既定 https://api.notion.com/v1 (末尾の / は落とす)。 */
    std::optional<std::string> base_url;
    /** API 呼び出しの最小間隔 ms (既定 350 ≒ 3 req/s)。 */
    std::optional<std::int64_t> min_interval_ms;
    /** 429 / 5xx の最大リトライ回数 (既定 4)。 */
    std::optional<int> max_retries;
    /** 1 回の要求の期限 (既定 60 秒)。TS には無い (fetch の既定は無期限)。 */
    std::chrono::milliseconds timeout{60'000};
    /** private network の base_url を許す (テストの偽のサーバ用)。 */
    bool allow_private_network = false;
    /** 差し替え用 (テスト)。既定は canalis::http::fetch。 */
    std::function<http::ClientResponse(const http::ClientRequest&)> fetch;
    /** 差し替え用 (テスト)。既定は std::this_thread::sleep_for。 */
    std::function<void(std::chrono::milliseconds)> sleep;
};

class NotionApiClient final : public NotionApi {
public:
    explicit NotionApiClient(NotionClientConfig config);
    Paged query_database(const std::string& database_id, const std::optional<std::string>& cursor) override;
    Paged get_block_children(const std::string& block_id, const std::optional<std::string>& cursor) override;
    Json retrieve_page(const std::string& page_id) override;

private:
    Json request(const std::string& method, const std::string& path, const std::optional<Json>& body);
    void throttle();

    NotionClientConfig config_;
    std::string version_;
    std::string base_url_;
    std::int64_t min_interval_ms_;
    int max_retries_;
    std::int64_t last_at_ = 0;
};

/** env NOTION_TOKEN (・NOTION_VERSION・NOTION_MIN_INTERVAL_MS) から作る。token が無ければ nullptr。 */
std::unique_ptr<NotionApiClient> create_notion_client_from_env();

} // namespace canalis::crawl::notion
