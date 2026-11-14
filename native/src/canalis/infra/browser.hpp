#pragma once
// 導入済みの Chromium 系ブラウザ (Edge / Chrome) をヘッドレスで動かす (Playwright の chromium.launch の代わり)。
// spec/plan/cpp-port.md §4 (D2: 導入済みのブラウザを CDP pipe で使う。Chromium はダウンロードしない)。
// - ブラウザは毎回一時プロファイルで起動し、close() でプロセスとプロファイルを消す。
// - 拡張・同期・バックグラウンド通信・コンポーネント更新を止めて起動する。
// スレッド安全ではない (1 つの Browser は 1 つのスレッドから使う)。
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "canalis/base/json.hpp"
#include "canalis/infra/cdp_client.hpp"
#include "canalis/infra/cdp_transport.hpp"

namespace canalis::cdp {

/** 使うブラウザを探す: env CANALIS_BROWSER (実行ファイルのパス) → Edge → Chrome / Chromium の決まった場所。無ければ nullopt。 */
std::optional<std::string> find_browser();

/** 見つかったブラウザの起動引数 (--remote-debugging-pipe は BrowserProcess が足す)。テスト用に公開。 */
std::vector<std::string> launch_args(const std::string& profile_dir, const std::vector<std::string>& extra_args);

struct BrowserOptions {
    /** 空なら find_browser() で探す。 */
    std::string executable;
    /** 追加の引数 (テストで通信先を絞るとき等)。 */
    std::vector<std::string> extra_args;
    /** 起動して最初の応答が来るまでの期限。 */
    std::chrono::milliseconds startup_timeout{20'000};
};

class Browser;

/** 1 つのタブ (CDP のターゲット + flatten セッション)。Browser より長く使わない (同じ接続を指すため)。 */
class Page {
public:
    /** Browser だけが作れる鍵 (passkey)。コンストラクタを public にして std::make_unique で作れるようにする (生の new を書かない)。 */
    class Key {
        friend class Browser;
        Key() = default;
    };
    Page(Key, Client& client, std::string target_id, std::string session_id);
    /** url へ移動し、DOMContentLoaded まで待つ (Playwright の waitUntil: 'domcontentloaded')。失敗は CdpError。 */
    void navigate(const std::string& url, std::chrono::milliseconds timeout);
    /** 式をページ内で評価し、値を JSON で返す (returnByValue)。例外は CdpError。 */
    Json evaluate(const std::string& expression, std::chrono::milliseconds timeout);
    /** タブを閉じる。何度呼んでもよい。 */
    void close();
    ~Page();
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;

private:
    Client& client_;
    std::string target_id_;
    std::string session_id_;
    bool closed_ = false;
};

class Browser {
public:
    /** 起動する。ブラウザが見つからない・起動できないときは std::runtime_error。 */
    explicit Browser(BrowserOptions options = {});
    /** 偽の transport で動かす (テスト用。プロセスもプロファイルも作らない)。 */
    explicit Browser(std::unique_ptr<Transport> transport);
    ~Browser();
    Browser(const Browser&) = delete;
    Browser& operator=(const Browser&) = delete;

    /** about:blank のタブを開く。 */
    std::unique_ptr<Page> new_page(std::chrono::milliseconds timeout);
    /** ブラウザを閉じ、一時プロファイルを消す。何度呼んでもよい。 */
    void close();

private:
    std::unique_ptr<Transport> transport_;
    std::unique_ptr<Client> client_;
    std::string profile_dir_;
    bool closed_ = false;
};

} // namespace canalis::cdp
