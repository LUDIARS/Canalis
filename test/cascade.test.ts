// 0+1 カスケードの検証: Tier0(決定論辞書) で取れれば LLM を呼ばない、
// 取れなければ次段(注入 executor)へ委ね、 解決した tier を記録する。

import { describe, it, expect, vi } from 'vitest';
import { lexiconScorer } from '../src/llm/lexicon.js';
import { executorScorer } from '../src/llm/scorer.js';
import { Cascade, toClassificationExample } from '../src/llm/cascade.js';
import type { LlmExecutor } from '../src/llm/local-openai.js';

const LEX = lexiconScorer({
  terms: { amazing: { joy: 1 }, boring: { boredom: 1 } },
  emoji: { '😡': { anger: 2 } },
});

describe('lexiconScorer (Tier0, 決定論)', () => {
  it('辞書語に当たれば label + tier0', () => {
    const r = LEX({ text: 'this game is amazing' });
    expect(r).toMatchObject({ label: 'joy', tier: 0 });
    expect(r!.confidence).toBeGreaterThan(0);
  });
  it('emoji 信号も拾う', () => {
    const r = LEX({ text: 'meh', signals: { emojis: ['😡'] } });
    expect(r?.label).toBe('anger');
  });
  it('語が無ければ abstain (null)', () => {
    expect(LEX({ text: 'the quick brown fox' })).toBeNull();
  });
});

describe('Cascade (安い順・LLM は最後)', () => {
  it('Tier0 で取れたら LLM executor を呼ばない', async () => {
    const exec = vi.fn<Parameters<LlmExecutor>, ReturnType<LlmExecutor>>(
      async () => ({ text: 'joy' }),
    );
    const cascade = new Cascade([LEX, executorScorer({ executor: exec, labels: ['joy', 'boredom'] })]);
    const r = await cascade.score({ text: 'amazing run' });
    expect(r).toMatchObject({ label: 'joy', tier: 0 });
    expect(exec).not.toHaveBeenCalled();
  });

  it('Tier0 が abstain なら Tier1(executor) が解決し tier=1', async () => {
    const exec: LlmExecutor = async () => ({ text: 'boredom' });
    const cascade = new Cascade([LEX, executorScorer({ executor: exec, labels: ['joy', 'boredom'] })]);
    const r = await cascade.score({ text: 'no lexicon words here' });
    expect(r).toMatchObject({ label: 'boredom', tier: 1 });
  });

  it('全段 abstain なら null', async () => {
    const exec: LlmExecutor = async () => ({ text: 'totally-unknown-label' });
    const cascade = new Cascade([LEX, executorScorer({ executor: exec, labels: ['joy', 'boredom'] })]);
    expect(await cascade.score({ text: 'no words' })).toBeNull();
  });
});

describe('toClassificationExample (FT 教師化)', () => {
  it('結果を {input,label} に変換', () => {
    const ex = toClassificationExample('amazing run', { label: 'joy', confidence: 1, tier: 0 });
    expect(ex).toEqual({ input: 'amazing run', label: 'joy' });
  });
  it('null は null', () => {
    expect(toClassificationExample('x', null)).toBeNull();
  });
});
