# interface: 外部取得元 (① crawler が叩く外部 contract)

①Crawl adapter が叩く外部ソースのエンドポイント・認証・前提。実体は `src/crawl/<source>/`。
各 adapter は `fetch` / `api` / `fetcher` を注入可能にしており、テストは fake で実通信せず契約検証する。

## notion (Notion API)

- adapter: `NotionSource` (`src/crawl/notion/`、Tr packages/notion 移植)。
- 認証: integration token (`config.token` / env `NOTION_TOKEN`)。`Notion-Version` ヘッダは env `NOTION_VERSION`。
- config: `{ databaseId, token?, crawl?: { maxDepth?, maxPages? } }`。
- 前提: database id を起点に再帰クロール。`crawl.maxDepth` / `maxPages` で抑制。

## notion-public (公開 Notion ページ / Playwright)

- adapter: `NotionPublicSource` (`PlaywrightFetcher` でブラウザレンダリング)。token 不要。
- config: `{ url, options?: { timeout=30000, scrollDelay=800, maxScrolls=15 } }`。
- 前提: Playwright (Chromium) 実行環境が要る ([setup/installation.md](../setup/installation.md))。スクロールで遅延ロードを取り込む。

## youtube (YouTube Data API v3)

- base: `https://www.googleapis.com/youtube/v3` (`commentThreads`)。
- 認証: API キー (`config.apiKey`)。
- config: `{ videoId, apiKey, maxResults=100, textFormat='plainText' }`。
- 前提: `part=snippet`、`nextPageToken` でページ送りし `maxResults` まで取得。非 200 は throw。

## reddit (公開 JSON API)

- base: `https://www.reddit.com`。posts=`/r/<sub>/<sort>.json`、comments=`/r/<sub>/comments/<postId>.json`。
- 認証: 不要。ただし **User-Agent 必須** (`Canalis-Crawler/1.0 (LUDIARS; +https://github.com/LUDIARS)`)。
- config: `{ subreddit? | postId?, subredditForPost?, limit?, sort?='hot', mode?='posts'|'comments' }`。
- 前提: comments は `[postListing, commentsListing]` の 2 要素配列。`t1` コメントを再帰 flatten。

## website (任意 HTML)

- 任意 URL を `fetch` (`User-Agent`=`Canalis-Crawler/1.0 …`、timeout 既定 10000ms、AbortController)。
- config: `{ url, userAgent?, timeoutMs? }`。
- HTML→構造化テキストは `HtmlParser` を注入 (Lector 接続口)。未注入時はタグ除去フォールバック。

## discord (Discord REST API v10)

- base: `https://discord.com/api/v10`。channels/messages・guilds/channels・guilds/threads/active・archived threads。
- 認証: Bot token (`config.token` / env `DISCORD_BOT_TOKEN`)。
- config (channel): `{ channelId, token?, guildId?, options?: { maxMessages=100, before?, after?, oldestFirst? } }`。
- config (guild): `{ guildId, token?, channelTypes?=[0,5,15], excludeChannelIds?, options? }`。
- 前提: ページネーションは before/after カーソル。guild crawl は text/announcement/forum を走査。

関連: [feature/crawl-stage.md](../feature/crawl-stage.md) / [data/raw-record.md](../data/raw-record.md)
