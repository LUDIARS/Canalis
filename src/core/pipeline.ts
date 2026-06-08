// 3 ステージの抽象契約。 実体 (adapter / plugin) はこの interface を実装する。
//
//   ① Source   : 取得元から RawRecord[] を得る            (汎用 — adapter を足せば source 増)
//   ② Transform: RawRecord[] を sink ネイティブ形へ整形    (サービス固有 — 各リポが実装)
//   ③ Sink     : raw / エンベロープを保存先へ verbatim 書込 (汎用 — writer を足せば sink 増)

import type { RawRecord } from './raw-record.js';
import type { SinkBatch, SinkKind } from './envelope.js';

/** ①Crawl/Scrape — 取得元から生レコードを取得する。 config は adapter 固有。 */
export interface Source<C = unknown> {
  /** adapter 名。 manifest の crawl.sources[].adapter と突合する。 */
  readonly name: string;
  crawl(config: C): Promise<RawRecord[]>;
}

/**
 * ②Clean/Categorize — RawRecord[] を sink ネイティブのエンベロープへ整形する。
 * 1 つの入力から複数 sink 向けエンベロープを出してよい (例: Tr = RdbBatch + KgBatch)。
 * 実体はサービスリポ側に置く (共有 lib は契約のみ)。
 */
export interface Transform<O extends SinkBatch = SinkBatch> {
  readonly name: string;
  run(records: RawRecord[]): Promise<O[]>;
}

/**
 * ③Save/Import — 渡されたものを保存先へ verbatim 書き込む。
 * accepts が 'raw' の sink は RawRecord[] を、 それ以外は対応する SinkBatch を受ける。
 */
export interface Sink<P = unknown> {
  readonly name: string;
  /** この sink が受け付けるペイロード種別。 runner のルーティングに使う。 */
  readonly accepts: SinkKind;
  write(payload: P): Promise<void>;
}
