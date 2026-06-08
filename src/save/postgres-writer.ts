// ③Save: RDB sink — RdbBatch を Postgres へ upsert する。
// DB ドライバは注入する (共有 lib は postgres パッケージに直接依存しない)。
// エンベロープが「最終形」なので、 ここは table/rows/conflictKey をそのまま SQL 化して書くだけ。

import type { RdbBatch, RdbUpsert, Row } from '../core/envelope.js';
import type { Sink } from '../core/pipeline.js';

/**
 * パラメタ化クエリ実行関数。 consumer 側が postgres / pg 等の driver で実装して注入する。
 * 例 (postgres.js): (q, p) => sql.unsafe(q, p as any).then(() => {})
 */
export type SqlExecutor = (query: string, params: unknown[]) => Promise<void>;

/** Postgres 識別子を安全に引用する ("..."、 内部の " は重ねる)。 */
function quoteIdent(name: string): string {
  if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(name)) {
    // 想定外の識別子は明示エラー (② が出す最終形は制御下のはずなので fail-fast)。
    if (name.includes('"')) throw new Error(`unsafe identifier: ${name}`);
  }
  return `"${name.replace(/"/g, '""')}"`;
}

/** 1 upsert 単位を INSERT ... ON CONFLICT DO UPDATE 文へ。 行ごとに 1 文を生成する。 */
function buildUpsertStatements(u: RdbUpsert): { query: string; params: unknown[] }[] {
  if (u.rows.length === 0) return [];
  const out: { query: string; params: unknown[] }[] = [];
  const table = quoteIdent(u.table);
  const conflict = u.conflictKey.map(quoteIdent).join(', ');

  for (const row of u.rows) {
    const cols = Object.keys(row);
    if (cols.length === 0) continue;
    const colList = cols.map(quoteIdent).join(', ');
    const placeholders = cols.map((_, i) => `$${i + 1}`).join(', ');
    const updates = cols
      .filter((c) => !u.conflictKey.includes(c))
      .map((c) => `${quoteIdent(c)} = EXCLUDED.${quoteIdent(c)}`)
      .join(', ');
    const onConflict = updates
      ? `ON CONFLICT (${conflict}) DO UPDATE SET ${updates}`
      : `ON CONFLICT (${conflict}) DO NOTHING`;
    const query = `INSERT INTO ${table} (${colList}) VALUES (${placeholders}) ${onConflict}`;
    out.push({ query, params: cols.map((c) => (row as Row)[c]) });
  }
  return out;
}

/** RdbBatch を Postgres へ書き込む sink。 */
export class PostgresSink implements Sink<RdbBatch> {
  readonly name: string;
  readonly accepts = 'rdb' as const;

  constructor(private readonly exec: SqlExecutor, name = 'postgres') {
    this.name = name;
  }

  async write(batch: RdbBatch): Promise<void> {
    for (const u of batch.upserts) {
      for (const stmt of buildUpsertStatements(u)) {
        await this.exec(stmt.query, stmt.params);
      }
    }
  }
}
