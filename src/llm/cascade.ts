// カスケード: 安い段から順に試し、 確信が出た段で打ち切る。 LLM 段は最後の手段。
// tiers = [Tier0(決定論), Tier1(ローカルモデル), Tier2(LLM)] の順で渡す (後ろほど高コスト)。
// 各段が abstain (null) なら次段へ。 解決した段の index を ScoreResult.tier に入れる。

import type { ScoreInput, ScoreResult, TierScorer } from './types.js';
import type { FtClassificationExample } from '../core/envelope.js';

export type CascadeOptions = {
  /** 採用に必要な最小 confidence。 これ未満なら次段へ委ねる。 既定 0 (=非null なら即採用)。 */
  minConfidence?: number;
};

/** 段を束ね、 安い順に評価する分類カスケード。 */
export class Cascade {
  constructor(
    private readonly tiers: TierScorer[],
    private readonly options: CascadeOptions = {},
  ) {}

  /** 1 件をスコアリング。 全段が確信に満たなければ最後の非null (無ければ null)。 */
  async score(input: ScoreInput): Promise<ScoreResult | null> {
    const min = this.options.minConfidence ?? 0;
    let last: ScoreResult | null = null;
    for (let i = 0; i < this.tiers.length; i++) {
      const raw = await this.tiers[i]!(input);
      if (!raw) continue; // abstain → 次段
      const result: ScoreResult = { ...raw, tier: i };
      if (result.confidence >= min) return result; // 確信あり → 打ち切り
      last = result; // 確信不足だが候補として保持
    }
    return last;
  }

  /** 複数件を並行スコアリング。 */
  async scoreBatch(inputs: ScoreInput[]): Promise<(ScoreResult | null)[]> {
    return Promise.all(inputs.map((i) => this.score(i)));
  }
}

/**
 * カスケード結果を FT(分類) の学習例に変換する。
 * tier フィルタで「どの段の結果を教師に使うか」を選べる (例: tier<=1 の高確信のみ silver、
 * tier===2 の LLM 解決を gold として FtBatch に流す → 自己改善ループ)。
 */
export function toClassificationExample(
  text: string,
  result: ScoreResult | null,
): FtClassificationExample | null {
  if (!result) return null;
  return { input: text, label: result.label };
}
