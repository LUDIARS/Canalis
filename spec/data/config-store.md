# data: config.json (設定ストア)

リポジトリ直下の `config.json` に保持する設定。`notion.token` のみ暗号化、非シークレットは平文。
正本: `src/config/store.ts` + `src/config/crypto.ts`。

## 保存先 / master 鍵

- パス: env `CANALIS_CONFIG_PATH` → 無ければ `<cwd>/config.json` (`.gitignore` 済、コミットされない)。
- master 鍵: env `CANALIS_MASTER_KEY` → 無ければマシン束縛値 `canalis:<hostname>:<username>`。

## スキーマ (`CanalisConfig`)

```ts
type CanalisConfig = {
  notion?: {
    tokenEnc?: EncryptedBlob;   // 暗号化済 integration token
    version?: string;            // Notion-Version ヘッダ (非シークレット・平文)
    minIntervalMs?: number;      // API 呼出最小間隔 ms (非シークレット・平文)
  };
};
```

CLI から触れるキー (`ConfigKey`): `notion.token` / `notion.version` / `notion.minIntervalMs`。

## EncryptedBlob (暗号化トークン)

salt 付き AES-256-GCM (Excubitor `src/secrets/crypto.ts` と同方式の移植)。全フィールド base64。

```ts
type EncryptedBlob = { v: 1; salt: string; iv: string; tag: string; data: string };
```

- 鍵導出 = `scrypt(masterSecret, salt, 32)`。salt はレコード毎ランダム (16 bytes)、iv 12 bytes。
- master 鍵が変わると復号失敗 → `getStoredToken()` は `null` を返す (throw しない)。

## 解決順 (crawl 時)

- token: env `NOTION_TOKEN` 優先 → 無ければ `config.json` の暗号化トークンを復号。
- 非シークレット: `applyNotionConfigToEnv()` が `version` → env `NOTION_VERSION`、
  `minIntervalMs` → env `NOTION_MIN_INTERVAL_MS` に注入 (既存 env は上書きしない)。

関連: [interface/cli.md](../interface/cli.md) (config サブコマンド) / [setup/configuration.md](../setup/configuration.md)
