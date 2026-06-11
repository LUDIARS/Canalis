// Kuzu 実 DB 結合テスト (in-process embedded、外部サーバ不要)。
// kuzu npm パッケージを devDependency で使う。
// tmpdir に DB を作成して各テスト後に削除する。

import { describe, it, expect, beforeAll, afterAll } from 'vitest';
import { mkdtemp, rm } from 'node:fs/promises';
import { join } from 'node:path';
import { tmpdir } from 'node:os';
import { KuzuSink } from '../../src/save/kuzu-writer.js';
import type { KgBatch } from '../../src/core/envelope.js';

let dbDir: string;
// eslint-disable-next-line @typescript-eslint/no-explicit-any
let db: any, conn: any;

describe('KuzuSink — integration', () => {
  beforeAll(async () => {
    const tmpBase = await mkdtemp(join(tmpdir(), 'canalis-kuzu-'));
    dbDir = tmpBase;
    const kuzuPath = join(tmpBase, 'db');
    const kuzu = await import('kuzu');
    db = new kuzu.Database(kuzuPath);
    conn = new kuzu.Connection(db);

    // テスト用スキーマを作成
    await conn.query('CREATE NODE TABLE IF NOT EXISTS Item(key STRING, name STRING, score INT64, PRIMARY KEY(key))');
    await conn.query('CREATE REL TABLE IF NOT EXISTS RELATED(FROM Item TO Item, weight DOUBLE)');
  });

  afterAll(async () => {
    if (conn) conn.close();
    if (db) db.close();
    if (dbDir) await rm(dbDir, { recursive: true, force: true });
  });

  it('merges nodes into kuzu', async () => {
    const exec = async (query: string, params: Record<string, unknown>) => {
      const prepared = await conn.prepare(query);
      await conn.execute(prepared, params);
    };
    const sink = new KuzuSink(exec, { keyProp: 'key' });
    const batch: KgBatch = {
      kind: 'kg',
      nodes: [
        { label: 'Item', key: 'a', props: { name: 'Alpha', score: 10 } },
        { label: 'Item', key: 'b', props: { name: 'Beta', score: 20 } },
      ],
      edges: [],
    };
    await sink.write(batch);

    const result = await conn.query('MATCH (n:Item) RETURN n.key, n.name, n.score ORDER BY n.key');
    const rows = await result.getAll();
    expect(rows).toHaveLength(2);
    expect(rows[0]['n.key']).toBe('a');
    expect(rows[0]['n.name']).toBe('Alpha');
    expect(rows[1]['n.score']).toBe(20);
  });

  it('merges edges between nodes', async () => {
    const exec = async (query: string, params: Record<string, unknown>) => {
      const prepared = await conn.prepare(query);
      await conn.execute(prepared, params);
    };
    const sink = new KuzuSink(exec, { keyProp: 'key' });
    const batch: KgBatch = {
      kind: 'kg',
      nodes: [],
      edges: [
        {
          label: 'RELATED',
          from: { label: 'Item', key: 'a' },
          to: { label: 'Item', key: 'b' },
          props: { weight: 0.9 },
        },
      ],
    };
    await sink.write(batch);

    const result = await conn.query('MATCH (a:Item)-[r:RELATED]->(b:Item) RETURN a.key, b.key, r.weight');
    const rows = await result.getAll();
    expect(rows).toHaveLength(1);
    expect(rows[0]['a.key']).toBe('a');
    expect(rows[0]['b.key']).toBe('b');
    expect(rows[0]['r.weight']).toBeCloseTo(0.9);
  });

  it('re-merges (MERGE = upsert) existing node without error', async () => {
    const exec = async (query: string, params: Record<string, unknown>) => {
      const prepared = await conn.prepare(query);
      await conn.execute(prepared, params);
    };
    const sink = new KuzuSink(exec, { keyProp: 'key' });
    const batch: KgBatch = {
      kind: 'kg',
      nodes: [{ label: 'Item', key: 'a', props: { name: 'Alpha Updated', score: 99 } }],
      edges: [],
    };
    await sink.write(batch);

    const result = await conn.query("MATCH (n:Item {key: 'a'}) RETURN n.name, n.score");
    const rows = await result.getAll();
    expect(rows[0]['n.name']).toBe('Alpha Updated');
  });
});
