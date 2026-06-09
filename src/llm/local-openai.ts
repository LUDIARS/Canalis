// opt-in ローカル LLM executor — OpenAI 互換 `/v1/chat/completions` を叩く注入ヘルパ。
//
// 設計鉄則 (DESIGN §0.1): Canalis core は LLM を知らない・呼ばない。 これは
//   ②Transform (サービス側) が **opt-in で注入**するためのヘルパであり、 別エントリ
//   (`@ludiars/canalis/llm`) に隔離して root の公開面 (core/crawl/save) には混ぜない。
// SDK 依存も API キー要求も無い (raw fetch + baseUrl のみ) ので「共有 lib に LLM 依存を
//   足さない」 に抵触しない (commandRunner が child_process だけの runner ヘルパなのと同じ立て付け)。
// Ollama / vLLM / LM Studio / llama.cpp server が公開する OpenAI 互換 API に共通対応
//   (例: ローカル Gemma 4 12B = Ollama `http://localhost:11434/v1` / model `gemma4:12b`)。

/** ②が LLM へ渡す 1 リクエスト。 */
export type LlmRequest = {
  prompt: string;
  /** 固定指示 (system message)。 */
  system?: string;
  /** モデル名上書き (未指定なら executor 既定)。 */
  model?: string;
  maxTokens?: number;
  /** 応答待ちタイムアウト ms (既定 120000)。 */
  timeoutMs?: number;
};

/** LLM 応答。 失敗は throw (②側で握って heuristic にフォールバックする)。 */
export type LlmResponse = {
  text: string;
  usage?: { inputTokens?: number; outputTokens?: number };
};

/**
 * 注入型 LLM executor。 ②Transform が DI で受け取り opt-in 利用する関数 contract。
 * SqlExecutor / CypherExecutor / FtRunner と同じ「driver は注入」 思想。
 */
export type LlmExecutor = (req: LlmRequest) => Promise<LlmResponse>;

export type LocalOpenAiOptions = {
  /** OpenAI 互換ベース URL (末尾 `/v1`、 例 `http://localhost:11434/v1`)。 */
  baseUrl: string;
  /** 既定モデル (例 `gemma4:12b`)。 req.model が優先。 */
  model: string;
  /** 任意。 設定時のみ `Authorization: Bearer` を送る (vLLM 等)。 Ollama は不要。 */
  apiKey?: string;
  /** 既定タイムアウト ms (既定 120000)。 */
  timeoutMs?: number;
  /** テスト用 fetch 差し替え。 */
  fetchImpl?: typeof fetch;
};

type OpenAiChatResponse = {
  choices?: { message?: { content?: string } }[];
  usage?: { prompt_tokens?: number; completion_tokens?: number };
};

const DEFAULT_MAX_TOKENS = 1024;
const DEFAULT_TIMEOUT_MS = 120_000;

/**
 * ローカル OpenAI 互換エンドポイント用の {@link LlmExecutor} を作る。
 * 失敗 (未起動 / HTTP エラー / 空応答) は Error を throw する。
 */
export function localOpenAiExecutor(opts: LocalOpenAiOptions): LlmExecutor {
  const baseUrl = (opts.baseUrl ?? '').replace(/\/+$/, '');
  const doFetch = opts.fetchImpl ?? fetch;
  const defaultTimeout = opts.timeoutMs ?? DEFAULT_TIMEOUT_MS;

  return async (req: LlmRequest): Promise<LlmResponse> => {
    if (!baseUrl) throw new Error('local LLM baseUrl not set');
    const model = req.model ?? opts.model;
    if (!model) throw new Error('local LLM model not set');

    const messages: { role: string; content: string }[] = [];
    if (req.system) messages.push({ role: 'system', content: req.system });
    messages.push({ role: 'user', content: req.prompt });

    const headers: Record<string, string> = { 'content-type': 'application/json' };
    if (opts.apiKey) headers.authorization = `Bearer ${opts.apiKey}`;

    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), req.timeoutMs ?? defaultTimeout);
    try {
      const res = await doFetch(`${baseUrl}/chat/completions`, {
        method: 'POST',
        headers,
        body: JSON.stringify({
          model,
          max_tokens: req.maxTokens ?? DEFAULT_MAX_TOKENS,
          messages,
          stream: false,
        }),
        signal: controller.signal,
      });
      if (!res.ok) {
        const body = await res.text().catch(() => '');
        throw new Error(`local LLM http ${res.status}: ${body.slice(0, 200)}`);
      }
      const json = (await res.json()) as OpenAiChatResponse;
      const text = (json.choices?.[0]?.message?.content ?? '').trim();
      if (text.length === 0) throw new Error('local LLM empty text response');
      return {
        text,
        usage: json.usage
          ? { inputTokens: json.usage.prompt_tokens, outputTokens: json.usage.completion_tokens }
          : undefined,
      };
    } finally {
      clearTimeout(timer);
    }
  };
}
