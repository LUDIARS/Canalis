#include "canalis/llm/lexicon.hpp"

#include <limits>

#include "canalis/base/strings.hpp"

namespace canalis::llm {
namespace {

/** haystack 中の needle の出現回数 (重ならない部分一致。TS の split(needle).length - 1)。 */
std::size_t count_occurrences(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return 0;
    std::size_t n = 0;
    for (auto at = haystack.find(needle); at != std::string_view::npos; at = haystack.find(needle, at + needle.size())) ++n;
    return n;
}

double sum_weights(const Json& weights) {
    double s = 0;
    for (const auto& w : weights)
        if (w.is_number()) s += w.get<double>();
    return s;
}

void add_weights(Json& into, const Json& weights, std::size_t times) {
    for (const auto& [label, w] : weights.items()) {
        if (!w.is_number()) continue;
        const double prev = into.contains(label) ? into[label].get<double>() : 0.0;
        into[label] = prev + w.get<double>() * static_cast<double>(times);
    }
}

} // namespace

TierScorer lexicon_scorer(LexiconConfig config) {
    const bool ci = config.case_insensitive.value_or(true);
    const double min_weight = config.min_weight.value_or(1);
    // 用語の辞書は大文字小文字の正規化を前もって済ませる。
    Json terms = Json::object();
    for (const auto& [k, v] : config.terms.items())
        if (v.is_object()) terms[ci ? str::to_lower(k) : k] = v;
    Json emoji = config.emoji.is_object() ? config.emoji : Json::object();

    return [terms = std::move(terms), emoji = std::move(emoji), ci, min_weight](const ScoreInput& input) -> std::optional<ScoreResult> {
        const std::string text = ci ? str::to_lower(input.text) : input.text;
        Json scores = Json::object();
        double total = 0;
        for (const auto& [term, weights] : terms.items()) {
            const auto n = count_occurrences(text, term);
            if (n > 0) {
                add_weights(scores, weights, n);
                total += sum_weights(weights) * static_cast<double>(n);
            }
        }
        // emoji は本文と signals.emojis の両方。emoji は大文字小文字の正規化をしない。
        std::vector<std::string> sources{input.text};
        for (const auto& e : json_array(input.signals, "emojis"))
            if (e.is_string()) sources.push_back(e.get<std::string>());
        const std::string emoji_source = str::join(sources, " ");
        for (const auto& [glyph, weights] : emoji.items()) {
            if (!weights.is_object()) continue;
            const auto n = count_occurrences(emoji_source, glyph);
            if (n > 0) {
                add_weights(scores, weights, n);
                total += sum_weights(weights) * static_cast<double>(n);
            }
        }
        if (total < min_weight) return std::nullopt;  // 棄権 → 次の段へ
        std::string best;
        double best_value = -std::numeric_limits<double>::infinity();
        for (const auto& [label, v] : scores.items()) {
            if (v.get<double>() > best_value) {
                best = label;
                best_value = v.get<double>();
            }
        }
        if (best.empty() && scores.empty()) return std::nullopt;
        return ScoreResult{best, best_value / total, 0, scores};
    };
}

} // namespace canalis::llm
