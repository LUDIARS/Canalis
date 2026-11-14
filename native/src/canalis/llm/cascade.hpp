#pragma once
// カスケード: 安い段から順に試し、確信が出た段で打ち切る。LLM の段は最後の手段 (src/llm/cascade.ts の移植)。
// TS との違い: score_batch は 1 件ずつ順に評価する (TS は Promise.all で並行)。
#include <optional>
#include <string>
#include <vector>

#include "canalis/llm/types.hpp"

namespace canalis::llm {

struct CascadeOptions {
    /** 採用に要る最小の confidence。これ未満なら次の段へ。既定 0 (= 棄権でなければ採用)。 */
    std::optional<double> min_confidence;
};

class Cascade {
public:
    explicit Cascade(std::vector<TierScorer> tiers, CascadeOptions options = {});
    /** 1 件を評価する。どの段も確信に満たなければ最後の非棄権 (無ければ nullopt)。 */
    std::optional<ScoreResult> score(const ScoreInput& input) const;
    std::vector<std::optional<ScoreResult>> score_batch(const std::vector<ScoreInput>& inputs) const;

private:
    std::vector<TierScorer> tiers_;
    CascadeOptions options_;
};

/** カスケードの結果を FT (分類) の学習例 {input, label} にする。結果が無ければ nullopt。 */
std::optional<Json> to_classification_example(const std::string& text, const std::optional<ScoreResult>& result);

} // namespace canalis::llm
