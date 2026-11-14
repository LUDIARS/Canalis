#include "canalis/llm/scorer.hpp"

#include <stdexcept>

#include "canalis/base/strings.hpp"

namespace canalis::llm {
namespace {

constexpr std::string_view kDefaultSystem =
    "You are a strict text classifier. Reply with EXACTLY ONE label from the allowed list and nothing else.";

/** LLM の出力から候補ラベルを 1 つ拾う (完全一致を優先 → 部分一致)。 */
std::optional<std::string> match_label(const std::string& raw, const std::vector<std::string>& labels) {
    const std::string lower = str::to_lower(str::trim(raw));
    for (const auto& l : labels)
        if (lower == str::to_lower(l)) return l;
    for (const auto& l : labels)
        if (lower.find(str::to_lower(l)) != std::string::npos) return l;
    return std::nullopt;
}

} // namespace

TierScorer executor_scorer(ExecutorScorerOptions options) {
    if (!options.executor) throw std::invalid_argument("executorScorer: executor is required");
    return [options = std::move(options)](const ScoreInput& input) -> std::optional<ScoreResult> {
        const std::vector<std::string>& labels = input.labels ? *input.labels : options.labels;
        if (labels.empty()) throw std::invalid_argument("executorScorer: labels required (opts.labels or input.labels)");
        LlmRequest req;
        req.system = options.system.value_or(std::string(kDefaultSystem));
        req.model = options.model;
        req.prompt = "Allowed labels: " + str::join(labels, ", ") + "\n\nText:\n" + input.text + "\n\nLabel:";
        const auto picked = match_label(options.executor(req).text, labels);
        if (!picked) return std::nullopt;
        return ScoreResult{*picked, options.confidence.value_or(0.85), 0, std::nullopt};  // tier はカスケードが上書きする
    };
}

} // namespace canalis::llm
