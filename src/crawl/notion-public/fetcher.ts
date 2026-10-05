// Playwright を使った公開 Notion ページ取得の実装。
// テスト時は PageFetcher を差し替えて playwright を起動しない。
// ブラウザは初回 fetch で起動して使い回す (複数ページ取得を速くする)。 使い終わったら close()。

/// <reference lib="dom" />

import type { Browser } from 'playwright';
import type { PageFetcher, FetchPageOptions, FetchedPage, ExtractionResult } from './types.js';
import { extractPageContent, blocksToMarkdown } from './extract.js';

export class PlaywrightFetcher implements PageFetcher {
  private browser: Promise<Browser> | null = null;

  async fetch(url: string, options: FetchPageOptions): Promise<FetchedPage> {
    const browser = await this.launch();
    const page = await browser.newPage();
    try {
      // Notion は通信を張り続けるため networkidle には到達しない。 HTML 読込後に本文要素の出現を待つ。
      await page.goto(url, { waitUntil: 'domcontentloaded', timeout: options.timeout });

      // Notion の遅延レンダリングが始まるまで待つ (空ページは非表示のことがあるので attached で判定)
      try {
        await page.waitForSelector('.notion-page-content', { state: 'attached', timeout: options.timeout });
      } catch (err) {
        // Cloudflare のボット確認画面に当たった場合は、 タイムアウトと区別できるよう明示する。
        const title = await page.title().catch(() => '');
        if (isBotChallengeTitle(title)) {
          throw new Error(`Notion のボット確認画面で止められました (title="${title}"): ${url}`);
        }
        throw err;
      }

      // スクロールで lazy-load ブロックを展開
      await scrollToBottom(page, options.scrollDelay, options.maxScrolls);

      // 最終レンダリング待機
      await page.waitForTimeout(500);

      const result = await page.evaluate<ExtractionResult>(extractPageContent);
      const markdown = blocksToMarkdown(result.blocks);

      return { url, title: result.title, markdown, raw: result };
    } finally {
      await page.close();
    }
  }

  async close(): Promise<void> {
    const browser = this.browser;
    this.browser = null;
    if (browser) await (await browser).close();
  }

  private launch(): Promise<Browser> {
    if (!this.browser) {
      // playwright は動的 import — 未インストール時のモジュールロードエラーを防ぐ
      this.browser = (import('playwright') as Promise<typeof import('playwright')>).then(({ chromium }) =>
        chromium.launch({ headless: true }),
      );
      this.browser.catch(() => {
        this.browser = null;
      });
    }
    return this.browser;
  }
}

/** Cloudflare のボット確認画面のタイトルか。 */
export function isBotChallengeTitle(title: string): boolean {
  return /^(just a moment|attention required|しばらくお待ちください)/i.test(title.trim());
}

async function scrollToBottom(
  page: Awaited<ReturnType<Browser['newPage']>>,
  scrollDelay: number,
  maxScrolls: number,
): Promise<void> {
  for (let i = 0; i < maxScrolls; i++) {
    const prevHeight = await page.evaluate(() => document.documentElement.scrollHeight);
    await page.evaluate(() => window.scrollTo(0, document.documentElement.scrollHeight));
    await page.waitForTimeout(scrollDelay);
    const newHeight = await page.evaluate(() => document.documentElement.scrollHeight);
    if (newHeight === prevHeight) break;
  }
  // ページ先頭に戻す (ブロック順序を安定させる)
  await page.evaluate(() => window.scrollTo(0, 0));
}
