#include "canalis/llm/cascade.hpp"

namespace canalis::llm {

Cascade::Cascade(std::vector<TierScorer> tiers, CascadeOptions options) : tiers_(std::move(tiers)), options_(options) {}

std::optional<ScoreResult> Cascade::score(const ScoreInput& input) const {
    const double min = options_.min_confidence.value_or(0);
    std::optional<ScoreResult> last;
    for (std::size_t i = 0; i < tiers_.size(); ++i) {
        auto raw = tiers_[i](input);
        if (!raw) continue;  // 棄権 → 次の段
        raw->tier = static_cast<int>(i);
        if (raw->confidence >= min) return raw;  // 確信あり → 打ち切る
        last = std::move(raw);                   // 確信は足りないが候補として残す
    }
    return last;
}

std::vector<std::optional<ScoreResult>> Cascade::score_batch(const std::vector<ScoreInput>& inputs) const {
    std::vector<std::optional<ScoreResult>> out;
    out.reserve(inputs.size());
    for (const auto& i : inputs) out.push_back(score(i));
    return out;
}

std::optional<Json> to_classification_example(const std::string& text, const std::optional<ScoreResult>& result) {
    if (!result) return std::nullopt;
    return Json{{"input", text}, {"label", result->label}};
}

} // namespace canalis::llm
