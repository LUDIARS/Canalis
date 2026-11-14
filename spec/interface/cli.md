# interface: CLI 起動契約 (`canalis`)

bin = `dist/cli.js` (`package.json` の `bin.canalis`)。開発時は `npm run cli -- …` (= `tsx src/cli.ts`)。
正本: `src/cli.ts` + `src/config/cli.ts`。
C++ 版 (移行中、spec/plan/cpp-port.md): `native/` をビルドした `build/bin/canalis` (正本 `native/src/cli/`)。

## コマンド

```
canalis notion-crawl <databaseId> [--out <dir>] [--max-depth N] [--max-pages N]
canalis config <subcommand> ...
canalis crawl <website|notion-public|notion> --config <JSON | @file>            (C++ 版のみ)
canalis notion-public-crawl <url> [--max-depth N] [--max-pages N] [--timeout ms] [--scroll-delay ms] [--max-scrolls N]  (C++ 版のみ)
canalis website-parse --url <url>   (HTML は標準入力、C++ 版のみ)
```

未知コマンドは stderr `commands: notion-crawl, config` + exit 2 (TS 版)。
C++ 版 (`native/`、spec/plan/cpp-port.md) は `commands: notion-crawl, config, crawl, notion-public-crawl, website-parse` + exit 2。

### C++ 版で足したサブコマンド

TS / Node の利用者が実行ファイルを子プロセスで呼ぶための口 (spec/plan/cpp-port.md §3)。出力は UTF-8 の JSON。

| コマンド | 標準出力 | exit |
|---|---|---|
| `crawl <adapter> --config <JSON \| @file>` | `RawRecord` を 1 行 1 件の JSON。config は adapter の TS の Config と同じキー (website: `url` / `userAgent` / `timeoutMs`、notion-public: `url` / `options{timeout, scrollDelay, maxScrolls}`、notion: `databaseId` / `token` / `crawl{maxDepth, maxPages, includeChildDatabases}`) | 0 成功 / 2 引数の誤り・未知の adapter / 1 取得の失敗 (`canalis: <message>`) |
| `notion-public-crawl <url> ...` | `crawlPublicPages` の結果 (`rootUrl` / `pages` / `errors` / `truncated`) を 1 行の JSON | 0 (ページごとの失敗は `errors`) / 2 引数の誤り |
| `website-parse --url <url>` | 標準入力の HTML を fallback パーサで解析した `{title?, text, links}` を 1 行の JSON (取得はしない。利用者が自分の取得規則で取った HTML を渡す) | 0 / 2 引数の誤り / 1 入力が 64 MiB を超えた |

C++ 版の挙動の差 (詳細は `native/docs/port-notes/`):
- `--max-depth` / `--max-pages` が整数でなければ usage + exit 2 (TS は NaN のまま進む)。
- website と notion-public は、既定で private network (loopback・社内アドレス等) のページを取得しない。

### `notion-crawl`

→ [feature/notion-crawl-cli.md](../feature/notion-crawl-cli.md)。出力 `<out>/notion/<databaseId>/<stamp>.jsonl`。

| exit | 条件 |
|---|---|
| 0 | クロール成功 |
| 2 | databaseId 欠落 / token 未設定 |
| 1 | 例外 (`canalis: <message>` を stderr) |

### `config` サブコマンド (設定 UI)

token は暗号化保存、非シークレットは平文 ([data/config-store.md](../data/config-store.md))。

| サブコマンド | 用途 |
|---|---|
| `config set notion.token [value]` | token を暗号化保存 (value 省略で非表示入力 / `echo … \| …` でパイプ可) |
| `config set notion.version <v>` | Notion-Version (平文) |
| `config set notion.minIntervalMs <ms>` | レート間隔 ms (平文) |
| `config unset notion.token` | token 削除 |
| `config show` | 設定一覧 (token は末尾 4 文字マスク表示、出所 env/config/none) |
| `config path` | config.json のパス表示 |

## 環境変数

- `NOTION_TOKEN` — あれば config の暗号化トークンより優先。
- `CANALIS_CONFIG_PATH` / `CANALIS_MASTER_KEY` — config 保存先 / 復号 master 鍵 ([setup/configuration.md](../setup/configuration.md))。
- `NOTION_VERSION` / `NOTION_MIN_INTERVAL_MS` — `applyNotionConfigToEnv()` が config から注入 (既存は上書きしない)。

## プログラム API (フルパイプライン)

CLI は ① + raw のみ。②→③ を含む実行は `runPipeline(manifest, deps)` を import して呼ぶ
([feature/manifest.md](../feature/manifest.md))。公開エントリは [setup/installation.md](../setup/installation.md) の exports 表。
