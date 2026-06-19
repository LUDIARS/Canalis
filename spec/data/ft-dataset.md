# data: FT データセット / ジョブ manifest (FtSink の出力)

`FtSink` が `FtBatch` を materialize した永続データ。外部 FT runner の入力になる。
正本: `src/save/ft-writer.ts` + `runners/ft/README.md`。

## 配置レイアウト

```
<dir>/<dataset>/<stamp>/
  ├── data.jsonl   # 1 行 1 学習例
  └── job.json     # ジョブ記述 (runner 入力)
```

- `dir`: `FtSinkConfig.dir`。`dataset`: `FtBatch.dataset` (省略時 `default`)。
- `stamp`: 生成時刻 (ISO8601、`:` → `-`)。`FtSinkConfig.now` で注入可 (テスト決定性)。

## data.jsonl (1 行 1 例)

`task` で形が変わる (verbatim 書込):

- `task=classification`: `{ "input": "...", "label": "..." }`
- `task=causal-lm`: `{ "messages": [...] }` または `{ "prompt": "...", "completion": "..." }`

## job.json (`FtJob`)

```jsonc
{
  "task": "classification" | "causal-lm",
  "dataset": "sentiment",
  "count": 1234,                 // 学習例数
  "dataPath": ".../<stamp>/data.jsonl",
  "jobPath":  ".../<stamp>/job.json",
  "model": { /* base / adapter / ハイパラ。runner が解釈。core は中身を見ない */ },
  "createdAt": "2026-06-09T..."  // ISO8601
}
```

- core は `model` の中身を解釈しない。中間データ (dataset/job) は Canalis 側に残るので、
  別 base / 別ハイパラの再学習は同じ `data.jsonl` を `model` 差し替えで再 import すればよい (再クロール不要)。

関連: [data/sink-envelope.md](./sink-envelope.md) (FtBatch) / [feature/ft-import.md](../feature/ft-import.md) / [interface/ft-runner.md](../interface/ft-runner.md)
