// ③Save: KG sink — KgBatch を Kuzu (グラフ DB) へ MERGE する。
// driver は注入する (共有 lib は kuzu パッケージに直接依存しない)。
// node/edge は ② が出した最終形なので、 ここは MERGE 文に落として書くだけ。
//
// 前提: 対象 node/edge のスキーマ (テーブル) は事前に作成済であること (= サービスの責務)。
// Kuzu は厳格スキーマのため、 ノード/リレーションのテーブル定義はマイグレーションで用意する。

import type { GraphEdge, GraphNode, KgBatch } from '../core/envelope.js';
import type { Sink } from '../core/pipeline.js';

/**
 * Cypher 実行関数。 consumer が kuzu driver で実装して注入する。
 * 例: (q, p) => conn.query(q, p).then(() => {})
 */
export type CypherExecutor = (query: string, params: Record<string, unknown>) => Promise<void>;

export type KuzuSinkConfig = {
  /** ノードの一意キーを格納するプロパティ名 (MERGE キー)。 既定 'key'。 */
  keyProp?: string;
};

function assertSafeLabel(label: string): string {
  if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(label)) {
    throw new Error(`unsafe graph label: ${label}`);
  }
  return label;
}

/** props を `alias.k = $p_k` 形式の SET 句とパラメタ dict に展開する。Kuzu は SET n += $map 非対応のため。 */
function buildSetClauses(
  alias: string,
  props: Record<string, unknown>,
): { setClauses: string; params: Record<string, unknown> } {
  const entries = Object.entries(props);
  if (entries.length === 0) return { setClauses: '', params: {} };
  const params: Record<string, unknown> = {};
  const clauses = entries.map(([k, v]) => {
    const paramKey = `p_${k}`;
    params[paramKey] = v;
    return `${alias}.${k} = $${paramKey}`;
  });
  return { setClauses: clauses.join(', '), params };
}

/** KgBatch を Kuzu へ書き込む sink。 */
export class KuzuSink implements Sink<KgBatch> {
  readonly name: string;
  readonly accepts = 'kg' as const;
  private readonly keyProp: string;

  constructor(
    private readonly exec: CypherExecutor,
    config: KuzuSinkConfig = {},
    name = 'kuzu',
  ) {
    this.name = name;
    this.keyProp = config.keyProp ?? 'key';
  }

  async write(batch: KgBatch): Promise<void> {
    for (const node of batch.nodes) {
      await this.mergeNode(node);
    }
    for (const edge of batch.edges) {
      await this.mergeEdge(edge);
    }
  }

  private async mergeNode(node: GraphNode): Promise<void> {
    const label = assertSafeLabel(node.label);
    const props = node.props ?? {};
    const { setClauses, params } = buildSetClauses('n', props);
    const query =
      `MERGE (n:${label} {${this.keyProp}: $key})` +
      (setClauses ? ` SET ${setClauses}` : '');
    await this.exec(query, { key: node.key, ...params });
  }

  private async mergeEdge(edge: GraphEdge): Promise<void> {
    const fromLabel = assertSafeLabel(edge.from.label);
    const toLabel = assertSafeLabel(edge.to.label);
    const relLabel = assertSafeLabel(edge.label);
    const props = edge.props ?? {};
    const { setClauses, params } = buildSetClauses('r', props);
    const query =
      `MATCH (a:${fromLabel} {${this.keyProp}: $fromKey}), ` +
      `(b:${toLabel} {${this.keyProp}: $toKey}) ` +
      `MERGE (a)-[r:${relLabel}]->(b)` +
      (setClauses ? ` SET ${setClauses}` : '');
    await this.exec(query, {
      fromKey: edge.from.key,
      toKey: edge.to.key,
      ...params,
    });
  }
}
