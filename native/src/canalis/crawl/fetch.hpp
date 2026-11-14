#pragma once
// ①Crawl adapter が共通で使う取得・待機の差し替え口と、応答の JSON の読み取り。
// TS の adapter は `fetch` / `sleep` を deps で受け取るので、C++ も同じく関数で受け取る (テストで偽物にする)。
#include <chrono>
#include <functional>
#include <string>
#include <thread>

#include "canalis/base/json.hpp"
#include "canalis/infra/http_client.hpp"

namespace canalis::crawl {

using FetchFn = std::function<http::ClientResponse(const http::ClientRequest&)>;
using SleepFn = std::function<void(std::chrono::milliseconds)>;

inline http::ClientResponse default_fetch(const http::ClientRequest& request) { return http::fetch(request); }
inline void default_sleep(std::chrono::milliseconds d) {
    if (d.count() > 0) std::this_thread::sleep_for(d);
}

/** 外部 API の応答 1 件を読む上限 (ページネーションの 1 ページ分)。 */
inline JsonLimits api_json_limits() {
    JsonLimits limits;
    limits.max_bytes = 32u << 20;
    limits.max_elements = 4'000'000;
    limits.max_depth = 256;  // Reddit のコメントの返信は深く入れ子になる
    return limits;
}

/** 本文を JSON として読む。壊れていれば `<label>: invalid JSON response` の std::runtime_error。 */
inline Json parse_response_json(const http::ClientResponse& res, const std::string& label) {
    std::string error;
    auto json = parse_json(res.body, api_json_limits(), &error);
    if (!json) throw std::runtime_error(label + ": invalid JSON response (" + error + ")");
    return *json;
}

} // namespace canalis::crawl
