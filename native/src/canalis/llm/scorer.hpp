#pragma once
// 注入した LlmExecutor をカスケードの 1 段 (TierScorer) につなぐ = LLM を「分類器」として使う段 (src/llm/scorer.ts の移植)。
#include <optional>
#include <string>
#include <vector>

#include "canalis/llm/types.hpp"

namespace canalis::llm {

struct ExecutorScorerOptions {
    LlmExecutor executor;
    /** 候補ラベルの既定 (ScoreInput.labels が優先)。どちらも無ければ評価のときに std::invalid_argument。 */
    std::vector<std::string> labels;
    std::optional<std::string> system;
    std::optional<std::string> model;
    /** ラベルが取れたときの confidence (LLM は確率を返さないので固定。既定 0.85)。 */
    std::optional<double> confidence;
};

/** 候補外の応答は棄権 (nullopt) → 次の段へ。 */
TierScorer executor_scorer(ExecutorScorerOptions options);

} // namespace canalis::llm
