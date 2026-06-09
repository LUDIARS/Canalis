#!/usr/bin/env python3
"""Canalis FT runner 参照スケルトン (core 外)。

job.json (FtSink 出力) を読み、task で分岐して fine-tune する雛形。
実際の学習部は環境 (GPU / ライブラリ) に依存するため TODO として残す。
契約は ../README.md / Canalis DESIGN.md §FT を参照。

  python train.py --job <path/to/job.json>

依存 (例、環境に応じて): transformers, peft, datasets, torch / unsloth。
core はこのファイルに依存しない (LLM/ML 依存ゼロを保つ)。
"""
import argparse
import json
import sys
from pathlib import Path


def load_job(job_path: str) -> dict:
    job = json.loads(Path(job_path).read_text(encoding="utf-8"))
    for key in ("task", "dataPath"):
        if key not in job:
            raise SystemExit(f"job.json missing '{key}'")
    return job


def read_examples(data_path: str) -> list[dict]:
    rows = []
    for line in Path(data_path).read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line:
            rows.append(json.loads(line))
    return rows


def train_classification(examples: list[dict], model: dict, out_dir: Path) -> None:
    # A: text -> label の分類器を fine-tune (例: distilbert 多言語 + LoRA)。
    # TODO: transformers + peft で実装。labels = sorted(set(e["label"] for e in examples))
    #       AutoModelForSequenceClassification + LoraConfig + Trainer。
    #       出力を out_dir / (Ollama不要、ONNX/transformers で推論)。
    raise NotImplementedError("classification FT は環境依存。transformers+peft で実装する")


def train_causal_lm(examples: list[dict], model: dict, out_dir: Path) -> None:
    # B: 生成 LLM を LoRA/QLoRA で fine-tune (例: Unsloth + base = model["base"])。
    # TODO: messages / (prompt,completion) を chat template 化 → SFT。
    #       adapter を out_dir へ、必要なら GGUF 変換 → Ollama 登録。
    raise NotImplementedError("causal-lm FT は環境依存。unsloth/peft で実装する")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--job", required=True)
    args = ap.parse_args()

    job = load_job(args.job)
    examples = read_examples(job["dataPath"])
    out_dir = Path(job["jobPath"]).parent / "output"
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"[canalis-ft] task={job['task']} count={len(examples)} -> {out_dir}", file=sys.stderr)

    if job["task"] == "classification":
        train_classification(examples, job.get("model", {}), out_dir)
    elif job["task"] == "causal-lm":
        train_causal_lm(examples, job.get("model", {}), out_dir)
    else:
        raise SystemExit(f"unknown task: {job['task']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
