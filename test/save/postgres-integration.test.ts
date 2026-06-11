// Postgres 実 DB 結合テスト。
// TEST_POSTGRES_URL が未設定の場合は全テストをスキップする (CI 環境では省略可)。
//
// 手動実行:
//   TEST_POSTGRES_URL=postgres://user:pass@localhost:5432/testdb npm test -- postgres-integration

import { describe, it, expect, beforeAll, afterAll } from 'vitest';
import { PostgresSink } from '../../src/save/postgres-writer.js';
import type { RdbBatch } from '../../src/core/envelope.js';

const PG_URL = process.env['TEST_POSTGRES_URL'];
const skip = !PG_URL;

describe.skipIf(skip)('PostgresSink — integration', () => {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  let sql: any;
  const TABLE = 'canalis_test_items';

  beforeAll(async () => {
    const { default: postgres } = await import('postgres');
    sql = postgres(PG_URL!);
    await sql`CREATE TABLE IF NOT EXISTS ${sql(TABLE)} (
      id TEXT PRIMARY KEY,
      value TEXT,
      count INTEGER
    )`;
    await sql`TRUNCATE ${sql(TABLE)}`;
  });

  afterAll(async () => {
    if (sql) {
      await sql`DROP TABLE IF EXISTS ${sql(TABLE)}`;
      await sql.end();
    }
  });

  it('upserts rows into postgres', async () => {
    const exec = async (query: string, params: unknown[]) => {
      await sql.unsafe(query, params as never[]);
    };
    const sink = new PostgresSink(exec);
    const batch: RdbBatch = {
      kind: 'rdb',
      upserts: [
        {
          table: TABLE,
          conflictKey: ['id'],
          rows: [
            { id: 'item1', value: 'hello', count: 1 },
            { id: 'item2', value: 'world', count: 2 },
          ],
        },
      ],
    };
    await sink.write(batch);

    const rows = await sql`SELECT * FROM ${sql(TABLE)} ORDER BY id`;
    expect(rows).toHaveLength(2);
    expect(rows[0].id).toBe('item1');
    expect(rows[0].value).toBe('hello');
    expect(rows[1].count).toBe(2);
  });

  it('upserts update existing rows (ON CONFLICT)', async () => {
    const exec = async (query: string, params: unknown[]) => {
      await sql.unsafe(query, params as never[]);
    };
    const sink = new PostgresSink(exec);
    const batch: RdbBatch = {
      kind: 'rdb',
      upserts: [{ table: TABLE, conflictKey: ['id'], rows: [{ id: 'item1', value: 'updated', count: 99 }] }],
    };
    await sink.write(batch);

    const [row] = await sql`SELECT * FROM ${sql(TABLE)} WHERE id = 'item1'`;
    expect(row.value).toBe('updated');
    expect(row.count).toBe(99);
  });
});

describe.skipIf(!skip)('PostgresSink — integration (skipped: no TEST_POSTGRES_URL)', () => {
  it.skip('skipped', () => {});
});
