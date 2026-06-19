# setup: 設定 / 環境変数 / シークレット

正本: `src/config/store.ts` / `src/cli.ts` / 各 `src/crawl/<source>/`。スキーマ詳細は [data/config-store.md](../data/config-store.md)。

## config.json (リポ直下)

- パス: env `CANALIS_CONFIG_PATH` → 無ければ `<cwd>/config.json` (`.gitignore` 済、コミットしない)。
- `notion.token` は salt 付き AES-256-GCM で暗号化。非シークレット (`notion.version` / `notion.minIntervalMs`) は平文。
- 設定 UI は CLI `canalis config …` ([interface/cli.md](../interface/cli.md))。

```bash
npm run cli -- config set notion.token            # 対話入力 (echo 非表示)
echo "$NOTION_TOKEN" | npm run cli -- config set notion.token   # パイプ
npm run cli -- config show                        # token マスク表示
```

## 環境変数一覧

| 環境変数 | 用途 | 既定 / 備考 |
|---|---|---|
| `CANALIS_CONFIG_PATH` | config.json の保存先 override | 無ければ `<cwd>/config.json` |
| `CANALIS_MASTER_KEY` | token 暗号化/復号の master 鍵 | 無ければマシン束縛値 `canalis:<hostname>:<user>` |
| `NOTION_TOKEN` | Notion integration token | 設定時は config の暗号化トークンより**優先** |
| `NOTION_VERSION` | Notion-Version ヘッダ | `applyNotionConfigToEnv()` が config から注入 (既存は上書きしない) |
| `NOTION_MIN_INTERVAL_MS` | Notion API 最小呼出間隔 ms | 同上 |
| `DISCORD_BOT_TOKEN` | Discord Bot token (discord adapter) | `config.token` 省略時に参照 |
| `TEST_POSTGRES_URL` | Postgres 結合テスト接続先 | 未設定時は postgres 結合テストを skip |

> youtube は `apiKey` を config で渡す (env 経由ではない)。reddit / website / notion-public は認証不要。

## master 鍵の注意

`CANALIS_MASTER_KEY` 未設定でマシン束縛値を使う場合、hostname / username が変わると保存済 token を復号できなくなる
(`getStoredToken()` が `null` を返す)。可搬性が要るなら `CANALIS_MASTER_KEY` を明示する。

関連: [data/config-store.md](../data/config-store.md) / [interface/external-sources.md](../interface/external-sources.md)
