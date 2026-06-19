# feature: マニフェスト (パイプライン宣言)

## 目的

各サービスがコードでなく宣言データでパイプラインの流れを定義する「フォーマット」。
`runPipeline(manifest, deps)` が解釈する。YAML / JSON を `loadManifest()` で読み込める。

正本: `src/core/manifest.ts` (`Manifest`) + `src/core/manifest-loader.ts`。

## スキーマ (`Manifest`)

```ts
type Manifest = {
  pipeline: string;                    // 必須。ログ/レポート用の名前
  crawl: {
    sources: { adapter: string; config: unknown }[];   // ①取得元 (複数可)
    rawSave?: { adapter: string; accepts: SinkKind; config?: unknown };  // 任意: raw verbatim 保存
  };
  transform?: string;                  // ②Transform の adapter 名 (省略時は crawl+rawSave のみ)
  save?: { adapter: string; accepts: SinkKind; config?: unknown }[];     // ③保存先 (複数可)
  replayFrom?: { adapter: string; accepts: SinkKind; config?: unknown }; // 任意: 再処理の起点
};
```

`adapter` 名 → インスタンスの解決はサービス側が `deps` 注入で行う (core はグローバル registry を持たない)。

## 例 (DESIGN より)

```yaml
pipeline: tr-companies
crawl:
  sources: [{ adapter: notion, config: { databaseId: "..." } }]
  rawSave: { adapter: jsonl, accepts: raw, config: { dir: "data/raw/tr" } }
transform: tr:company-normalize
save:
  - { adapter: postgres,  accepts: rdb }   # 企業 → ポスグレ
  - { adapter: reco-kuzu, accepts: kg }    # 推薦タグ関係 → graph
```

## ローダ (`loadManifest`)

- 拡張子 `.yaml` / `.yml` → js-yaml、それ以外 → `JSON.parse`。
- 最小バリデーション: `pipeline` (string) と `crawl` (object) が必須、欠ければ throw。

## 振る舞い上の制約

- `save[].accepts` は対応 sink の `accepts` と一致必須 (runner が突合、不一致 throw)。
- `replayFrom` + `deps.replayLoader` 指定時は crawl.sources を起点にしない ([feature/replay.md](./replay.md))。

関連: [feature/pipeline-overview.md](./pipeline-overview.md) / [data/sink-envelope.md](../data/sink-envelope.md)
