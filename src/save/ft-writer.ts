// ③Save: FT sink — FtBatch を「学習データセット (JSONL) + ジョブ manifest」に materialize し、
// 任意で外部 FT runner を起動する。 A=分類器 / B=生成LLM の両対応 (task で分岐)。
//
// 設計鉄則: Canalis core は学習を一切持たない (LLM/ML 依存ゼロ)。
//   - FtSink がやるのは「データを書く」 + 「runner を起動する」 だけ。
//   - 実際の fine-tune は core 外の runner (Python: PEFT/Unsloth 等) が行う。
//   - runner は注入 (FtRunner) する。 commandRunner で外部プロセスとして起動できる。

import { mkdir, writeFile } from 'node:fs/promises';
import { spawn } from 'node:child_process';
import { join, dirname } from 'node:path';
import type { FtBatch, FtExample, FtTask } from '../core/envelope.js';
import type { Sink } from '../core/pipeline.js';

/** runner へ渡すジョブ記述 (manifest 兼 runner 入力)。 */
export type FtJob = {
  task: FtTask;
  dataset: string;
  /** 学習例数。 */
  count: number;
  /** JSONL データセットの絶対/相対パス。 */
  dataPath: string;
  /** このジョブ manifest 自身のパス。 */
  jobPath: string;
  /** base model / adapter / ハイパラ等 (runner が解釈)。 */
  model: Record<string, unknown>;
  /** 生成時刻 (ISO8601)。 */
  createdAt: string;
};

/** FT runner。 core 外の学習実体 (Python プロセス等) を起動する関数。 */
export type FtRunner = (job: FtJob) => Promise<void>;

export type FtSinkConfig = {
  /** 出力ルート。 <dir>/<dataset>/<stamp>/ に data.jsonl + job.json を書く。 */
  dir: string;
  /** runner。 未指定ならデータセット materialize のみ (起動しない)。 */
  runner?: FtRunner;
  /** stamp (テスト決定性のため注入可)。 既定は呼び出し時刻。 */
  now?: () => string;
};

/** FtBatch を学習データセット化 + 任意 runner 起動する sink。 */
export class FtSink implements Sink<FtBatch> {
  readonly name: string;
  readonly accepts = 'ft' as const;

  constructor(private readonly config: FtSinkConfig, name = 'ft') {
    this.name = name;
  }

  async write(batch: FtBatch): Promise<void> {
    const now = this.config.now ?? (() => new Date().toISOString());
    const createdAt = now();
    const dataset = batch.dataset ?? 'default';
    const stamp = createdAt.replace(/:/g, '-');
    const outDir = join(this.config.dir, dataset, stamp);
    const dataPath = join(outDir, 'data.jsonl');
    const jobPath = join(outDir, 'job.json');

    await mkdir(outDir, { recursive: true });

    // 学習データセット: 1 行 1 例の JSONL (task に応じた形をそのまま書く = verbatim)。
    const lines = batch.examples.map((ex) => JSON.stringify(toRecord(batch.task, ex)));
    await writeFile(dataPath, lines.join('\n') + (lines.length ? '\n' : ''), 'utf8');

    const job: FtJob = {
      task: batch.task,
      dataset,
      count: batch.examples.length,
      dataPath,
      jobPath,
      model: batch.model ?? {},
      createdAt,
    };
    await writeFile(jobPath, JSON.stringify(job, null, 2), 'utf8');

    // runner があれば学習を実行させる (core 外)。 無ければ materialize で終わり。
    if (this.config.runner) {
      await this.config.runner(job);
    }
  }
}

/** task に応じて 1 例を JSONL レコードへ整形する。 不正な形は fail-fast。 */
function toRecord(task: FtTask, ex: FtExample): Record<string, unknown> {
  if (task === 'classification') {
    if ('input' in ex && 'label' in ex) return { input: ex.input, label: ex.label };
    throw new Error('classification example requires { input, label }');
  }
  // causal-lm: chat か prompt/completion のどちらか
  if ('messages' in ex) return { messages: ex.messages };
  if ('prompt' in ex && 'completion' in ex) return { prompt: ex.prompt, completion: ex.completion };
  throw new Error('causal-lm example requires { messages } or { prompt, completion }');
}

/**
 * 外部コマンドを FT runner として起動するファクトリ。
 * 規約: shell を介さず引数は配列、 cwd 明示、 exit/error を購読してクリーンアップ。
 * runner は `<command> [...args] --job <jobPath>` で呼ばれ、 job.json を読んで学習する。
 */
export function commandRunner(command: string, args: string[] = [], opts: { cwd?: string } = {}): FtRunner {
  return (job: FtJob) =>
    new Promise<void>((resolve, reject) => {
      const child = spawn(command, [...args, '--job', job.jobPath], {
        cwd: opts.cwd,
        stdio: 'inherit',
        shell: false,
      });
      child.on('error', (err) => reject(err));
      child.on('close', (code) => {
        if (code === 0) resolve();
        else reject(new Error(`ft runner '${command}' exited with code ${code}`));
      });
    });
}
