# interface: CLI 起動契約 (`canalis`)

bin = `dist/cli.js` (`package.json` の `bin.canalis`)。開発時は `npm run cli -- …` (= `tsx src/cli.ts`)。
正本: `src/cli.ts` + `src/config/cli.ts`。

## コマンド

```
canalis notion-crawl <databaseId> [--out <dir>] [--max-depth N] [--max-pages N]
canalis config <subcommand> ...
```

未知コマンドは stderr `commands: notion-crawl, config` + exit 2。

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
