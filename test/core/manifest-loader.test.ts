import { describe, it, expect, beforeAll, afterAll } from 'vitest';
import { writeFile, unlink, mkdtemp } from 'node:fs/promises';
import { join } from 'node:path';
import { tmpdir } from 'node:os';
import { loadManifest } from '../../src/core/manifest-loader.js';

let dir: string;

beforeAll(async () => {
  dir = await mkdtemp(join(tmpdir(), 'canalis-test-'));
});

afterAll(async () => {
  // cleanup handled by OS tmpdir GC
});

describe('loadManifest', () => {
  it('loads a JSON manifest', async () => {
    const manifest = {
      pipeline: 'test-json',
      crawl: { sources: [{ adapter: 'notion', config: { databaseId: 'db1' } }] },
    };
    const path = join(dir, 'manifest.json');
    await writeFile(path, JSON.stringify(manifest), 'utf-8');
    const loaded = await loadManifest(path);
    expect(loaded.pipeline).toBe('test-json');
    expect(loaded.crawl.sources).toHaveLength(1);
    await unlink(path);
  });

  it('loads a YAML manifest', async () => {
    const yaml = `
pipeline: test-yaml
crawl:
  sources:
    - adapter: youtube
      config:
        videoId: vid1
        apiKey: key1
`;
    const path = join(dir, 'manifest.yaml');
    await writeFile(path, yaml, 'utf-8');
    const loaded = await loadManifest(path);
    expect(loaded.pipeline).toBe('test-yaml');
    expect((loaded.crawl.sources[0].config as Record<string, string>).videoId).toBe('vid1');
    await unlink(path);
  });

  it('loads a .yml manifest', async () => {
    const yaml = `pipeline: test-yml\ncrawl:\n  sources: []\n`;
    const path = join(dir, 'manifest.yml');
    await writeFile(path, yaml, 'utf-8');
    const loaded = await loadManifest(path);
    expect(loaded.pipeline).toBe('test-yml');
    await unlink(path);
  });

  it('throws if pipeline field is missing', async () => {
    const path = join(dir, 'bad.json');
    await writeFile(path, JSON.stringify({ crawl: { sources: [] } }), 'utf-8');
    await expect(loadManifest(path)).rejects.toThrow('pipeline');
    await unlink(path);
  });

  it('throws if file does not exist', async () => {
    await expect(loadManifest(join(dir, 'nonexistent.yaml'))).rejects.toThrow();
  });
});
