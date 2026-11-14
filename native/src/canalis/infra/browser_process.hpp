#pragma once
// Chromium 系ブラウザ (Edge / Chrome) を --remote-debugging-pipe で起動し、CDP のメッセージを送受する。
// - ポートは開けない。子の fd 3 (ブラウザが読む) と fd 4 (ブラウザが書く) に pipe をつなぐ。
//   Windows は CRT の fd 継承 (STARTUPINFO.lpReserved2) で fd 3 / 4 を渡す (Node の child_process と同じ方式)。
// - メッセージは NUL 区切り。1 メッセージ・溜まっている受信の合計に上限をかけ、超えたら接続を切る。
// - 子は Windows では Job object、それ以外ではプロセスグループに入れ、close() で必ず止める。
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "canalis/infra/cdp_transport.hpp"

namespace canalis::cdp {

struct LaunchOptions {
    /** ブラウザの実行ファイル (絶対パス)。 */
    std::string executable;
    /** --remote-debugging-pipe 以外の引数 (呼び出し側が組む)。 */
    std::vector<std::string> args;
    /** 1 メッセージの上限。 */
    std::size_t max_message_bytes = 32u << 20;
    /** 読んだがまだ受け取られていないメッセージの合計の上限。 */
    std::size_t max_pending_bytes = 64u << 20;
};

class BrowserProcess final : public Transport {
public:
    /** 起動する。失敗は std::runtime_error。 */
    explicit BrowserProcess(const LaunchOptions& options);
    ~BrowserProcess() override;
    BrowserProcess(const BrowserProcess&) = delete;
    BrowserProcess& operator=(const BrowserProcess&) = delete;

    bool send(std::string_view message) override;
    std::optional<std::string> receive(std::chrono::milliseconds timeout) override;
    bool closed() const override;

    /** 子が自分で終わるのを grace まで待ち、終わらなければ強制終了する。何度呼んでもよい。 */
    void close(std::chrono::milliseconds grace = std::chrono::milliseconds(3000));

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace canalis::cdp
