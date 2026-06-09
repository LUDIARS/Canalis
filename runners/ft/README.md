# Canalis FT runner 契約

`FtSink`（Canalis core / TS）は学習を**持たない**。データセットを materialize し、
ここに置く **runner（core 外）** を起動するだけ。実際の fine-tune は runner が行う。

> core が LLM/ML 依存ゼロを保つための分離。runner は Python（PEFT/Unsloth/transformers）等
> 任意の実装でよく、Canalis は `--job <job.json>` を渡して起動する。

## 起動契約

```
<command> [...args] --job <path/to/job.json>
```

`commandRunner(command, args, { cwd })`（`@ludiars/canalis/save`）がこの形で spawn する
（shell 非経由・引数配列・exit 購読）。exit code 0 = 成功、非0 = 失敗。

## job.json（FtSink が出力）

```jsonc
{
  "task": "classification" | "causal-lm",
  "dataset": "sentiment",
  "count": 1234,
  "dataPath": ".../sentiment/<stamp>/data.jsonl",
  "jobPath":  ".../sentiment/<stamp>/job.json",
  "model": { /* base / adapter / ハイパラ。runner が解釈。core は中身を見ない */ },
  "createdAt": "2026-06-09T..."
}
```

## data.jsonl（1 行 1 例）

- `task=classification`（A: 分類器）: `{ "input": "...", "label": "..." }`
- `task=causal-lm`（B: 生成 LLM）: `{ "messages": [...] }` または `{ "prompt": "...", "completion": "..." }`

## runner の責務

1. `--job` から job.json を読む → `dataPath` の JSONL を読む。
2. `task` で分岐し fine-tune（推奨: LoRA/QLoRA）。`model` の指定（base model 等）に従う。
3. 出力（adapter 重み / merged model / メトリクス）を `model` の指定先、または
   `<stamp>/output/` に書く。ローカル推論（Ollama 用 GGUF / ONNX / transformers）への
   登録もここで行う。
4. 成功で exit 0、失敗で非0。

参照実装は [`train.py`](./train.py)（スケルトン）。GPU/依存は環境次第なので、各環境で適宜。
