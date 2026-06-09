// FtSink が学習データセット (JSONL) + job manifest を materialize し、 runner を起動することを検証。
// 実 FT は行わない (runner は fake)。 ②→③(ft) 契約 = データセット形を固定するテスト。

import { describe, it, expect } from 'vitest';
import { mkdtemp, readFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { FtSink, type FtJob } from '../src/save/ft-writer.js';
import type { FtBatch } from '../src/core/envelope.js';

const STAMP = '2026-06-09T00:00:00.000Z';
const STAMP_DIR = STAMP.replace(/:/g, '-');

async function newDir(): Promise<string> {
  return mkdtemp(join(tmpdir(), 'canalis-ft-'));
}

describe('FtSink', () => {
  it('classification: text→label を JSONL 化し runner を起動する', async () => {
    const dir = await newDir();
    const seen: FtJob[] = [];
    const sink = new FtSink({ dir, now: () => STAMP, runner: async (job) => { seen.push(job); } });

    const batch: FtBatch = {
      kind: 'ft',
      task: 'classification',
      dataset: 'sentiment',
      model: { base: 'distilbert-multilingual', method: 'lora' },
      examples: [
        { input: 'this game is amazing', label: 'joy' },
        { input: 'boring grind', label: 'boredom' },
      ],
    };
    await sink.write(batch);

    const base = join(dir, 'sentiment', STAMP_DIR);
    const data = await readFile(join(base, 'data.jsonl'), 'utf8');
    const rows = data.trim().split('\n').map((l) => JSON.parse(l));
    expect(rows).toEqual([
      { input: 'this game is amazing', label: 'joy' },
      { input: 'boring grind', label: 'boredom' },
    ]);

    const job = JSON.parse(await readFile(join(base, 'job.json'), 'utf8')) as FtJob;
    expect(job.task).toBe('classification');
    expect(job.count).toBe(2);
    expect(job.model).toEqual({ base: 'distilbert-multilingual', method: 'lora' });

    // runner が job を受け取って起動された
    expect(seen).toHaveLength(1);
    expect(seen[0]!.dataset).toBe('sentiment');
  });

  it('causal-lm: chat / prompt-completion を JSONL 化する', async () => {
    const dir = await newDir();
    const sink = new FtSink({ dir, now: () => STAMP }); // runner 無し = materialize のみ

    const batch: FtBatch = {
      kind: 'ft',
      task: 'causal-lm',
      examples: [
        { messages: [{ role: 'user', content: 'q' }, { role: 'assistant', content: 'a' }] },
        { prompt: 'p', completion: 'c' },
      ],
    };
    await sink.write(batch);

    const base = join(dir, 'default', STAMP_DIR);
    const rows = (await readFile(join(base, 'data.jsonl'), 'utf8')).trim().split('\n').map((l) => JSON.parse(l));
    expect(rows[0]).toEqual({ messages: [{ role: 'user', content: 'q' }, { role: 'assistant', content: 'a' }] });
    expect(rows[1]).toEqual({ prompt: 'p', completion: 'c' });
  });

  it('task に合わない例は fail-fast する', async () => {
    const dir = await newDir();
    const sink = new FtSink({ dir, now: () => STAMP });
    const bad: FtBatch = {
      kind: 'ft',
      task: 'classification',
      examples: [{ prompt: 'p', completion: 'c' }],
    };
    await expect(sink.write(bad)).rejects.toThrow(/classification example requires/);
  });
});
