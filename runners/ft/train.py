#!/usr/bin/env python3
"""Canalis FT runner — Gemma を Unsloth + QLoRA で fine-tune する (core 外)。

FtSink (Canalis core / TS) が出力した job.json を読み、task=causal-lm の場合に
Gemma を 4bit QLoRA + LoRA で SFT し、LoRA adapter を出力する。

  python train.py --job <path/to/job.json>           # 学習を実行
  python train.py --job <path/to/job.json> --dry-run # データ契約のみ検証 (ML 依存不要)

実行環境: WSL2 / Linux + CUDA 前提 (unsloth / bitsandbytes は Windows native では基本動かない)。
依存は requirements.txt 参照。core はこのファイルに依存しない (LLM/ML 依存ゼロを保つ)。

job.json の `model` で受け付ける設定 (すべて任意・既定は下記 GEMMA_DEFAULTS):
  base, max_seq_length, load_in_4bit, lora_r, lora_alpha, lora_dropout,
  target_modules, epochs, max_steps, learning_rate, batch_size,
  grad_accum, warmup_steps, weight_decay, seed, chat_template
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from dataset import load_job, read_examples, to_conversations

# Gemma 決めうちの既定。job.model で個別上書き可 (base は Gemma 系サイズ差し替えを想定)。
GEMMA_DEFAULTS = {
    "base": "unsloth/gemma-2-2b-it",
    "max_seq_length": 2048,
    "load_in_4bit": True,
    "lora_r": 16,
    "lora_alpha": 16,
    "lora_dropout": 0.0,
    "target_modules": [
        "q_proj", "k_proj", "v_proj", "o_proj",
        "gate_proj", "up_proj", "down_proj",
    ],
    "epochs": 1,
    "max_steps": 0,  # >0 なら epochs より優先
    "learning_rate": 2e-4,
    "batch_size": 2,
    "grad_accum": 4,
    "warmup_steps": 5,
    "weight_decay": 0.01,
    "seed": 3407,
    "chat_template": "gemma-2",
}


def resolve_config(model_cfg: dict) -> dict:
    """job.model を既定にマージする (未知キーも通す＝runner 拡張余地)。"""
    cfg = dict(GEMMA_DEFAULTS)
    cfg.update(model_cfg or {})
    return cfg


def train_gemma(conversations: list[dict], cfg: dict, out_dir: Path) -> None:
    """Gemma を Unsloth + QLoRA で SFT し、LoRA adapter を out_dir へ保存する。

    重い依存 (unsloth/torch/trl) はこの関数内で遅延 import する。
    """
    from unsloth import FastLanguageModel
    from unsloth.chat_templates import get_chat_template
    from datasets import Dataset
    from trl import SFTConfig, SFTTrainer

    model, tokenizer = FastLanguageModel.from_pretrained(
        model_name=cfg["base"],
        max_seq_length=cfg["max_seq_length"],
        dtype=None,  # 環境に合わせ自動 (bf16/fp16)
        load_in_4bit=cfg["load_in_4bit"],
    )
    tokenizer = get_chat_template(tokenizer, chat_template=cfg["chat_template"])

    model = FastLanguageModel.get_peft_model(
        model,
        r=cfg["lora_r"],
        target_modules=cfg["target_modules"],
        lora_alpha=cfg["lora_alpha"],
        lora_dropout=cfg["lora_dropout"],
        bias="none",
        use_gradient_checkpointing="unsloth",
        random_state=cfg["seed"],
    )

    def render(row: dict) -> dict:
        text = tokenizer.apply_chat_template(
            row["messages"], tokenize=False, add_generation_prompt=False
        )
        return {"text": text}

    dataset = Dataset.from_list(conversations).map(render)

    sft_args = SFTConfig(
        per_device_train_batch_size=cfg["batch_size"],
        gradient_accumulation_steps=cfg["grad_accum"],
        warmup_steps=cfg["warmup_steps"],
        num_train_epochs=cfg["epochs"],
        max_steps=cfg["max_steps"] if cfg["max_steps"] and cfg["max_steps"] > 0 else -1,
        learning_rate=cfg["learning_rate"],
        weight_decay=cfg["weight_decay"],
        seed=cfg["seed"],
        logging_steps=1,
        optim="adamw_8bit",
        lr_scheduler_type="linear",
        output_dir=str(out_dir / "checkpoints"),
        dataset_text_field="text",
        max_seq_length=cfg["max_seq_length"],
        report_to="none",
    )

    trainer = SFTTrainer(
        model=model,
        tokenizer=tokenizer,
        train_dataset=dataset,
        args=sft_args,
    )
    stats = trainer.train()

    # LoRA adapter + tokenizer を保存 (merged/GGUF 変換は別スコープ)。
    model.save_pretrained(str(out_dir))
    tokenizer.save_pretrained(str(out_dir))
    print(
        f"[canalis-ft] done: loss={stats.training_loss:.4f} -> {out_dir}",
        file=sys.stderr,
    )


def main() -> int:
    ap = argparse.ArgumentParser(description="Canalis Gemma FT runner (Unsloth QLoRA)")
    ap.add_argument("--job", required=True, help="FtSink が出力した job.json")
    ap.add_argument(
        "--dry-run",
        action="store_true",
        help="データ契約のみ検証して終了 (モデルを読まない＝ML 依存不要)",
    )
    args = ap.parse_args()

    job = load_job(args.job)
    examples = read_examples(job["dataPath"])

    if job["task"] == "classification":
        # 分類器 FT は Gemma スコープ外 (別 runner: transformers+peft)。
        raise SystemExit("classification FT は本 runner のスコープ外 (Gemma=causal-lm のみ)")
    if job["task"] != "causal-lm":
        raise SystemExit(f"unknown task: {job['task']}")

    conversations = to_conversations(examples)
    cfg = resolve_config(job.get("model", {}))
    out_dir = Path(job["jobPath"]).parent / "output"

    print(
        f"[canalis-ft] task=causal-lm base={cfg['base']} count={len(conversations)} -> {out_dir}",
        file=sys.stderr,
    )

    if args.dry_run:
        sample = conversations[0]["messages"] if conversations else []
        print(f"[canalis-ft] dry-run OK: {len(conversations)} examples normalized", file=sys.stderr)
        print(f"[canalis-ft] sample[0] messages: {sample}", file=sys.stderr)
        return 0

    out_dir.mkdir(parents=True, exist_ok=True)
    train_gemma(conversations, cfg, out_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
