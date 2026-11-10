# plan: Canalis 本体の C++ 化

依頼: neco 2026-11-11「Consult の Canalis を C++ 化する」→ 範囲は「Canalis 本体を C++ 化」(Consult セッション lictor-5842be82 で確認)。
基点: Canalis main `eeb033a`。前例: Discutere native (job `cpp-node-20261030`、`Discutere/native/docs/PORTING.md`・`INVENTORY.md`)。

この文書は設計の正本。§8 の判断は決まった。実装は §9 の段階ごとに PR を出す。

## 1. 目的

- Node.js / npm 依存 (package-lock 212 件) と、Playwright が別途ダウンロードする Chromium をなくす。
  Discutere の C++ 化と同じで、狙いは脆弱性の受け口と余分な依存を減らすこと。
- 取得 (①) の挙動・`RawRecord` / `SinkBatch` の JSON の形・CLI の引数と終了コード・config ファイルの形式は変えない。
  移植前のデータ (raw JSONL・暗号化済み config) は移植後もそのまま読める。

目的にしないもの: 機能の追加。取得元の仕様変更への追従は、移植とは別の PR にする。

## 2. 現状の棚卸し (TS 4,164 行・テスト 17 ファイル / 約 108 件)

| 範囲 | 行 | Node / npm への依存 | C++ での扱い |
|---|---|---|---|
| core (契約・pipeline・manifest) | 222 | js-yaml (manifest-loader) | 移す。manifest は JSON にする (§8 D3) |
| config (暗号化 config・CLI) | 371 | node:crypto (scrypt + AES-256-GCM)、TTY 入力 | 移す。OpenSSL の EVP (scrypt N=16384,r=8,p=1 = Node の既定) で同じ形式を読み書きする |
| crawl/notion (公式 API) | 700 | fetch | 移す |
| crawl/notion-public (公開ページ) | 428 | **playwright** (goto・waitForSelector・evaluate・スクロール) | 移す。ブラウザは §4 |
| crawl/website | 142 | fetch (HtmlParser は注入) | 移す |
| crawl/steam | 590 | fetch・同じホストに限った転送 | 移す (転送の制限はそのまま) |
| crawl/discord | 502 | fetch (REST) | 移す |
| crawl/reddit・youtube | 296 | fetch | 移す |
| llm (opt-in のローカル LLM・語彙・採点) | 332 | fetch | 移す (別ターゲット `canalis_llm`、core から参照しない原則を保つ) |
| save (raw / postgres / kuzu / ft) | 343 | fs・child_process | 移す。DB ドライバは今と同じく注入 (§3) |
| runner・cli | 201 | process | 移す |

## 3. 形 (配布物と使い方)

C++20 の静的ライブラリと実行ファイルにする。名前空間は `canalis::<ディレクトリ>`。

```
native/                      (移行中は TS の src/ と並べる。TS を退役したら直下へ)
  CMakeLists.txt
  src/canalis/{core,config,crawl/<adapter>,llm,save,runner}/   ← src/ を写す
  src/canalis/base/          http クライアント・JSON 上限・時刻・ログ・子プロセス・CDP
  src/cli/main.cpp           実行ファイル `canalis`
  tests/                     ctest (TS テストの移し替え + golden)
```

- **C++ の利用者** (Discutere native): submodule の `add_subdirectory` で `canalis::core` などをリンクする。
  Source / Transform / Sink は抽象クラス、SqlExecutor / CypherExecutor / FtRunner / LlmExecutor は `std::function` で注入する (今の DI をそのまま写す)。
- **TS / Node の利用者** (Tirocinium・Consult fetch-link・Discutere sidecar): 実行ファイルを子プロセスで呼ぶ。
  新しいサブコマンドを足す:
  - `canalis crawl <adapter> --config <JSON | @file>`: RawRecord を 1 行 1 件の JSON で標準出力へ出す。
    adapter は notion・notion-public・website・steam・discord・reddit・youtube。
  - `canalis save <adapter> --config …`: 標準入力の SinkBatch / RawRecord を書く。
    書けるのは raw (jsonl) と ft だけで、postgres / kuzu は利用者側が書く。DB ドライバを持たない今の原則のまま。
  - 既存の `notion-crawl`・`config` は引数・出力・終了コードを変えない (spec/interface/cli.md)。
- Transform (②) は今と同じく各サービスに置く。Canalis はサービス固有の処理を持たない。

## 4. ブラウザ (notion-public)

Playwright の代わりに、Chrome DevTools Protocol を `--remote-debugging-pipe` (WebSocket を使わず、標準入出力の pipe) で話す小さなクライアントを書く。

- ブラウザは導入済みの Chromium 系を使う。探す順は `CANALIS_BROWSER` (実行ファイルのパス)、Edge、Chrome。Windows には Edge が標準で入っている。
  Chromium の自動ダウンロードはしない。
- 使う命令は `Target.createTarget`・`Page.navigate`・`Page.lifecycleEvent` (DOMContentLoaded)・`Runtime.evaluate` (セレクタ待ち・スクロール・`extract.ts` の抽出)・`Target.closeTarget` だけ。
- `extract.ts` はブラウザの中で動く JS なので、移植せずに JS のままバイナリへ埋め込む (`Runtime.evaluate` に渡す)。
- ブラウザの起動は一時プロファイル・`--headless=new`・拡張なし。終了時にプロセスとプロファイルを消す。
- ボット確認画面の扱い (`db99a03` で明示エラーにした挙動) と期限はそのまま移す。

## 5. 依存 (Discutere native と揃える)

| 依存 | 用途 | 固定 |
|---|---|---|
| Boost (Asio / Beast、ヘッダのみ) | HTTPS クライアント | URL_HASH SHA256 |
| OpenSSL 3.x | TLS・AES-256-GCM・scrypt | システム (Discutere と同じ) |
| nlohmann_json | JSON (`ordered_json` でキー順を保つ) | URL_HASH SHA256 |
| Lorica | 入力の上限・hardening フラグ | commit 固定 |

Playwright・js-yaml (D3 で JSON にする)・kuzu (devDep)・tsx・vitest・typescript は不要になる。取り込むときは external-import-review の手順で検査記録を作る。

## 6. 安全

- 外部入力 (HTTP 応答・CDP の応答・子プロセス出力・JSONL・config・manifest) は、確保の前に上限をかける (Lorica の input_budget)。
- website / notion-public の URL は利用者が指定するので、private network への接続を拒否する (SSRF)。steam の転送は同じホストに限る、を保つ。
- token (Notion・Discord) はログ・エラー文・標準出力に出さない。config の表示は今の「末尾 4 文字のマスク」のまま。
- CDP の pipe は自分が起動したブラウザだけにつなぐ。ポートは開けない。
- 規約: AIFormat の C++ 規約 (RULE_CODE_CPP) と Discutere の PORTING.md (std::regex を使わない・非同期を同期にする・上限必須など) に従う。

## 7. テストと互換性

- vitest の 17 ファイルを同じ観点で `native/tests/<モジュール>_tests.cpp` に移す (1 ファイル = 1 ctest)。
- golden: TS 実装の入出力 (記録した HTTP 応答 → RawRecord、暗号化 config の往復、manifest の読み込み) を fixtures に書き出し、C++ が同じ JSON を出すことを確かめる。
- 本物の Notion / Discord / Steam などには繋がない。偽のサーバか記録した応答を使う。CDP はテスト用の偽ブラウザ (pipe の相手) で確かめる。
- 移行中の CI は TS と C++ の両方を通す。

## 8. 決めたこと (neco 2026-11-11、すべて推奨案)

| # | 内容 | 決定 |
|---|---|---|
| D1 | TS 版 `@ludiars/canalis` の扱い | 利用者を実行ファイル呼び出しへ移し終えたら TS を消し、npm の publish も止める。移行中だけ TS と C++ を並べる |
| D2 | notion-public のブラウザ | 導入済みの Edge / Chrome を CDP pipe で使う。Chromium はダウンロードしない |
| D3 | manifest の書式 | JSON にする。YAML の依存は足さない。既存の YAML manifest は移行時に JSON へ変換する |
| D4 | 利用者への配布 | GitHub Release に OS 別のビルド済みバイナリを付ける (`eeb033a` の bundle 添付を置き換える) |
| D5 | Discutere の sidecar | この移植の後、Discutere sidecar の Notion クロールを C++ ライブラリに置き換える (Discutere 側の別 PR) |

## 9. 段階 (1 段階 = 1 PR)

1. 土台: CMake・依存の固定と検査記録・base (http・JSON の上限・時刻・ログ・子プロセス)・config (暗号化 config の互換)。
2. notion-public (CDP クライアント) と website。Consult の fetch-link が使う部分なので先に移す。
3. notion (公式 API) と CLI の `notion-crawl`・`config`・`crawl`。
4. steam・discord・reddit・youtube。
5. core の runner・manifest、save、llm。
6. 利用者の切り替え: Consult fetch-link (Node の `import` を実行ファイル呼び出しへ)・Tirocinium・Discutere。それぞれの利用者のリポで PR を出す。
7. TS の退役 (D1)、配布 (D4)、README / spec の更新。

段階 2 が終われば、Consult は Canalis の npm 依存と Playwright なしで Notion 公開ページを取れる。
Consult `_source/setup.mjs` の Canalis 部分 (npm ci・build・Chromium の導入) は、段階 6 で実行ファイルの用意に置き換える。
