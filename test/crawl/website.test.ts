import { describe, it, expect, vi } from 'vitest';
import { WebsiteSource } from '../../src/crawl/website/source.js';

const STUB_HTML = `<!DOCTYPE html>
<html>
<head><title>Test Page &amp; More</title></head>
<body>
<p>Hello <strong>world</strong>!</p>
<a href="/about">About</a>
<a href="https://external.com/page">External</a>
<script>alert('skip me')</script>
</body>
</html>`;

function makeFetch(html: string, url = 'https://example.com/') {
  return vi.fn().mockResolvedValue({
    ok: true,
    url,
    text: async () => html,
  });
}

describe('WebsiteSource', () => {
  it('returns a RawRecord with parsed title and text', async () => {
    const fetch = makeFetch(STUB_HTML);
    const source = new WebsiteSource({ fetch: fetch as never, now: () => '2026-06-11T00:00:00Z' });
    const records = await source.crawl({ url: 'https://example.com/' });

    expect(records).toHaveLength(1);
    const r = records[0];
    expect(r.source).toBe('website');
    expect(r.title).toBe('Test Page & More');
    expect(r.text).toContain('Hello');
    expect(r.text).not.toContain('<p>');
    expect(r.text).not.toContain('alert');
    expect(r.url).toBe('https://example.com/');
  });

  it('extracts same-domain links only', async () => {
    const fetch = makeFetch(STUB_HTML);
    const source = new WebsiteSource({ fetch: fetch as never, now: () => '' });
    const [r] = await source.crawl({ url: 'https://example.com/' });
    const links = r.meta?.links as string[];
    expect(links.some((l) => l.includes('example.com/about'))).toBe(true);
    expect(links.some((l) => l.includes('external.com'))).toBe(false);
  });

  it('uses injected HtmlParser', async () => {
    const fetch = makeFetch('<html><body>raw</body></html>');
    const htmlParser = { parse: vi.fn().mockReturnValue({ title: 'Custom', text: 'parsed text', links: [] }) };
    const source = new WebsiteSource({ fetch: fetch as never, now: () => '', htmlParser });
    const [r] = await source.crawl({ url: 'https://example.com/' });
    expect(r.title).toBe('Custom');
    expect(r.text).toBe('parsed text');
    expect(htmlParser.parse).toHaveBeenCalledOnce();
  });

  it('throws on HTTP error', async () => {
    const fetch = vi.fn().mockResolvedValue({ ok: false, status: 404, url: 'https://example.com/' });
    const source = new WebsiteSource({ fetch: fetch as never, now: () => '' });
    await expect(source.crawl({ url: 'https://example.com/' })).rejects.toThrow('404');
  });

  it('throws on missing url', async () => {
    const source = new WebsiteSource();
    await expect(source.crawl({ url: '' })).rejects.toThrow('url');
  });
});
