"""Canalis FT runner のデータ層 (ML 依存ゼロ・純 stdlib)。

job.json / data.jsonl を読み、学習例を chat messages へ正規化する。
train.py から使う。ここは unsloth/torch に一切依存しないので、GPU 無し環境でも
`--dry-run` / CI でデータ契約 (FtSink 出力) の疎通を検証できる。

契約は ../README.md / Canalis DESIGN.md §FT を参照。
"""
from __future__ import annotations

import json
from pathlib import Path

# Gemma の chat ロールは user / assistant (model) の 2 種。system ロールは持たないので、
# system 内容は直後の user ターン先頭へ畳み込む (apply_chat_template が gemma で落ちないように)。
_ROLE_ALIASES = {
    "user": "user",
    "human": "user",
    "assistant": "assistant",
    "model": "assistant",
    "ai": "assistant",
    "system": "system",
}


def load_job(job_path: str) -> dict:
    """FtSink が出力した job.json を読み、必須キーを検証して返す。"""
    job = json.loads(Path(job_path).read_text(encoding="utf-8"))
    for key in ("task", "dataPath"):
        if key not in job:
            raise SystemExit(f"job.json missing '{key}'")
    return job


def read_examples(data_path: str) -> list[dict]:
    """data.jsonl を 1 行 1 例で読む (空行は無視)。"""
    rows: list[dict] = []
    for i, line in enumerate(Path(data_path).read_text(encoding="utf-8").splitlines(), 1):
        line = line.strip()
        if not line:
            continue
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError as e:
            raise SystemExit(f"data.jsonl line {i}: invalid JSON ({e})")
    return rows


def _normalize_messages(messages: list[dict]) -> list[dict]:
    """messages 形式 ({role, content}[]) を Gemma 用 user/assistant 列へ正規化する。

    - role エイリアスを吸収 (human→user, model/ai→assistant)。
    - system は直後の user 先頭へ畳み込む (先頭が user でなければ user ターンを新設)。
    """
    pending_system: list[str] = []
    out: list[dict] = []
    for m in messages:
        if not isinstance(m, dict) or "role" not in m or "content" not in m:
            raise SystemExit(f"message requires {{role, content}}: {m!r}")
        role = _ROLE_ALIASES.get(str(m["role"]).lower())
        if role is None:
            raise SystemExit(f"unknown message role: {m['role']!r}")
        content = str(m["content"])
        if role == "system":
            pending_system.append(content)
            continue
        if role == "user" and pending_system:
            content = "\n\n".join([*pending_system, content])
            pending_system = []
        out.append({"role": role, "content": content})
    if pending_system:
        # 末尾に未消化の system が残る = user ターンが無い。先頭に user として差し込む。
        out.insert(0, {"role": "user", "content": "\n\n".join(pending_system)})
    if not out:
        raise SystemExit("example has no usable messages")
    return out


def normalize_example(ex: dict) -> dict:
    """causal-lm の 1 例を `{"messages": [...]}` へ正規化する。

    受け付ける形 (FtSink の causal-lm 契約):
      - `{"messages": [{"role", "content"}, ...]}`
      - `{"prompt": "...", "completion": "..."}`
    """
    if "messages" in ex:
        return {"messages": _normalize_messages(ex["messages"])}
    if "prompt" in ex and "completion" in ex:
        return {
            "messages": [
                {"role": "user", "content": str(ex["prompt"])},
                {"role": "assistant", "content": str(ex["completion"])},
            ]
        }
    raise SystemExit("causal-lm example requires {messages} or {prompt, completion}")


def to_conversations(examples: list[dict]) -> list[dict]:
    """学習例の配列を `{"messages": [...]}` の配列へ正規化する。"""
    return [normalize_example(ex) for ex in examples]
