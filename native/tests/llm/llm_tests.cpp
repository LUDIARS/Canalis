// 推論カスケード・辞書のスコアラ・ローカル LLM (test/cascade.test.ts・test/local-openai.test.ts の観点)。LLM には繋がない。
#include "check.hpp"

#include <memory>

#include "canalis/llm/cascade.hpp"
#include "canalis/llm/lexicon.hpp"
#include "canalis/llm/local_openai.hpp"
#include "canalis/llm/scorer.hpp"

using namespace canalis;
using namespace canalis::llm;

namespace {

TierScorer lex() {
    LexiconConfig c;
    c.terms = parse_json_or_throw("{\"Amazing\":{\"joy\":1},\"boring\":{\"boredom\":1}}");
    c.emoji = parse_json_or_throw("{\"\xF0\x9F\x98\xA1\":{\"anger\":2}}");  // 😡
    return lexicon_scorer(c);
}

ScoreInput in(const std::string& text) { return ScoreInput{text, Json::object(), std::nullopt}; }

} // namespace

TEST_CASE("辞書: 語に当たれば label と tier 0、emoji の信号も拾い、無ければ棄権") {
    const auto r = lex()(in("an AMAZING run"));
    CHECK(r.has_value());
    CHECK_EQ(r->label, std::string("joy"));
    CHECK_EQ(r->tier, 0);
    CHECK(r->confidence > 0);
    ScoreInput angry = in("hmm");
    angry.signals = parse_json_or_throw("{\"emojis\":[\"\xF0\x9F\x98\xA1\"]}");
    CHECK_EQ(lex()(angry)->label, std::string("anger"));
    CHECK(!lex()(in("the quick brown fox")).has_value());
}

TEST_CASE("カスケード: Tier0 で取れたら LLM を呼ばず、棄権なら次の段が解決し、全段棄権なら無し") {
    int calls = 0;
    ExecutorScorerOptions o;
    o.executor = [&](const LlmRequest& req) {
        ++calls;
        CHECK(req.prompt.find("Allowed labels: joy, boredom") != std::string::npos);
        return LlmResponse{" Boredom.", std::nullopt};
    };
    o.labels = {"joy", "boredom"};
    const Cascade cascade({lex(), executor_scorer(o)});
    const auto first = cascade.score(in("amazing run"));
    CHECK_EQ(first->label, std::string("joy"));
    CHECK_EQ(first->tier, 0);
    CHECK_EQ(calls, 0);
    const auto second = cascade.score(in("meh"));
    CHECK_EQ(second->label, std::string("boredom"));
    CHECK_EQ(second->tier, 1);
    CHECK_EQ(second->confidence, 0.85);
    const Cascade none({lex()});
    CHECK(!none.score(in("no words")).has_value());
    CHECK_EQ(none.score_batch({in("amazing"), in("x")}).size(), std::size_t{2});
}

TEST_CASE("カスケード: 確信が足りなければ次へ、最後まで足りなければ最後の候補") {
    const TierScorer low = [](const ScoreInput&) { return std::optional<ScoreResult>(ScoreResult{"a", 0.3, 0, std::nullopt}); };
    const TierScorer mid = [](const ScoreInput&) { return std::optional<ScoreResult>(ScoreResult{"b", 0.6, 0, std::nullopt}); };
    CascadeOptions opt;
    opt.min_confidence = 0.9;
    const auto r = Cascade({low, mid}, opt).score(in("x"));
    CHECK_EQ(r->label, std::string("b"));
    CHECK_EQ(r->tier, 1);
}

TEST_CASE("to_classification_example") {
    CHECK_EQ(json_stringify(*to_classification_example("amazing run", ScoreResult{"joy", 1, 0, std::nullopt})),
             std::string("{\"input\":\"amazing run\",\"label\":\"joy\"}"));
    CHECK(!to_classification_example("x", std::nullopt).has_value());
}

TEST_CASE("ローカル LLM: chat/completions を叩き、content と usage を返す") {
    auto captured = std::make_shared<http::ClientRequest>();
    LocalOpenAiOptions o;
    o.base_url = "http://localhost:11434/v1/";
    o.model = "gemma4:12b";
    o.fetch = [captured](const http::ClientRequest& req) {
        *captured = req;
        http::ClientResponse res;
        res.transport_ok = true;
        res.status = 200;
        res.body = "{\"choices\":[{\"message\":{\"content\":\"  整形済みテキスト \"}}],\"usage\":{\"prompt_tokens\":12,\"completion_tokens\":8}}";
        return res;
    };
    LlmRequest req;
    req.prompt = "本文を要約";
    req.system = "簡潔に";
    const auto res = local_openai_executor(o)(req);
    CHECK_EQ(res.text, std::string("整形済みテキスト"));
    CHECK_EQ(res.usage->input_tokens.value_or(0), std::int64_t{12});
    CHECK_EQ(res.usage->output_tokens.value_or(0), std::int64_t{8});
    CHECK_EQ(captured->url, std::string("http://localhost:11434/v1/chat/completions"));
    CHECK(captured->allow_private_network);
    const Json body = parse_json_or_throw(captured->body);
    CHECK_EQ(json_string(body, "model"), std::string("gemma4:12b"));
    CHECK_EQ(json_int(body, "max_tokens"), std::int64_t{4096});
    CHECK_EQ(json_stringify(body["messages"]),
             std::string("[{\"role\":\"system\",\"content\":\"簡潔に\"},{\"role\":\"user\",\"content\":\"本文を要約\"}]"));
    CHECK(!captured->headers.has("authorization"));
    req.model = "b";
    local_openai_executor(o)(req);
    CHECK_EQ(json_string(parse_json_or_throw(captured->body), "model"), std::string("b"));
}

TEST_CASE("ローカル LLM: HTTP エラー・空の応答・baseUrl / model の欠落は失敗") {
    LocalOpenAiOptions o;
    o.base_url = "http://x/v1";
    o.model = "m";
    o.fetch = [](const http::ClientRequest&) {
        http::ClientResponse res;
        res.transport_ok = true;
        res.status = 404;
        res.body = "no model";
        return res;
    };
    try {
        local_openai_executor(o)(LlmRequest{"p", std::nullopt, std::nullopt, std::nullopt, std::nullopt});
        CHECK(false);
    } catch (const std::runtime_error& e) {
        CHECK(std::string(e.what()).find("404") != std::string::npos);
    }
    o.fetch = [](const http::ClientRequest&) {
        http::ClientResponse res;
        res.transport_ok = true;
        res.status = 200;
        res.body = "{\"choices\":[{\"message\":{\"content\":\"   \"}}]}";
        return res;
    };
    CHECK_THROWS(local_openai_executor(o)(LlmRequest{"p", std::nullopt, std::nullopt, std::nullopt, std::nullopt}));
    LocalOpenAiOptions no_base;
    no_base.model = "m";
    CHECK_THROWS(local_openai_executor(no_base)(LlmRequest{"p", std::nullopt, std::nullopt, std::nullopt, std::nullopt}));
    LocalOpenAiOptions no_model;
    no_model.base_url = "http://x/v1";
    CHECK_THROWS(local_openai_executor(no_model)(LlmRequest{"p", std::nullopt, std::nullopt, std::nullopt, std::nullopt}));
}

CI_TEST_MAIN()
