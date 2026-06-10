// config ストアの検証: notion.token は暗号化往復で復元でき、 非シークレットは平文、
// 解決順 (env NOTION_TOKEN 優先 → config)、 表示はマスクされること。

import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import { mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { encryptJson, decryptJson, isEncryptedBlob } from '../src/config/crypto.js';

const MASTER = 'test-master-key';

describe('crypto (AES-256-GCM round-trip)', () => {
  it('暗号化→復号で元の値に戻る', () => {
    const blob = encryptJson('secret_abc123', MASTER);
    expect(isEncryptedBlob(blob)).toBe(true);
    expect(JSON.stringify(blob)).not.toContain('secret_abc123'); // 平文を含まない
    expect(decryptJson<string>(blob, MASTER)).toBe('secret_abc123');
  });

  it('master 鍵違いは復号に失敗 (throw)', () => {
    const blob = encryptJson('x', MASTER);
    expect(() => decryptJson(blob, 'wrong-key')).toThrow();
  });
});

describe('config store (notion.token 暗号化 + 解決)', () => {
  let dir: string;
  let cfgPath: string;
  const saved = { ...process.env };

  beforeEach(() => {
    dir = mkdtempSync(join(tmpdir(), 'canalis-cfg-'));
    cfgPath = join(dir, 'config.json');
    process.env.CANALIS_CONFIG_PATH = cfgPath;
    process.env.CANALIS_MASTER_KEY = MASTER;
    delete process.env.NOTION_TOKEN;
    delete process.env.NOTION_VERSION;
    delete process.env.NOTION_MIN_INTERVAL_MS;
  });

  afterEach(() => {
    process.env = { ...saved };
    rmSync(dir, { recursive: true, force: true });
  });

  it('token を暗号化保存し、 ファイルに平文を残さない', async () => {
    const { setNotionToken, getStoredToken } = await import('../src/config/store.js');
    setNotionToken('ntn_topsecret');
    const raw = readFileSync(cfgPath, 'utf8');
    expect(raw).not.toContain('ntn_topsecret');
    expect(raw).toContain('tokenEnc');
    expect(getStoredToken()).toBe('ntn_topsecret');
  });

  it('resolveNotionToken は env を優先、 無ければ config', async () => {
    const { setNotionToken, resolveNotionToken } = await import('../src/config/store.js');
    setNotionToken('from_config');
    expect(resolveNotionToken()).toBe('from_config');
    process.env.NOTION_TOKEN = 'from_env';
    expect(resolveNotionToken()).toBe('from_env');
  });

  it('非シークレット (version) は平文で保存され env へ反映 (既存 env 優先)', async () => {
    const { setNotionVersion, applyNotionConfigToEnv } = await import('../src/config/store.js');
    setNotionVersion('2099-01-01');
    expect(readFileSync(cfgPath, 'utf8')).toContain('2099-01-01'); // 平文
    applyNotionConfigToEnv();
    expect(process.env.NOTION_VERSION).toBe('2099-01-01');
    // 既存 env は上書きしない
    process.env.NOTION_VERSION = 'keep-me';
    applyNotionConfigToEnv();
    expect(process.env.NOTION_VERSION).toBe('keep-me');
  });

  it('getConfigStatus は token をマスクし source を示す', async () => {
    const { setNotionToken, getConfigStatus } = await import('../src/config/store.js');
    setNotionToken('ntn_abcd1234');
    const s = getConfigStatus();
    expect(s.tokenSource).toBe('config');
    expect(s.tokenHint).toBe('…1234');
    expect(JSON.stringify(s)).not.toContain('ntn_abcd1234'); // 平文を返さない
  });
});
