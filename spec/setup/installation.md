# setup: インストール / ビルド / 公開

`@ludiars/canalis` (ESM、`type: module`)。正本: `package.json` / `tsconfig.json` / `.github/workflows/`。

## 前提

- Node 22 (CI は `node-version: 22`)。
- TypeScript 5.6 / vitest 2.1 / tsx 4.19 (devDependencies)。
- 依存 (dependencies): `js-yaml` (manifest ローダ) / `playwright` (notion-public crawler)。
- `notion-public` adapter を使う場合、Playwright のブラウザ (Chromium) が必要 (`npx playwright install chromium`)。
- 実 DB 結合テスト: `kuzu` (devDependency, in-process embedded) / Postgres は任意 (`TEST_POSTGRES_URL`)。
- FT runner (`runners/ft/`) は core 外。Python + WSL2/Linux + CUDA 前提 (`runners/ft/requirements.txt`)。

## ローカル手順

```bash
npm install
npm run build      # tsc -p tsconfig.json → dist/
npm test           # vitest run (fake で契約検証)
npm run typecheck  # tsc --noEmit
npm run cli -- ... # tsx src/cli.ts (CLI を開発実行)
```

## 公開エントリ (exports)

| サブパス | 内容 |
|---|---|
| `.` (root) | core 契約 + ①adapter + ③writer (LLM を含まない) |
| `./core` | 契約 (raw-record / envelope / pipeline / manifest) |
| `./save` | ③writer + `commandRunner` |
| `./notion` `./notion-public` `./youtube` `./reddit` `./website` `./discord` | ①adapter 個別 |
| `./llm` | opt-in LLM 機構 (core/crawl/save とは別系統) |

- `bin.canalis` = `dist/cli.js`。`files: ["dist"]` (ソースは publish しない)。

## 公開 (GitHub Packages)

- registry: `https://npm.pkg.github.com` (`publishConfig`、@ludiars scope)。
- `v*` タグ push で publish ワークフローが `npm ci && npm run build && npm publish` (`.github/workflows`)。

関連: [setup/configuration.md](./configuration.md) / [test/strategy.md](../test/strategy.md)
