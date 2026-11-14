# 段階 3: notion (公式 API)・raw JSONL sink・CLI

設計: `spec/plan/cpp-port.md` §3 と §9 の 3。CLI の引数・出力・終了コードは `spec/interface/cli.md`。

## 1. 機能の対応表

| TS | C++ | 備考 |
|---|---|---|
| `src/crawl/notion/types.ts` | `native/src/canalis/crawl/notion/types.*` | block / page は TS と同じく JSON のまま緩く持つ |
| `src/crawl/notion/url.ts` | `native/src/canalis/crawl/notion/url.*` | 正規表現は使わず線形の走査で同じ規則を実装した |
| `src/crawl/notion/{blocks,page}.ts` | `native/src/canalis/crawl/notion/blocks.*` | `String(x)` (formula・number) は JS と同じ書式 |
| `src/crawl/notion/client.ts` | `native/src/canalis/crawl/notion/client.*` | 最小間隔・429 / 5xx の再試行 (Retry-After を優先、無ければ 1s・2s・4s… 最大 10s) は同じ |
| `src/crawl/notion/{crawl,source}.ts` | `native/src/canalis/crawl/notion/crawl.*` | `crawl_database`・`crawl_page`・`NotionSource` |
| `src/save/raw-writer.ts` | `native/src/canalis/save/raw_writer.*` | `JsonlRawSink`・読み戻し。CLI の notion-crawl が使う分を先に移した (ほかの sink は段階 5) |
| `src/cli.ts`・`src/config/cli.ts` | `native/src/cli/{commands,main}.*` | `notion-crawl`・`config` は同じ引数・出力。`crawl`・`notion-public-crawl`・`website-parse` を足した |

CLI に足した 3 つは、TS / Node の利用者が実行ファイルを子プロセスで呼ぶための口 (D1 で TS 版を退役する前提)。
`website-parse` は取得をせず、標準入力の HTML を解析するだけにした。
Consult の fetch-link のように、取得先を自分の規則 (信頼するサイトの一覧) で絞っている利用者が、取得はそのままで解析だけを移せるため。

## 2. テストの対応表

| TS テスト | ctest |
|---|---|
| `test/notion-source.test.ts` | `crawl.notion` (RawRecord への変換・databaseId 必須。raw の JSON の形も固定) |
| `test/notion-page-crawl.test.ts` (crawlPage・Notion URL・findNotionUrls) | `crawl.notion` (同じ観点。`app.notion.com/p/<id>` も) |
| — (client) | `crawl.notion`: ヘッダ・本文・パスの符号化・429 の Retry-After と指数の待ち・失敗の文言に token が出ないこと |
| — (blocks / page) | `crawl.notion`: 主なブロックと properties の文字列 |
| — (raw sink) | `save.raw_writer`: 書式 (1 行 1 件・末尾改行・stamp の ":" → "-")・読み戻し・名前順の連結・壊れた行 |
| — (CLI) | `cli.cli`: 未知のコマンド・引数の誤り・token 無し・config の set / show / path / unset・パイプと端末からの token・website-parse・SSRF の遮断。実行ファイルを起動して終了コードと UTF-8 の入出力も確かめる |

このセッションでの結果 (Windows・Visual Studio 2022・Release): 15 件すべて通過。

## 3. 安全の対応表

| 防御 | C++ |
|---|---|
| token を出さない | Authorization ヘッダだけに入れる。失敗の文言は `notion <METHOD> <path> failed: <status> <本文の先頭 200 文字>` で、token を含まない (テストで確かめる) |
| API の転送 | 転送を追わない (`max_redirects = 0`)。Authorization を別の場所へ持っていかない |
| 応答の大きさ | 1 応答 16 MiB まで。JSON は上限つきで読む |
| 終わらないページネーション | 1 つの一覧で 10,000 回 (100 万件) を超えたら失敗にする |
| 深い入れ子 | 1 ページ内の展開は 64 段まで |
| パスの注入 | id はパスの 1 区間として符号化する (TS はそのまま連結) |
| CLI の入力 | `--config` の JSON は 1 MiB まで、標準入力の HTML は 64 MiB まで、token は 64 KiB まで |

## 4. 移していないもの・挙動の差

- `NotionApiClient` の 1 回の要求に期限 (既定 60 秒) を付けた (TS の fetch は無期限)。
- `NotionSource` が token から client を作るとき、Notion-Version と最小間隔は `config::notion_settings()` (env が優先、無ければ config) から取る。TS は env だけを見る (CLI は先に config を env へ写すので、CLI の挙動は同じ)。
- `parse_notion_page_id`: 最後の区間の `decodeURIComponent` が失敗したとき、TS は例外になるが C++ は「id 無し」を返す。
- CLI: `--max-depth` / `--max-pages` が整数でなければ usage + exit 2 (TS は NaN のまま進む)。
- CLI: 未知のコマンドの一覧に、C++ 版で足したコマンドが入る。
- Windows の CLI は引数をワイド文字から UTF-8 で受け取り、出力も UTF-8 のバイト列のまま書く。

## 5. 土台に欲しいもの / 人間の判断が要ること

- 段階 6 で Consult の fetch-link を切り替えるとき: 公開 Notion は `notion-public-crawl`、信頼するサイトは「Consult が取得 → `website-parse`」で置き換える想定。
