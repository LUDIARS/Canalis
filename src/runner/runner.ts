// manifest を解釈して 3 ステージを実行するオーケストレータ。
//   crawl(① sources) → [rawSave(raw sink)] → transform(②) → save(③ sinks)
// source / transform / sink の実体は呼び出し側が deps として注入する (DI)。
// サービス固有の ②Transform は各サービスリポに置き、 ここでは名前で解決するだけ。

import type { RawRecord } from '../core/raw-record.js';
import type { SinkBatch } from '../core/envelope.js';
import type { Source, Transform, Sink } from '../core/pipeline.js';
import type { Manifest, SinkSpec } from '../core/manifest.js';

/** 注入する実体群。 adapter 名 → インスタンス。 */
export type PipelineDeps = {
  sources: Record<string, Source>;
  transforms?: Record<string, Transform>;
  sinks: Record<string, Sink>;
  /** manifest.replayFrom 指定時に raw store から RawRecord[] を読む関数 (任意)。 */
  replayLoader?: (spec: SinkSpec) => Promise<RawRecord[]>;
};

export type StageError = { stage: string; detail: string };

export type RunReport = {
  pipeline: string;
  mode: 'crawl' | 'replay';
  recordCount: number;
  rawSaved: boolean;
  written: { sink: string; kind: string; batches: number }[];
  errors: StageError[];
};

function requireSink(deps: PipelineDeps, spec: SinkSpec): Sink {
  const sink = deps.sinks[spec.adapter];
  if (!sink) throw new Error(`unknown sink adapter: ${spec.adapter}`);
  if (sink.accepts !== spec.accepts) {
    throw new Error(
      `sink '${spec.adapter}' accepts '${sink.accepts}' but manifest declares '${spec.accepts}'`,
    );
  }
  return sink;
}

/** ①Crawl: manifest の各 source を実行して RawRecord[] を集める。 */
async function crawlAll(manifest: Manifest, deps: PipelineDeps, errors: StageError[]): Promise<RawRecord[]> {
  const records: RawRecord[] = [];
  for (const spec of manifest.crawl.sources) {
    const source = deps.sources[spec.adapter];
    if (!source) throw new Error(`unknown source adapter: ${spec.adapter}`);
    try {
      records.push(...(await source.crawl(spec.config)));
    } catch (err) {
      errors.push({ stage: `crawl:${spec.adapter}`, detail: (err as Error).message });
    }
  }
  return records;
}

/** ③Save: ②が出した各エンベロープを、 kind が一致する save 先 sink へ振り分けて書く。 */
async function saveAll(
  manifest: Manifest,
  deps: PipelineDeps,
  batches: SinkBatch[],
  errors: StageError[],
): Promise<RunReport['written']> {
  const written: RunReport['written'] = [];
  for (const spec of manifest.save ?? []) {
    const sink = requireSink(deps, spec);
    const matched = batches.filter((b) => b.kind === spec.accepts);
    let count = 0;
    for (const batch of matched) {
      try {
        await sink.write(batch);
        count++;
      } catch (err) {
        errors.push({ stage: `save:${spec.adapter}`, detail: (err as Error).message });
      }
    }
    written.push({ sink: sink.name, kind: spec.accepts, batches: count });
  }
  return written;
}

/** manifest 1 本を実行する。 */
export async function runPipeline(manifest: Manifest, deps: PipelineDeps): Promise<RunReport> {
  const errors: StageError[] = [];
  const replay = Boolean(manifest.replayFrom && deps.replayLoader);
  const mode: RunReport['mode'] = replay ? 'replay' : 'crawl';

  // ① 取得 (または replay 読み込み)
  let records: RawRecord[];
  if (replay) {
    records = await deps.replayLoader!(manifest.replayFrom!);
  } else {
    records = await crawlAll(manifest, deps, errors);
  }

  // raw 保存 (crawl 時のみ。 replay 元を上書きしない)
  let rawSaved = false;
  if (!replay && manifest.crawl.rawSave) {
    const rawSink = requireSink(deps, manifest.crawl.rawSave);
    try {
      await rawSink.write(records);
      rawSaved = true;
    } catch (err) {
      errors.push({ stage: `rawSave:${rawSink.name}`, detail: (err as Error).message });
    }
  }

  // ② 整形
  let batches: SinkBatch[] = [];
  if (manifest.transform) {
    const transform = deps.transforms?.[manifest.transform];
    if (!transform) throw new Error(`unknown transform: ${manifest.transform}`);
    try {
      batches = await transform.run(records);
    } catch (err) {
      errors.push({ stage: `transform:${manifest.transform}`, detail: (err as Error).message });
    }
  }

  // ③ 保存
  const written = await saveAll(manifest, deps, batches, errors);

  return { pipeline: manifest.pipeline, mode, recordCount: records.length, rawSaved, written, errors };
}
