#pragma once
// Steam の adapter 共通の型と、Steam のエンドポイント向けの小さな HTTP (src/crawl/steam/{types,http}.ts の移植)。
// 責務は User-Agent の明示・polite delay・同じホスト内 (https) だけの転送。
// エラー文には URL を出さない (steamcommunity の URL には SteamID64 = 個人の識別子が入るため)。
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "canalis/base/json.hpp"
#include "canalis/crawl/fetch.hpp"

namespace canalis::crawl::steam {

inline constexpr std::string_view kSteamUserAgent = "LUDIARS-Canalis-Crawler/0.1 (+https://github.com/LUDIARS/Canalis)";

/** 全 Steam adapter 共通の HTTP オプション。 */
struct SteamHttpOptions {
    /** 差し替え用。既定は canalis::http::fetch。 */
    FetchFn fetch;
    /** 同じドメインへの最小の間隔 ms。既定 1200。負は std::invalid_argument。 */
    std::optional<std::int64_t> polite_delay_ms;
    /** 差し替え用。既定は std::this_thread::sleep_for。 */
    SleepFn sleep;
};

class SteamHttp {
public:
    explicit SteamHttp(const SteamHttpOptions& options);
    Json get_json(const std::string& url) const;
    std::string get_text(const std::string& url) const;
    void polite_wait() const;

private:
    http::ClientResponse get(const std::string& url) const;
    FetchFn fetch_;
    SleepFn sleep_;
    std::int64_t delay_ms_;
};

} // namespace canalis::crawl::steam
