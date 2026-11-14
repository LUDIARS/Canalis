# 段階 2: notion-public (ブラウザ) と website

設計: `spec/plan/cpp-port.md` §4 と §9 の 2。Consult の fetch-link が使う 2 つの取得を先に移す。

## 1. 機能の対応表

| TS | C++ | 備考 |
|---|---|---|
| `src/core/raw-record.ts` | `native/src/canalis/core/raw_record.*` | JSON のキー順は TS の adapter が組むオブジェクトと同じ。値の無い項目は出さない |
| `src/core/pipeline.ts` の `Source` | `native/src/canalis/core/pipeline.hpp` | `template <class Config> class Source`。Transform / Sink は段階 5 |
| `src/crawl/website/{types,source}.ts` | `native/src/canalis/crawl/website/source.*` | fallbackParser は正規表現を使わず線形の走査で同じ規則を実装した |
| `src/crawl/notion-public/types.ts` | `native/src/canalis/crawl/notion_public/types.*` | |
| `src/crawl/notion-public/extract.ts` | `native/src/canalis/crawl/notion_public/extract.*` | `extractPageContent` はブラウザで動く JS なので、型注釈だけを外して埋め込んだ。`blocksToMarkdown` は C++ |
| `src/crawl/notion-public/fetcher.ts` (`PlaywrightFetcher`) | `native/src/canalis/crawl/notion_public/fetcher.*` (`BrowserFetcher`) | 手順は同じ (DOMContentLoaded → 本文要素を待つ → スクロール → 500ms → 抽出)。ボット確認画面の明示エラーも同じ |
| `src/crawl/notion-public/{source,crawl}.ts` | `native/src/canalis/crawl/notion_public/source.*` | `NotionPublicSource` と `crawl_public_pages` |
| playwright (chromium.launch・newPage・goto・waitForSelector・evaluate) | `native/src/canalis/infra/{browser_process,cdp_client,browser}.*` | 導入済みの Edge / Chrome を `--remote-debugging-pipe` で動かす (D2)。ポートは開けない |
| `new URL(ref, base).href` | `canalis::url::resolve` / `encode_unsafe` / `Url::href` | WHATWG URL の完全な実装ではない (IDNA・バックスラッシュは扱わない) |

ブラウザの扱い:
- 探す順は env `CANALIS_BROWSER` → Edge → Chrome / Chromium の決まった場所。`CANALIS_BROWSER` を指定したのに無いときは、別のブラウザで黙って続けない。
- 毎回一時プロファイル (`<temp>/canalis-browser-<乱数>`) で起動し、閉じるときにプロセス (Windows は Job object、それ以外はプロセスグループ) とプロファイルを消す。
- 拡張・同期・背景通信・コンポーネント更新・フィッシング検知などを止める引数で起動する (`cdp::launch_args`)。
- Windows では子の fd 3 / 4 を CRT の fd 継承 (`STARTUPINFO.lpReserved2`) で渡す。Node の child_process と同じ方式。

## 2. テストの対応表

| TS テスト | ctest |
|---|---|
| `test/crawl/website.test.ts` | `crawl.website` (同じ 5 件 + User-Agent・期限・転送後の URL・SSRF) |
| — (TS の fallbackParser との互換) | `crawl.website` の golden。`native/tests/golden/website_fallback.gen.mjs` (TS と同じ正規表現、Node の組み込みだけ) で fixtures を作る |
| `test/notion-public-source.test.ts` | `crawl.notion_public` (RawRecord の形・url 必須・blocksToMarkdown) |
| `test/notion-public-crawl.test.ts` (crawlPublicPages・isBotChallengeTitle) | `crawl.notion_public` (同じ 5 件 + ボット確認の判定)。Notion URL (`isNotionUrl`) は段階 3 の notion と一緒に移す |
| — | `core.raw_record`・`base.url_resolve` (期待値は Node の `new URL` で確かめた) |
| — (CDP) | `infra.cdp_client`: 台本どおりに応答する偽のブラウザで、要求と応答の対応・イベント・error・期限切れ・切断・上限・Page の手順を確かめる |
| — (本物のブラウザ) | `infra.browser_e2e`: `CANALIS_BROWSER_E2E=1` のときだけ動く。loopback の偽のページだけを取り、`--proxy-server` で外への通信を塞ぐ |

このセッションでの結果 (Windows・Visual Studio 2022・Release、Edge): 12 件すべて通過。`infra.browser_e2e` を `CANALIS_BROWSER_E2E=1` で実行し、3 / 3 通過した。終了後にブラウザのプロセスと一時プロファイルが残っていないことも確かめた。

## 3. 安全の対応表

| 防御 | C++ |
|---|---|
| website の SSRF | 既定の取得は private network (loopback・社内アドレス等) へ出ない (`http_client`)。TS 版には無かった |
| notion-public の SSRF | http / https で、ホストが localhost や private の IP アドレスなら取らない (`is_allowed_page_url`)。TS 版には無かった。ホスト名の名前解決の先・ブラウザ内の転送はブラウザに任せる |
| ブラウザとの接続 | ポートを開けない (pipe)。1 メッセージ 32 MiB・溜めた受信 64 MiB を超えたら切断する。JSON は上限つきで読む |
| 抽出結果の大きさ | ブロック 10 万件・リンク 1 万件を超えたら失敗にする |
| ブラウザの後始末 | 例外でも RAII でタブ・ブラウザ・プロファイルを閉じる。強制終了は子プロセスごと |
| 応答の大きさ (website) | `max_response_bytes` 既定 16 MiB |

## 4. 移していないもの・挙動の差

- Playwright の `waitForSelector` は DOM の変化を見張るが、C++ は 100ms ごとに `document.querySelector` を評価して待つ。
- website: 通信の失敗の文言は `WebsiteSource: <理由> <url>` (TS は fetch の例外のまま)。
- website: 期限 (`timeoutMs`) は全体の期限として渡す。接続の期限はそれより長くしない。
- notion-public: ブラウザの切断後は、同じ `BrowserFetcher` で続けて取得すると失敗する (TS も同じく起動に失敗したとき以外は作り直さない)。
- `BrowserFetcher` の `Page` は `Browser` より長く使わない (内部で同じ接続を指すため)。

## 5. 土台に欲しいもの / 人間の判断が要ること

- 段階 6 で Consult の fetch-link を C++ 版へ切り替えるには、段階 3 の CLI (`canalis crawl <adapter>`) が要る。
