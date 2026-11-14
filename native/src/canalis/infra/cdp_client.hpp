#pragma once
// Chrome DevTools Protocol の要求・応答・イベント (Playwright の代わりに使う小さな実装)。
// - 要求 {id, method, params, sessionId?} を送り、同じ id の応答を待つ。待つ間に届いたイベントは溜めておく。
// - セッションは flatten (Target.attachToTarget の flatten: true) で、メッセージの sessionId で振り分ける。
// - 受け取る JSON は parse_json の上限つきで読む。応答の error は CdpError にする。
// スレッド安全ではない (1 つのクライアントは 1 つのスレッドから使う)。
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "canalis/base/json.hpp"
#include "canalis/infra/cdp_transport.hpp"

namespace canalis::cdp {

/** CDP の応答の error、または接続の失敗・期限切れ。 */
class CdpError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Client {
public:
    /** transport は Client より長く生きること。 */
    explicit Client(Transport& transport, JsonLimits limits = default_limits());

    /** 要求を送り、応答の result を返す。期限切れ・切断・error は CdpError。 */
    Json call(std::string_view method, const Json& params, std::chrono::milliseconds timeout,
              std::string_view session_id = {});

    /** method (と sessionId) が一致するイベントを最大 timeout 待ち、その params を返す。溜めたイベントから先に探す。 */
    std::optional<Json> wait_event(std::string_view method, std::string_view session_id, std::chrono::milliseconds timeout);

    /** 溜めたイベントのうち、指定のセッションのものを捨てる (ページを閉じたあと等)。 */
    void drop_events(std::string_view session_id);

    /** ブラウザ側の 1 メッセージは大きくなり得る (ページの本文を返す evaluate)。 */
    static JsonLimits default_limits() { return JsonLimits{32u << 20, 2'000'000, 128}; }

private:
    /** 次のメッセージを 1 つ読み、イベントなら溜め、応答ならそれを返す。期限切れは nullopt。 */
    std::optional<Json> pump(std::chrono::milliseconds timeout);

    Transport& transport_;
    JsonLimits limits_;
    std::int64_t next_id_ = 1;
    std::deque<Json> events_;
    static constexpr std::size_t kMaxQueuedEvents = 2000;
};

} // namespace canalis::cdp
