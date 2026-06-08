// ③Save: raw sink — ①の出力 RawRecord[] を JSONL で verbatim 保存する。
// 同じ形を読み戻す loadRawRecords も提供し、 再処理 (replay) の起点にする。
// node の fs のみ依存 (外部 dep なし)。

import { mkdir, readFile, writeFile, readdir } from 'node:fs/promises';
import { join, dirname } from 'node:path';
import type { RawRecord } from '../core/raw-record.js';
import type { Sink } from '../core/pipeline.js';

export type JsonlRawSinkConfig = {
  /** 出力ディレクトリ。 source ごとにサブフォルダを切る運用を想定。 */
  dir: string;
  /** ファイル名に付けるタイムスタンプ。 省略時は呼び出し時刻 (ISO, コロンは - に置換)。 */
  stamp?: string;
};

/** RawRecord[] を 1 行 1 レコードの JSONL で書き出す raw sink。 */
export class JsonlRawSink implements Sink<RawRecord[]> {
  readonly name = 'jsonl';
  readonly accepts = 'raw' as const;

  constructor(private readonly config: JsonlRawSinkConfig) {}

  async write(records: RawRecord[]): Promise<void> {
    const stamp = (this.config.stamp ?? new Date().toISOString()).replace(/:/g, '-');
    const file = join(this.config.dir, `${stamp}.jsonl`);
    await mkdir(dirname(file), { recursive: true });
    const body = records.map((r) => JSON.stringify(r)).join('\n');
    await writeFile(file, body + (body ? '\n' : ''), 'utf8');
  }
}

/** JSONL ファイル 1 本を RawRecord[] に読み戻す (replay 用)。 */
export async function loadRawRecordsFromFile(file: string): Promise<RawRecord[]> {
  const text = await readFile(file, 'utf8');
  return text
    .split('\n')
    .map((l) => l.trim())
    .filter((l) => l.length > 0)
    .map((l) => JSON.parse(l) as RawRecord);
}

/** ディレクトリ内の全 *.jsonl を時系列順に読み込み連結する (replay 用)。 */
export async function loadRawRecordsFromDir(dir: string): Promise<RawRecord[]> {
  const entries = await readdir(dir);
  const files = entries.filter((f) => f.endsWith('.jsonl')).sort();
  const out: RawRecord[] = [];
  for (const f of files) {
    out.push(...(await loadRawRecordsFromFile(join(dir, f))));
  }
  return out;
}
