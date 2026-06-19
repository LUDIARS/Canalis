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

> `website` は `HtmlParser` を注入する口を持つ (Lector 接続用)。未注入時はタグ除去 + タイトル抽出の
> フォールバックパーサを使う。

## 振る舞い

入力 = adapter 固有 config → 処理 = 取得元 API/ブラウザを叩いてページ送り等で全件取得 →
出力 = `RawRecord[]` (`source` / `sourceId` / `fetchedAt` / `text` / `raw` / `meta` を埋める)。

各 source の `raw` / `meta` への詰め方は [data/raw-record.md](../data/raw-record.md)、
外部エンドポイントの前提は [interface/external-sources.md](../interface/external-sources.md)。

## 制約 / 現状

- youtube / reddit / website / discord は Di 用に追加済。Lector を parse に組み込む口は未配線部あり。
- crawl はレート制限 / 元データ消失 / コスト高なので、raw 保存 → replay が実運用で効く ([feature/replay.md](./replay.md))。
