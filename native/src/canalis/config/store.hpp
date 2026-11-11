#pragma once
// Canalis 設定ストア (src/config/store.ts の移植)。ファイルの形式・解決順は TS 版と同じ。
//   - 保存先は env CANALIS_CONFIG_PATH、無ければ作業ディレクトリ直下の config.json。
//   - 非シークレット (notion.version / notion.minIntervalMs) は平文、notion.token だけ暗号化 (config/crypto)。
//   - master 鍵は env CANALIS_MASTER_KEY、無ければマシン束縛値 `canalis:<hostname>:<username>`。
//   - crawl が使う token は env NOTION_TOKEN を優先し、無ければ config の暗号化 token を復号して使う。
//
// TS との違い: applyNotionConfigToEnv (process.env へ書き込む) の代わりに notion_settings() で値を返す。
// C++ 版の Notion クライアントは env を読まず、この値を引数で受け取るため。解決順 (env が優先) は同じ。
#include <optional>
#include <string>

#include "canalis/base/json.hpp"

namespace canalis::config {

/** 保存先: env CANALIS_CONFIG_PATH → 作業ディレクトリ直下 config.json。 */
std::string config_path();

/** config.json を読む。未存在・壊れている・object でなければ空 object。 */
Json read_config();

/** config.json を書く (JSON.stringify(cfg, null, 2) + "\n")。親ディレクトリは作る。失敗は std::runtime_error。 */
void write_config(const Json& cfg);

/** notion.token を暗号化して保存。空文字は std::invalid_argument。 */
void set_notion_token(const std::string& token);
/** notion.token を消す。 */
void clear_notion_token();
/** Notion-Version (平文) を保存。 */
void set_notion_version(const std::string& version);
/** API 呼び出しの最小間隔 ms (平文) を保存。負・非有限は std::invalid_argument。 */
void set_notion_min_interval(double ms);

/** config の暗号化 token を復号して返す。未設定・復号失敗 (master 鍵が変わった等) は nullopt。 */
std::optional<std::string> get_stored_token();

/** crawl が使う token: env NOTION_TOKEN (空文字でも設定されていれば優先) → config。 */
std::optional<std::string> resolve_notion_token();

struct NotionSettings {
    std::optional<std::string> version;
    std::optional<double> min_interval_ms;
};

/** Notion クライアントへ渡す非シークレット設定。env (NOTION_VERSION / NOTION_MIN_INTERVAL_MS) が優先、無ければ config。 */
NotionSettings notion_settings();

/** `config show` の表示内容。token の平文は持たない。 */
struct ConfigStatus {
    std::string path;
    std::string token_source;  // "env" | "config" | "none"
    std::optional<std::string> token_hint;  // "…" + 末尾 4 文字
    std::optional<std::string> version;
    std::optional<double> min_interval_ms;
};

ConfigStatus get_config_status();

/** TS の getConfigStatus と同じキー (path, tokenSource, tokenHint, version, minIntervalMs) の object。 */
Json to_json(const ConfigStatus& status);

} // namespace canalis::config
