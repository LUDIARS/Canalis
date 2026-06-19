# interface: FT runner 起動 contract (core 外プロセス)

`FtSink` (core / TS) は学習を持たず、データセットを materialize して core 外の **runner** を起動するだけ。
runner は Python (PEFT/Unsloth/transformers) 等、任意実装でよい。正本: `runners/ft/README.md` + `src/save/ft-writer.ts`。

## 起動契約

```
<command> [...args] --job <path/to/job.json>
```

- `commandRunner(command, args, { cwd })` (`@ludiars/canalis/save`) がこの形で spawn する
  (shell 非経由・引数配列・exit 購読)。
- exit code 0 = 成功、非 0 = 失敗 (reject)。

## job.json (FtSink が出力 → runner が読む)

```jsonc
{
  "task": "classification" | "causal-lm",
  "dataset": "sentiment",
  "count": 1234,
  "dataPath": ".../<stamp>/data.jsonl",
  "jobPath":  ".../<stamp>/job.json",
  "model": { /* base / adapter / ハイパラ。runner が解釈。core は中身を見ない */ },
  "createdAt": "2026-06-09T..."
}
```

`data.jsonl` の形式と配置は [data/ft-dataset.md](../data/ft-dataset.md)。

## runner の責務

1. `--job` から job.json → `dataPath` の JSONL を読む。
2. `task` で分岐し fine-tune (推奨 LoRA/QLoRA)。`model` 指定 (base model 等) に従う。
3. 出力 (adapter 重み / merged model / メトリクス) を `model` 指定先または `<stamp>/output/` に書く。
4. 成功で exit 0、失敗で非 0。

## 参照実装 (`runners/ft/`)

- `train.py` — causal-lm = Gemma 決めうち (Unsloth + 4bit QLoRA で SFT → LoRA adapter 出力)。`--dry-run` でデータ契約のみ検証 (ML 依存不要)。
- `dataset.py` — ML 依存ゼロの純 stdlib 層 (job/jsonl 読込 + messages 正規化)。
- `requirements.txt` — WSL2 / Linux + CUDA 前提。
- スコープ外: `task=classification` の FT (別 runner)、adapter の merge / GGUF 変換 / Ollama 登録。

関連: [feature/ft-import.md](../feature/ft-import.md) / [data/ft-dataset.md](../data/ft-dataset.md)
