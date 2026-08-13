# feature: ① Crawl ステージ (取得元 adapter)

## 目的

取得元から生データを取得し、source 非依存の `RawRecord[]` へ正規化する汎用ステージ。
adapter (`Source` 実装) を足せば取得元が増える。実体は `src/crawl/<source>/`。

## 契約 (`Source`)

```ts
interface Source<C = unknown> {
  readonly name: string;                  // manifest の crawl.sources[].adapter と突合
  crawl(config: C): Promise<RawRecord[]>;
}
```

各 source は実 API/ブラウザを叩く部分を `fetch` / `api` / `fetcher` 等で**注入可能**にしており、
テストは fake で `①→RawRecord` 契約を固定する。`now` も注入可 (fetchedAt の決定性)。

## 実装済 adapter (実物)

| adapter `name` | 取得元 | 主な config | 認証 |
|---|---|---|---|
| `notion` | Notion DB クロール (Tr packages/notion 移植) | `databaseId` / `token?` / `crawl?{maxDepth,maxPages}` | integration token |
| `notion-public` | 公開 Notion ページ (Playwright レンダリング) | `url` / `options?{timeout,scrollDelay,maxScrolls}` | 不要 |
| `youtube` | YouTube Data API v3 コメントスレッド | `videoId` / `apiKey` / `maxResults?` / `textFormat?` | API キー |
| `reddit` | Reddit 公開 JSON API (posts / comments) | `subreddit?` / `postId?` / `limit?` / `sort?` / `mode?` | 不要 (User-Agent 必須) |
| `website` | 任意 URL の HTML 取得 → 構造化テキスト | `url` / `userAgent?` / `timeoutMs?` | 不要 |
| `discord` | Discord REST API v10 (チャンネル / ギルド) | `channelId` または `guildId` / `token?` / `options?` | Bot token |
| `steam-new-releases` | Steam store 検索の新作一覧 (リリース日降順、`results_html` を決定論パース) | `maxApps?` / `countryCode?` / `language?` | 不要 |
| `steam-app-reviews` | Steam appreviews API (cursor 全件、`stopAtRecommendationId` で増分打ち切り) | `appId` / `languages?` / `maxReviews?` | 不要 |
| `steam-user-reviews` | steamcommunity プロフィールの全レビュー (HTML 決定論パース、非公開は空) | `steamId` (SteamID64) / `maxPages?` | 不要 |

> `website` は `HtmlParser` を注入する口を持つ (Lector 接続用)。未注入時はタグ除去 + タイトル抽出の
> フォールバックパーサを使う。

### steam (新作一覧 / アプリ別レビュー / ユーザ別レビュー) (SPEC-CRAWL-STEAM)

3 系とも `fetchImpl` / `sleepImpl` / `now` 注入可・polite delay 既定 1200ms・API キー不要。
集計だけ欲しい場合は `fetchAppReviewSummary(appId)` (num_per_page=0) が `total_reviews` を
1 リクエストで返す (レビュー数閾値のフィルタ用)。ユーザ別レビューに公式 API は無いため
steamcommunity プロフィール `recommended` ページをマーカー切り出し + タグ除去で決定論パースする
(SteamID64 以外は fail-fast、非公開プロフィールは空配列)。SteamID64 は `meta.steamId` に保持し、
同一人物の横断同定 (ペルソナ元データ化) は利用側 ② (Discutere `steam-persona`) が行う。
外部から受ける app ID・ページ/件数上限・API filter は URL を組み立てる前に検証し、リダイレクトは
追跡しない。取得失敗時の例外には、SteamID64 を含むリクエスト URL を出力しない。

## 振る舞い

入力 = adapter 固有 config → 処理 = 取得元 API/ブラウザを叩いてページ送り等で全件取得 →
出力 = `RawRecord[]` (`source` / `sourceId` / `fetchedAt` / `text` / `raw` / `meta` を埋める)。

各 source の `raw` / `meta` への詰め方は [data/raw-record.md](../data/raw-record.md)、
外部エンドポイントの前提は [interface/external-sources.md](../interface/external-sources.md)。

## 制約 / 現状

- youtube / reddit / website / discord は Di 用に追加済。Lector を parse に組み込む口は未配線部あり。
- crawl はレート制限 / 元データ消失 / コスト高なので、raw 保存 → replay が実運用で効く ([feature/replay.md](./replay.md))。
