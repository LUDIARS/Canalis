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

/** ラベルが識別子として妥当か (Cypher のラベル/型名はパラメタ化できないため検証)。 */
function assertSafeLabel(label: string): string {
  if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(label)) {
    throw new Error(`unsafe graph label: ${label}`);
  }
  return label;
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
    const query =
      `MERGE (n:${label} {${this.keyProp}: $key}) ` +
      `SET n += $props`;
    await this.exec(query, { key: node.key, props: node.props ?? {} });
  }

  private async mergeEdge(edge: GraphEdge): Promise<void> {
    const fromLabel = assertSafeLabel(edge.from.label);
    const toLabel = assertSafeLabel(edge.to.label);
    const relLabel = assertSafeLabel(edge.label);
    const query =
      `MATCH (a:${fromLabel} {${this.keyProp}: $fromKey}), ` +
      `(b:${toLabel} {${this.keyProp}: $toKey}) ` +
      `MERGE (a)-[r:${relLabel}]->(b) SET r += $props`;
    await this.exec(query, {
      fromKey: edge.from.key,
      toKey: edge.to.key,
      props: edge.props ?? {},
    });
  }
}
