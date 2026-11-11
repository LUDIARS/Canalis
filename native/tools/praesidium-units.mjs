// Praesidium (Revisor のセキュリティ事前検査) が C++ の各翻訳単位をどの設定でコンパイルするかを praesidium.json に書く。
// git が追跡している native/ の *.cpp を並べる (追跡していないファイルを入れると検査が失敗するため)。
// 使い方: node native/tools/praesidium-units.mjs   (リポのルートで。ファイルを足したら実行し直す)
// ヘッダの場所は検査用ホストの praesidium.host.json に登録された名前で指す (リポからは場所を指定できない)。
// 名前と版は native/cmake/CanalisDependencies.cmake と同じ (Discutere native と共通の登録を使う)。

import { execFileSync } from 'node:child_process';
import { writeFileSync } from 'node:fs';

const PACKAGES = ['odeum-json-3-11-3', 'odeum-boost-1-87-0', 'odeum-openssl-3', 'discutere-lorica-cab4f120'];
// native/cmake/CanalisWarnings.cmake (MSVC) と CanalisDependencies.cmake (canalis_boost) の定義と同じ。
const DEFINES = [
  'WIN32_LEAN_AND_MEAN',
  'NOMINMAX',
  '_WIN32_WINNT=0x0A00',
  'BOOST_ERROR_CODE_HEADER_ONLY',
  'BOOST_ASIO_NO_DEPRECATED',
  'BOOST_BEAST_USE_STD_STRING_VIEW',
  'BOOST_BEAST_SEPARATE_COMPILATION',
];

const tracked = execFileSync('git', ['ls-files', '--', 'native'], { encoding: 'utf8' })
  .split('\n')
  .map((l) => l.trim())
  .filter((f) => /\.(?:c|cc|cpp|cxx)$/i.test(f))
  .sort();

const native = tracked.map((file) => ({
  file,
  standard: 'c++20',
  includes: file.startsWith('native/tests/') ? ['native/src', 'native/tests'] : ['native/src'],
  defines: DEFINES,
  packages: PACKAGES,
}));

writeFileSync('praesidium.json', `${JSON.stringify({ version: 1, native }, null, 2)}\n`);
console.log(`praesidium.json: ${native.length} units`);
