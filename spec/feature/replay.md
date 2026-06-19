# feature: 再処理 (replay)

## 目的

①→②が常に `RawRecord[]` で受け渡されるので、raw store はその同じ形のバッファになる。
②のロジックを変えたら**再クロールせず** raw store → ② → ③ で焼き直せる。
クロールはレート制限 / 元データ消失 / コスト高なので、これが実運用で効く。

## 振る舞い

`runPipeline(manifest, deps)` (`src/runner/runner.ts`):

- `manifest.replayFrom` があり、かつ `deps.replayLoader` が注入されていれば **replay モード**
  (`RunReport.mode = 'replay'`)。
- replay 時は `deps.replayLoader(manifest.replayFrom)` で `RawRecord[]` を読み込み、
  crawl.sources を起点にしない。
- replay 時は **raw 保存をスキップ**する (replay 元を上書きしない)。
- 以降の ②→③ は crawl モードと同一。

## raw store の読み戻し (参照実装)

`src/save/raw-writer.ts`:

- `loadRawRecordsFromFile(file)` — 1 本の JSONL を `RawRecord[]` に復元。
- `loadRawRecordsFromDir(dir)` — ディレクトリ内の `*.jsonl` をファイル名昇順で連結。

`deps.replayLoader` はこれらを使って `SinkSpec` (raw store の場所) から `RawRecord[]` を返す関数として注入する。

## 例 (manifest)

```yaml
replayFrom: { adapter: jsonl, accepts: raw, config: { dir: "data/raw/tr" } }
```

関連: [data/raw-jsonl-store.md](../data/raw-jsonl-store.md) / [feature/manifest.md](./manifest.md)
