// `@ludiars/canalis/llm` — ②Transform が opt-in で注入するローカル LLM executor 群。
// 鉄則 (DESIGN §0.1): core/crawl/save の公開面 (root export) には混ぜない。
// LLM を使うサービス② が明示的にこのサブパスから import して DI する。
export * from './local-openai.js';
