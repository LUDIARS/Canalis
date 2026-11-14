#pragma once
// Chrome DevTools Protocol (CDP) のメッセージの送受 (1 メッセージ = 1 つの JSON 文字列)。
// 本物は BrowserProcess (--remote-debugging-pipe)。テストは偽の実装を差し込む。
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace canalis::cdp {

class Transport {
public:
    virtual ~Transport() = default;
    /** 1 メッセージ送る。相手がいない・書けないときは false。 */
    virtual bool send(std::string_view message) = 0;
    /** 次のメッセージを最大 timeout 待つ。期限切れ・相手が閉じたときは nullopt (closed() で区別する)。 */
    virtual std::optional<std::string> receive(std::chrono::milliseconds timeout) = 0;
    /** 相手が閉じた (以後 receive は何も返さない)。 */
    virtual bool closed() const = 0;
};

} // namespace canalis::cdp
