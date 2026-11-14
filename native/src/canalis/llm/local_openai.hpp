#pragma once
// opt-in のローカル LLM executor — OpenAI 互換 `/v1/chat/completions` を叩く (src/llm/local-openai.ts の移植)。
// SDK も API キーの要求も無い (base URL の HTTP だけ)。Ollama / vLLM / LM Studio / llama.cpp server に共通で使える。
// 呼び出し先は利用者が設定したローカルのサービスなので、private network (loopback 等) への接続を許す (PORTING の規則)。
#include <optional>
#include <string>

#include "canalis/crawl/fetch.hpp"
#include "canalis/llm/types.hpp"

namespace canalis::llm {

struct LocalOpenAiOptions {
    /** OpenAI 互換のベース URL (末尾 /v1。例 http://localhost:11434/v1)。 */
    std::string base_url;
    /** 既定のモデル (例 gemma4:12b)。LlmRequest.model が優先。 */
    std::string model;
    /** 設定したときだけ Authorization: Bearer を送る (vLLM 等)。 */
    std::optional<std::string> api_key;
    /** 既定の期限 ms (既定 120000)。 */
    std::optional<std::int64_t> timeout_ms;
    /** 差し替え用 (テスト)。 */
    crawl::FetchFn fetch;
};

/** 失敗 (base URL / model の欠落・HTTP エラー・空の応答) は std::runtime_error。 */
LlmExecutor local_openai_executor(LocalOpenAiOptions options);

} // namespace canalis::llm
