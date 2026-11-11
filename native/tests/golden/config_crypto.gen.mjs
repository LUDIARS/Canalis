// golden: TS 版 (src/config/crypto.ts) と同じ方式で暗号化した EncryptedBlob を fixtures に書き出す。
// C++ 版 (canalis::config::decrypt_json) が、TS 版の書いた config.json を読めることを確かめるために使う。
// 使い方: node native/tests/golden/config_crypto.gen.mjs  (Node の組み込みだけを使う。npm 依存なし)
// 方式は src/config/crypto.ts の encryptJson と同じ (scryptSync の既定係数 → AES-256-GCM、各値は base64)。
// crypto.ts を変えたら、ここも同じに直して生成し直す。

import { createCipheriv, randomBytes, scryptSync } from 'node:crypto';
import { writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

function encryptJson(value, masterSecret) {
  const salt = randomBytes(16);
  const iv = randomBytes(12);
  const key = scryptSync(masterSecret, salt, 32);
  const cipher = createCipheriv('aes-256-gcm', key, iv);
  const enc = Buffer.concat([cipher.update(Buffer.from(JSON.stringify(value), 'utf8')), cipher.final()]);
  return {
    v: 1,
    salt: salt.toString('base64'),
    iv: iv.toString('base64'),
    tag: cipher.getAuthTag().toString('base64'),
    data: enc.toString('base64'),
  };
}

// 明らかに偽物の値だけを使う (本物の token・鍵を書かない)。
const MASTER = 'test-master-key';
const cases = [
  { name: 'string-token', value: 'ntn_fake_token_for_golden_0000' },
  { name: 'unicode-object', value: { note: '日本語の値', n: 350, nested: [true, null] } },
];

const out = {
  generator: 'native/tests/golden/config_crypto.gen.mjs',
  master: MASTER,
  cases: cases.map((c) => ({ ...c, blob: encryptJson(c.value, MASTER) })),
  // TS 版の store が書く config.json の形 (writeConfig: JSON.stringify(cfg, null, 2) + "\n")。
  configJson:
    JSON.stringify(
      { notion: { version: '2022-06-28', tokenEnc: encryptJson('ntn_fake_config_token_1234', MASTER), minIntervalMs: 350 } },
      null,
      2,
    ) + '\n',
};

const here = dirname(fileURLToPath(import.meta.url));
writeFileSync(join(here, '..', 'fixtures', 'config', 'crypto_golden.json'), `${JSON.stringify(out, null, 2)}\n`);
