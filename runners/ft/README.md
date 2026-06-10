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

## 参照実装 — Gemma (Unsloth QLoRA)

`task=causal-lm` の実体実装を同梱（**Gemma 決めうち**）。

- [`train.py`](./train.py) — Unsloth + 4bit QLoRA で Gemma を SFT し、LoRA adapter を
  `<stamp>/output/` へ保存する。ハイパラ等の重い依存は遅延 import。
- [`dataset.py`](./dataset.py) — ML 依存ゼロの純 stdlib 層（job/jsonl 読込 + messages 正規化）。
  GPU 無し環境でもこの層だけでデータ契約を検証できる。
- [`requirements.txt`](./requirements.txt) — 依存。**WSL2 / Linux + CUDA 前提**
  （bitsandbytes/unsloth は Windows native では基本動かない）。

```bash
pip install -r requirements.txt        # unsloth は extra 付き install 推奨 (requirements.txt 参照)
python train.py --job <job.json>            # 学習を実行 (GPU 必須)
python train.py --job <job.json> --dry-run  # データ契約のみ検証 (ML 依存不要)
```

`--dry-run` は job.json + data.jsonl を読んで messages へ正規化できるかだけを確認する
（モデルを一切ロードしない）。FtSink の出力疎通を CI / 非 GPU 環境で確かめるのに使う。

### job.model で渡せる設定（すべて任意・既定は `train.py` の `GEMMA_DEFAULTS`）

`base`（既定 `unsloth/gemma-2-2b-it`・Gemma 系サイズ差し替え用）, `max_seq_length`,
`load_in_4bit`, `lora_r`, `lora_alpha`, `lora_dropout`, `target_modules`,
`epochs`, `max_steps`（>0 で epochs より優先）, `learning_rate`, `batch_size`,
`grad_accum`, `warmup_steps`, `weight_decay`, `seed`, `chat_template`（既定 `gemma-2`）。

> 中間データ（dataset/job）は Canalis 側に残るので、別 base や別ハイパラでの再学習は
> 同じ data.jsonl を `model` 差し替えで再 import すればよい（再クロール不要）。

### スコープ外（本 runner では未対応）

- `task=classification`（分類器 FT）は別 runner（transformers+peft）。本 runner は exit 非0。
- adapter の merge / GGUF 変換 / Ollama 登録は別タスク（adapter 出力までで止める）。
