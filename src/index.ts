// Canalis — LUDIARS 共有データ取込パイプライン基盤。
// crawl/scrape (①) → clean/categorize (②) → save/import (③) を契約で接続する。
//
// 公開面:
//   - core   : 契約 (RawRecord / エンベロープ / Source・Transform・Sink / Manifest)
//   - notion : ①Crawl adapter (Tirocinium から移植)
//   - save   : ③Save writer (raw / postgres / kuzu)
//   - runner : manifest 実行オーケストレータ
//   - config : リポ内 config.json による設定 (notion.token は暗号化)
//
// ②Transform は各サービスリポに置く (共有 lib は 1 サービス専用ロジックを持たない)。

export * from './core/index.js';
export * from './crawl/notion/index.js';
export * from './crawl/youtube/index.js';
export * from './crawl/reddit/index.js';
export * from './crawl/website/index.js';
export * from './crawl/notion-public/index.js';
export * from './save/index.js';
export {
  runPipeline,
  type PipelineDeps,
  type RunReport,
  type StageError,
} from './runner/runner.js';
export {
  configPath,
  resolveNotionToken,
  applyNotionConfigToEnv,
  getConfigStatus,
  type CanalisConfig,
  type ConfigStatus,
} from './config/index.js';
