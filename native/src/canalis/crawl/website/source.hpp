#pragma once
// ①Crawl adapter: URL から HTML を取得し RawRecord へ変換する (src/crawl/website/{types,source}.ts の移植)。
// HTML → 構造化テキストの変換は HtmlParser を注入する。未注入なら fallback_parse (タグ除去 + タイトル抽出) を使う。
//
// TS との違い: 取得先が private network (loopback・社内アドレス等) なら取得しない (http_client の SSRF 対策)。
// URL は利用者が指定するため。テストなどでローカルを取るときは deps.allow_private_network を true にする。
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "canalis/core/pipeline.hpp"
#include "canalis/infra/http_client.hpp"

namespace canalis::crawl::website {

struct WebsiteSourceConfig {
    std::string url;
    /** User-Agent (既定: Canalis-Crawler/1.0)。 */
    std::optional<std::string> user_agent;
    /** タイムアウト ms (既定 10000)。 */
    std::optional<std::int64_t> timeout_ms;
};

struct ParsedPage {
    std::optional<std::string> title;
    /** 本文テキスト (Markdown 等)。 */
    std::string text;
    /** 同一ドメイン等のリンク一覧。 */
    std::vector<std::string> links;
};

/** HTML → 構造化テキストへ変換するパーサの口 (Lector 等を注入する)。 */
class HtmlParser {
public:
    virtual ~HtmlParser() = default;
    virtual ParsedPage parse(const std::string& html, const std::string& url) = 0;
};

/** タグ除去 + タイトル抽出の最小実装 (TS の fallbackParser と同じ規則。正規表現は使わず線形に走査する)。 */
ParsedPage fallback_parse(std::string_view html, std::string_view url);

/** URL を安定した ASCII の ID にする (hostname + pathname、[A-Za-z0-9._/-] 以外は _、200 文字まで)。 */
std::string to_source_id(std::string_view url);

struct WebsiteSourceDeps {
    /** 差し替え用の取得関数 (既定は canalis::http::fetch)。 */
    std::function<http::ClientResponse(const http::ClientRequest&)> fetch;
    /** fetchedAt の時刻源 (既定は現在時刻の ISO8601)。 */
    std::function<std::string()> now;
    std::shared_ptr<HtmlParser> html_parser;
    /** private network への取得を許す (テスト用。既定 false)。 */
    bool allow_private_network = false;
    /** 応答本文の上限 (既定 16 MiB)。 */
    std::size_t max_response_bytes = 16u << 20;
};

class WebsiteSource final : public core::Source<WebsiteSourceConfig> {
public:
    explicit WebsiteSource(WebsiteSourceDeps deps = {});
    std::string_view name() const override { return "website"; }
    /** 失敗 (url 未指定・通信の失敗・HTTP 2xx 以外) は std::runtime_error。 */
    std::vector<core::RawRecord> crawl(const WebsiteSourceConfig& config) override;

private:
    WebsiteSourceDeps deps_;
};

} // namespace canalis::crawl::website
