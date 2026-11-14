#pragma once
// 安価な推論カスケードと opt-in のローカル LLM の契約 (src/llm/{types,local-openai}.ts の型の移植)。
// Canalis core (core / crawl / save) はこの llm を include しない。② (サービス側) が明示して使う (DESIGN §0.1)。
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "canalis/base/json.hpp"

namespace canalis::llm {

/** スコアリングの入力。labels を渡すと候補ラベルを絞れる (各段が解釈する)。signals は本文以外の手がかり (upvotes / emojis 等)。 */
struct ScoreInput {
    std::string text;
    Json signals = Json::object();
    std::optional<std::vector<std::string>> labels;
};

/** スコアリングの結果。tier は何段目が解決したか (0 = 辞書 / 信号、1 = ローカルモデル、2 = LLM)。 */
struct ScoreResult {
    std::string label;
    /** 0..1。カスケードのしきい値の判定に使う。 */
    double confidence = 0;
    int tier = 0;
    /** ラベル別のスコア (任意。キーの順は最初に出た順)。 */
    std::optional<Json> scores;
};

/** 1 段のスコアラ。nullopt = 棄権 (確信なし → 次の段へ)。 */
using TierScorer = std::function<std::optional<ScoreResult>(const ScoreInput& input)>;

/** ② が LLM へ渡す 1 リクエスト。 */
struct LlmRequest {
    std::string prompt;
    std::optional<std::string> system;
    std::optional<std::string> model;
    std::optional<int> max_tokens;
    std::optional<std::int64_t> timeout_ms;
};

struct LlmUsage {
    std::optional<std::int64_t> input_tokens;
    std::optional<std::int64_t> output_tokens;
};

/** LLM の応答。失敗は例外 (② 側で握って決定論の処理に倒す)。 */
struct LlmResponse {
    std::string text;
    std::optional<LlmUsage> usage;
};

/** 注入する LLM executor (SqlExecutor / CypherExecutor / FtRunner と同じく、ドライバは注入する)。 */
using LlmExecutor = std::function<LlmResponse(const LlmRequest& request)>;

} // namespace canalis::llm
