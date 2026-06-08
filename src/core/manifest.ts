// パイプラインを宣言する「フォーマット」。 各サービスはコードでなくこの manifest で流れを定義する。

import type { SinkKind } from './envelope.js';

/** 取得元 1 つの指定。 adapter 名 + adapter 固有 config。 */
export type SourceSpec = {
  adapter: string;
  config: unknown;
};

/** 保存先 1 つの指定。 adapter 名 + 受け付ける種別 + adapter 固有 config。 */
export type SinkSpec = {
  adapter: string;
  accepts: SinkKind;
  config?: unknown;
};

/** パイプライン宣言。 */
export type Manifest = {
  /** パイプライン名 (ログ/レポート用)。 */
  pipeline: string;
  crawl: {
    sources: SourceSpec[];
    /** 任意: ①の出力 RawRecord[] を verbatim 保存する raw sink。 */
    rawSave?: SinkSpec;
  };
  /** ②Transform の adapter 名。 省略時は raw 取得のみ (crawl + rawSave)。 */
  transform?: string;
  /** ③Save の保存先 (複数可)。 ②の各エンベロープを kind で対応 sink へ振り分ける。 */
  save?: SinkSpec[];
  /**
   * 任意: 再処理モード。 crawl の代わりに raw store から RawRecord[] を読み直して
   * ②→③ を焼き直す。 指定時は crawl.sources を起点にしない。
   */
  replayFrom?: SinkSpec;
};
