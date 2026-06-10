# Canalis

> ラテン語で「管・水路」。LUDIARS 共有**データ取込パイプライン基盤**。

外部データを **① crawl/scrape → ② clean/categorize → ③ save/import** の 3 ステージで
取り込む。各ステージは型 (契約) で疎結合に接続され、サービスは **マニフェスト**で流れを宣言する。

- **① Crawl** — 取得元 adapter (汎用)。現状 `NotionSource` (Tirocinium から移植)。
- **② Clean** — サービス内部仕様への整形・分類 (**サービス固有**、各リポに置く)。
- **③ Save** — 保存先 writer (汎用)。`raw(jsonl)` / `postgres` / `kuzu`。

## 設計原則

- **LLM をできる限り排除する。** 共有 lib は LLM 依存ゼロ。整形は決定論 (セレクタ/正規表現/
  パーサ/辞書/ルール) を第一とし、LLM はサービス ②の中の opt-in に限る。詳細は `DESIGN.md §0.1`。
- **整形の知識は ②に集約、③は verbatim 書込。** ③writer は DB エンジン 1 種につき 1 個で全サービス共有。
- **共有 lib は 1 サービス専用ロジックを持たない。** ②は各サービスリポ。

## 使い方 (現 scaffold)

```bash
npm install
npm run build
npm test          # fake で ①→② 契約を検証

# Notion token を暗号化して config.json に保存 (env を使わない設定 UI)
npm run cli -- config set notion.token            # 対話入力 (echo 非表示)
#   または: echo "$NOTION_TOKEN" | npm run cli -- config set notion.token
npm run cli -- config show                        # token はマスク表示

# ① crawl + raw 保存だけの standalone (Tr scripts/notion-crawl 置換)
npm run cli -- notion-crawl <databaseId> --out data/raw
```

トークンは `config.json` (リポ直下・gitignore) に **salt 付き AES-256-GCM** で暗号化保存される
(Excubitor と同方式)。master 鍵は env `CANALIS_MASTER_KEY` → 無ければマシン束縛値 (hostname+user)。
非シークレット (`notion.version` / `notion.minIntervalMs`) は平文。env `NOTION_TOKEN` があればそちらを優先。

| `config` サブコマンド | 用途 |
|---|---|
| `config set notion.token [value]` | token を暗号化保存 (value 省略で非表示入力 / パイプ可) |
| `config set notion.version <v>` | Notion-Version (平文) |
| `config set notion.minIntervalMs <ms>` | レート間隔 ms (平文) |
| `config unset notion.token` | token 削除 |
| `config show` | 設定一覧 (token マスク) |
| `config path` | config.json のパス |

プログラムからフルパイプライン:

```ts
import { runPipeline, NotionSource, PostgresSink, JsonlRawSink } from '@ludiars/canalis';

const report = await runPipeline(manifest, {
  sources: { notion: new NotionSource() },
  transforms: { 'tr:company-normalize': myTransform },  // ② はサービス側
  sinks: {
    jsonl: new JsonlRawSink({ dir: 'data/raw/tr' }),
    postgres: new PostgresSink(mySqlExecutor),
  },
});
```

詳細・契約・マニフェスト仕様は **`DESIGN.md`**。

## ステータス

scaffold (v0.1)。① Notion adapter 移植済 / 契約・runner・③writer 骨組み済 / 実 DB 結合テスト未。
残タスクは `DESIGN.md §6`。
