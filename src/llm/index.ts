// `@ludiars/canalis/llm` — 安価推論カスケード + opt-in ローカル LLM executor 群。
// 鉄則 (DESIGN §0.1): core/crawl/save の公開面 (root export) には混ぜない。
// LLM を使うサービス② が明示的にこのサブパスから import して DI する。
//
// 構成: Tier0=lexiconScorer(決定論・LLM非依存) / Tier1・2=executorScorer(注入 LlmExecutor) /
//        Cascade が安い順に束ねる。 toClassificationExample で結果を FT 教師に変換できる。
export * from './local-openai.js';
export type { Signals, ScoreInput, ScoreResult, TierScorer } from './types.js';
export { lexiconScorer, type LexiconConfig, type WeightMap } from './lexicon.js';
export { executorScorer, type ExecutorScorerOptions } from './scorer.js';
export { Cascade, type CascadeOptions, toClassificationExample } from './cascade.js';
