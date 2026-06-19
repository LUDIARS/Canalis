# feature: notion-crawl CLI (standalone ① + raw 保存)

## 目的

②/③ のサービス固有コード無しで動く範囲 = **① Crawl + raw 保存**だけを提供する standalone エントリ。
Tirocinium の `scripts/notion-crawl` を置き換える。実体: `src/cli.ts`。

## 振る舞い

```
canalis notion-crawl <databaseId> [--out <dir>] [--max-depth N] [--max-pages N]
```

入力 = Notion database id + オプション →

1. `applyNotionConfigToEnv()` で非シークレット設定 (version / minInterval) を env へ反映。
2. `resolveNotionToken()` で token 解決 (env `NOTION_TOKEN` 優先 → config の暗号化トークン)。
   未設定なら exit 2 + 案内メッセージ。
3. `NotionSource.crawl({ databaseId, token, crawl:{maxDepth,maxPages} })` で `RawRecord[]` 取得。
4. `JsonlRawSink({ dir: <out>/notion/<databaseId> })` で JSONL 保存。

出力 = `<out>/notion/<databaseId>/<timestamp>.jsonl` (既定 `--out data/raw`)。
標準出力に `crawled <n> records → …` を表示。

## 引数

| 引数 | 既定 | 説明 |
|---|---|---|
| `<databaseId>` | (必須) | Notion database id。無ければ usage + exit 2 |
| `--out <dir>` | `data/raw` | 出力ルート。実配置は `<dir>/notion/<databaseId>/` |
| `--max-depth N` | (adapter 既定) | クロール深さ |
| `--max-pages N` | (adapter 既定) | 最大ページ数 |

## config サブコマンド

`canalis config …` で token 等の設定 UI を提供する → [interface/cli.md](../interface/cli.md) / [data/config-store.md](../data/config-store.md)。

## 制約 / 現状

- CLI は crawl + raw 保存のみ。②→③ を含むフルパイプラインは `runPipeline()` をプログラムから呼ぶ。
- 未知コマンドは `commands: notion-crawl, config` を stderr に出して exit 2。

関連: [feature/crawl-stage.md](./crawl-stage.md) / [interface/cli.md](../interface/cli.md)
