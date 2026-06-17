// Playwright を使った公開 Notion ページ取得の実装。
// テスト時は PageFetcher を差し替えて playwright を起動しない。

/// <reference lib="dom" />

import type { PageFetcher, FetchPageOptions, FetchedPage, ExtractionResult } from './types.js';
import { extractPageContent, blocksToMarkdown } from './extract.js';

export class PlaywrightFetcher implements PageFetcher {
  async fetch(url: string, options: FetchPageOptions): Promise<FetchedPage> {
    // playwright は動的 import — 未インストール時のモジュールロードエラーを防ぐ
    const { chromium } = await (import('playwright') as Promise<typeof import('playwright')>);

    const browser = await chromium.launch({ headless: true });
    try {
      const page = await browser.newPage();

      await page.goto(url, { waitUntil: 'networkidle', timeout: options.timeout });

      // Notion の遅延レンダリングが始まるまで待つ
      await page.waitForSelector('.notion-page-content', { timeout: options.timeout });

      // スクロールで lazy-load ブロックを展開
      await scrollToBottom(page, options.scrollDelay, options.maxScrolls);

      // 最終レンダリング待機
      await page.waitForTimeout(500);

      const result = await page.evaluate<ExtractionResult>(extractPageContent);
      const markdown = blocksToMarkdown(result.blocks);

      return { url, title: result.title, markdown, raw: result };
    } finally {
      await browser.close();
    }
  }
}

async function scrollToBottom(
  page: Awaited<ReturnType<import('playwright').Browser['newPage']>>,
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
