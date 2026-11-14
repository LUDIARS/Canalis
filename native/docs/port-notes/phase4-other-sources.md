# 段階 4: steam・discord・reddit・youtube

設計: `spec/plan/cpp-port.md` §9 の 4。

## 1. 機能の対応表

| TS | C++ | 備考 |
|---|---|---|
| `src/crawl/youtube/{types,source}.ts` | `native/src/canalis/crawl/youtube/source.*` | |
| `src/crawl/reddit/{types,source}.ts` | `native/src/canalis/crawl/reddit/source.*` | |
| `src/crawl/discord/types.ts` | `native/src/canalis/crawl/discord/types.hpp` | メッセージ・チャンネル・スレッドは JSON のまま |
| `src/crawl/discord/client.ts` | `native/src/canalis/crawl/discord/client.*` | 429 の再試行 (Retry-After 秒 → ms + 100、3 回まで) は同じ |
| `src/crawl/discord/{crawl,guild}.ts` | `native/src/canalis/crawl/discord/crawl.*` | `crawl_channel`・`crawl_guild` |
| `src/crawl/discord/{source,guild-source}.ts` | `native/src/canalis/crawl/discord/source.*` | `DiscordSource`・`DiscordGuildSource`・`messages_to_text` |
| `src/crawl/steam/{types,http}.ts` | `native/src/canalis/crawl/steam/{types,http}.*` | 同じホスト内 (https) だけの転送・polite delay・User-Agent |
| `src/crawl/steam/new-releases.ts` | `native/src/canalis/crawl/steam/new_releases.*` | `parse_search_results_html` は正規表現を使わずに同じ規則で書いた |
| `src/crawl/steam/app-reviews.ts` | `native/src/canalis/crawl/steam/app_reviews.*` | |
| `src/crawl/steam/user-reviews.ts` | `native/src/canalis/crawl/steam/user_reviews.*` | `parse_user_reviews_html` も同じく |
| `src/crawl/steam/source.ts` | `native/src/canalis/crawl/steam/source.*` | 3 つの Source |
| — | `native/src/canalis/crawl/{fetch,html_text}.*` | adapter 共通の取得・待機の差し替え口と、HTML の文字の取り出し (website も使う) |
| — | `native/src/cli/adapters.*` | `canalis crawl <adapter>` の adapter 名と JSON の config の対応 (10 個) |

## 2. テストの対応表

| TS テスト | ctest |
|---|---|
| `test/steam-source.test.ts` | `crawl.steam` (cursor の送り・増分の打ち切り・success!=1・入力の検査・集計・新作のページ送り・ユーザのレビュー・非公開・HTTP の境界・Source の形) |
| — (TS の正規表現との互換) | `crawl.steam` の golden。`native/tests/golden/steam_parse.gen.mjs` (TS と同じ正規表現、Node の組み込みだけ) で fixtures を作る。TS のテストの HTML と、境界の例 (閉じの無い span・最後の属性・review_box_content など) を入れた |
| `test/discord-source.test.ts`・`test/discord-guild-source.test.ts` | `crawl.discord` (+ client のヘッダ・パスの符号化・429・失敗の文言) |
| `test/crawl/reddit.test.ts`・`test/crawl/youtube.test.ts` | `crawl.reddit_youtube` |

このセッションでの結果 (Windows・Visual Studio 2022・Release): 18 件すべて通過。

## 3. 安全の対応表

| 防御 | C++ |
|---|---|
| Steam の転送 | 同じホストの https だけ、3 回まで。別のホスト・http・繰り返しは拒否し、エラー文に転送先を出さない |
| Steam の個人の識別子 | エラー文に URL を出さない (通信の失敗も種類だけを出す)。不正な SteamID64 の値もエラー文に出さない |
| Discord の token | Authorization ヘッダだけに入れる。転送を追わない。失敗の文言に token を出さない (テストで確かめる) |
| YouTube の API キー | URL のクエリに入るので、エラー文に URL を出さない (テストで確かめる) |
| 終わらないページ送り | Discord 10 万ページ・Steam のレビュー 10 万ページ・新作 1 万ページ・YouTube 1 万回で止める |
| Reddit の返信の入れ子 | 100 段まで (JSON の深さの上限 256 とは別に再帰を止める) |
| パスの注入 | Discord・Reddit の id / 名前はパスの 1 区間として符号化する (TS はそのまま連結) |
| 応答の大きさ | API の応答 1 件 32 MiB まで (`http_client` の既定は 16 MiB) |

## 4. 移していないもの・挙動の差

- Reddit / Discord の URL のパスで id・名前を符号化する (英数字の id なら TS と同じ URL)。
- YouTube: 応答に `items` が無いときは TS が例外になるが、C++ は 0 件として止める。
- Steam: `success` が文字列などのとき、エラー文の値の書式が JS と少し違う (`"1"` と `1`)。
- Steam の新作: `total_count` が null のとき、TS は 1 ページで止まり、C++ は続ける。

## 5. 土台に欲しいもの / 人間の判断が要ること

- なし。
