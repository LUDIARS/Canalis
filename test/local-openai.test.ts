// opt-in ローカル LLM executor (`@ludiars/canalis/llm`) のテスト。
// fetch を差し替え、 chat/completions のリクエスト整形・応答解釈・usage マップ・
// エラー処理を実エンドポイント無しで検証する。

import { describe, it, expect } from 'vitest';
import { localOpenAiExecutor } from '../src/llm/local-openai.js';

function jsonResponse(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), {
    status,
    headers: { 'content-type': 'application/json' },
  });
}

describe('localOpenAiExecutor', () => {
  it('chat/completions を叩き content を返し usage をマップする', async () => {
    let captured: { url: string; body: any } | null = null;
    const fetchImpl = (async (url: string | URL | Request, init?: RequestInit) => {
      captured = { url: String(url), body: JSON.parse(String(init?.body)) };
      return jsonResponse({
        choices: [{ message: { content: '  整形済みテキスト  ' } }],
        usage: { prompt_tokens: 12, completion_tokens: 8 },
      });
    }) as unknown as typeof fetch;

    const exec = localOpenAiExecutor({
      baseUrl: 'http://localhost:11434/v1/',
      model: 'gemma4:12b',
      fetchImpl,
    });
    const res = await exec({ prompt: '本文を要約', system: '簡潔に' });

    expect(res.text).toBe('整形済みテキスト');
    expect(res.usage).toEqual({ inputTokens: 12, outputTokens: 8 });
    expect(captured!.url).toBe('http://localhost:11434/v1/chat/completions');
    expect(captured!.body.model).toBe('gemma4:12b');
    expect(captured!.body.messages[0]).toEqual({ role: 'system', content: '簡潔に' });
    expect(captured!.body.messages[1]).toEqual({ role: 'user', content: '本文を要約' });
  });

  it('req.model が executor 既定より優先される', async () => {
    let model = '';
    const fetchImpl = (async (_u: unknown, init?: RequestInit) => {
      model = JSON.parse(String(init?.body)).model;
      return jsonResponse({ choices: [{ message: { content: 'x' } }] });
    }) as unknown as typeof fetch;
    const exec = localOpenAiExecutor({ baseUrl: 'http://x/v1', model: 'a', fetchImpl });
    await exec({ prompt: 'p', model: 'b' });
    expect(model).toBe('b');
  });

  it('HTTP エラーは throw する', async () => {
    const fetchImpl = (async () => jsonResponse({ error: 'not found' }, 404)) as unknown as typeof fetch;
    const exec = localOpenAiExecutor({ baseUrl: 'http://x/v1', model: 'm', fetchImpl });
    await expect(exec({ prompt: 'p' })).rejects.toThrow(/404/);
  });

  it('baseUrl / model 欠落は呼ぶ前に throw する', async () => {
    await expect(localOpenAiExecutor({ baseUrl: '', model: 'm' })({ prompt: 'p' })).rejects.toThrow(/baseUrl/);
    await expect(localOpenAiExecutor({ baseUrl: 'http://x/v1', model: '' })({ prompt: 'p' })).rejects.toThrow(
      /model/,
    );
  });
});
