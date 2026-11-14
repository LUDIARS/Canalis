#pragma once
// Tier0: 決定論のスコアラ (辞書 + 信号) (src/llm/lexicon.ts の移植)。LLM・外部依存なし。
// 用語 / emoji のラベル別の重みを足し、最大のラベルを返す。合計がしきい値未満なら棄権。辞書はサービスが注入する。
#include <optional>

#include "canalis/llm/types.hpp"

namespace canalis::llm {

struct LexiconConfig {
    /** 用語 → { ラベル: 重み }。例: { "amazing": { "joy": 1 } } (キーの順が同点のときの順になる) */
    Json terms = Json::object();
    /** emoji → { ラベル: 重み }。本文と signals.emojis の両方に当てる。大文字小文字の区別はそのまま。 */
    Json emoji = Json::object();
    /** 採用に要る最小の合計の重み。既定 1。 */
    std::optional<double> min_weight;
    /** 大文字小文字を無視するか。既定 true。 */
    std::optional<bool> case_insensitive;
};

TierScorer lexicon_scorer(LexiconConfig config);

} // namespace canalis::llm
