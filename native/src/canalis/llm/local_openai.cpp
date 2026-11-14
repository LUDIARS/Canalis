#include "canalis/llm/local_openai.hpp"

#include <stdexcept>

#include "canalis/base/strings.hpp"

namespace canalis::llm {
namespace {

// reasoning モデル (Gemma 4 等) は思考でトークンを使うので、小さいと最終回答 (content) が空になる。TS と同じ既定。
constexpr int kDefaultMaxTokens = 4096;
constexpr std::int64_t kDefaultTimeoutMs = 120'000;

} // namespace

LlmExecutor local_openai_executor(LocalOpenAiOptions options) {
    std::string base = options.base_url;
    while (!base.empty() && base.back() == '/') base.pop_back();
    crawl::FetchFn fetch = options.fetch ? options.fetch : crawl::FetchFn(crawl::default_fetch);
    return [base, options = std::move(options), fetch](const LlmRequest& req) -> LlmResponse {
        if (base.empty()) throw std::runtime_error("local LLM baseUrl not set");
        const std::string model = req.model.value_or(options.model);
        if (model.empty()) throw std::runtime_error("local LLM model not set");

        Json messages = Json::array();
        if (req.system && !req.system->empty()) messages.push_back(Json{{"role", "system"}, {"content", *req.system}});
        messages.push_back(Json{{"role", "user"}, {"content", req.prompt}});
        Json body = Json::object();
        body["model"] = model;
        body["max_tokens"] = req.max_tokens.value_or(kDefaultMaxTokens);
        body["messages"] = std::move(messages);
        body["stream"] = false;

        http::ClientRequest http_req;
        http_req.method = "POST";
        http_req.url = base + "/chat/completions";
        http_req.headers.set("content-type", "application/json");
        if (options.api_key && !options.api_key->empty()) http_req.headers.set("authorization", "Bearer " + *options.api_key);
        http_req.body = json_stringify(body);
        http_req.timeout = std::chrono::milliseconds(req.timeout_ms.value_or(options.timeout_ms.value_or(kDefaultTimeoutMs)));
        http_req.allow_private_network = true;  // 利用者が設定したローカルのサービス
        http_req.max_redirects = 0;

        const auto res = fetch(http_req);
        if (!res.transport_ok) throw std::runtime_error("local LLM request failed: " + res.error);
        if (!res.ok()) throw std::runtime_error("local LLM http " + std::to_string(res.status) + ": " + str::js_slice(res.body, 0, 200));
        const Json json = crawl::parse_response_json(res, "local LLM");
        const Json& choices = json_array(json, "choices");
        const std::string text = choices.empty() ? "" : str::trim(json_string(json_object(choices[0], "message"), "content"));
        if (text.empty()) throw std::runtime_error("local LLM empty text response");
        LlmResponse out;
        out.text = text;
        if (const auto u = json.find("usage"); u != json.end() && u->is_object()) {
            LlmUsage usage;
            if (const auto n = json_opt_number(*u, "prompt_tokens")) usage.input_tokens = static_cast<std::int64_t>(*n);
            if (const auto n = json_opt_number(*u, "completion_tokens")) usage.output_tokens = static_cast<std::int64_t>(*n);
            out.usage = usage;
        }
        return out;
    };
}

} // namespace canalis::llm
