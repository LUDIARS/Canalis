# feature: FT (学習的インポート)

## 目的

収集・整形したデータを**ローカル LLM へ学習として取り込む**経路。③Save の sink の一種
(`FtSink`, `accepts: 'ft'`) として実装する。A=分類器 / B=生成 LLM の両対応。

> **core は学習を持たない** (LLM/ML 依存ゼロ)。`FtSink` がやるのはデータを書く + runner を起動する
> だけ。実際の fine-tune は core 外の runner (Python: PEFT/Unsloth/transformers 等) が行う。

## 振る舞い (`FtSink.write`)

入力 = `FtBatch { task, examples, dataset?, model? }` →

1. `<dir>/<dataset>/<stamp>/` を作成。
2. `data.jsonl` に学習例を 1 行 1 例で verbatim 書込 (task 別の形 → [data/ft-dataset.md](../data/ft-dataset.md))。
3. `job.json` (`FtJob`) を書込 (task / dataset / count / dataPath / jobPath / model / createdAt)。
4. `runner` が注入されていれば `runner(job)` を実行 (無ければ materialize で終わり)。

## task

- **A=分類器** (`task:'classification'`, `{input,label}`): 0+1 カスケードのラベルを教師に
  ローカル小型分類器を FT → 使うほど LLM 需要↓ の自己改善ループ。
- **B=生成 LLM** (`task:'causal-lm'`, `{messages}` / `{prompt,completion}`): ドメイン知識を
  LoRA でローカル LLM に注入。

## runner 起動

- runner は注入 (`FtRunner = (job) => Promise<void>`)。
- `commandRunner(command, args, {cwd})` が `<command> [...args] --job <jobPath>` で外部プロセス起動
  (shell 非経由・引数配列・exit 購読、exit 0=成功 / 非 0=失敗で reject)。
- 契約と参照 Python は `runners/ft/` (core 外、tsc 対象外)。

## 制約 / 現状

- `FtSink` (TS) は実装済 + テスト済 (`test/ft-writer.test.ts`、runner は fake)。
- 参照 runner `runners/ft/train.py` は causal-lm = Gemma (Unsloth QLoRA) 決めうちの雛形。
  classification 用 runner は未実装 (別 runner)。GPU/依存は環境次第 (WSL2/Linux + CUDA 前提)。

関連: [interface/ft-runner.md](../interface/ft-runner.md) / [data/ft-dataset.md](../data/ft-dataset.md) / [data/sink-envelope.md](../data/sink-envelope.md)
