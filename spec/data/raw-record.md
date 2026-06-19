# data: RawRecord (①→② / ①→raw の受け渡しスキーマ)

①Crawl が出力し、②Transform と raw sink の両方が受け取る source 非依存の正規化生レコード。
永続化形態は JSONL (`JsonlRawSink` が 1 行 1 レコードで書き出す → §[data/raw-jsonl-store.md](./raw-jsonl-store.md))。

正本: `src/core/raw-record.ts` の `RawRecord`。

## フィールド

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `source` | `string` | ✅ | 取得元の種別。`"notion"` / `"notion-public"` / `"youtube"` / `"reddit"` / `"website"` / `"discord"` |
| `sourceId` | `string` | ✅ | source 内で一意な ID (dedup キー)。例: Notion page id / YouTube comment thread id / Reddit fullname (`t3_…`/`t1_…`) / Discord message・channel・thread id / website は `hostname+pathname` |
| `fetchedAt` | `string` | ✅ | 取得時刻 (ISO8601 / UTC)。source 内で同一バッチは同一値 |
| `url` | `string?` | | 元 URL |
| `title` | `string?` | | タイトル / 見出し |
| `text` | `string?` | | 正規化した本文テキスト (Markdown 等)。②の主入力 |
| `raw` | `unknown` | ✅ | source ネイティブの原データを verbatim 保持 (replay / 監査用) |
| `meta` | `Record<string, unknown>?` | | source 固有の補助情報 |

## source 別 raw / meta の中身 (実装裏取り)

各 ①adapter の `crawl()` が詰める内容。`raw` は replay の根拠なので原データを丸ごと持つ。

| source | `sourceId` | `raw` | `meta` の主キー |
|---|---|---|---|
| `notion` | page id | クロール結果ページ (properties 含む) | `kind` / `parentId` / `depth` / `databaseId` / `properties` / `truncated` |
| `notion-public` | ページ URL | `ExtractionResult` (`{title, blocks}`) | `url` / `blockCount` |
| `youtube` | comment thread id | `YouTubeCommentThread` | `videoId` / `authorDisplayName` / `authorChannelId` / `likeCount` / `replyCount` / `publishedAt` / `updatedAt` |
| `reddit` (posts) | `RedditPost.name` (`t3_…`) | `RedditPost` | `subreddit` / `author` / `score` / `upvote_ratio` / `num_comments` / `created_utc` |
| `reddit` (comments) | `RedditComment.name` (`t1_…`) | `RedditComment` | `subreddit` / `author` / `score` / `created_utc` / `parent_id` / `link_id` |
| `website` | `hostname+pathname` (ASCII 化, 200 字) | `{ html, url }` | `links` / `originalUrl` |
| `discord` | message id (チャンネル) / channel・thread id (guild) | `DiscordMessage` 等 | チャンネル/スレッド種別等 |

> 制約: `raw` は JSON シリアライズ可能であること (JSONL 永続化と replay 読み戻しの前提)。

関連: [feature/crawl-stage.md](../feature/crawl-stage.md) / [interface/external-sources.md](../interface/external-sources.md)
