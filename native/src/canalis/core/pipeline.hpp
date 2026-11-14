#pragma once
// 3 ステージの抽象契約 (src/core/pipeline.ts の移植、段階 2 では ①Source だけ)。
//   ① Source   : 取得元から RawRecord を得る (汎用 — adapter を足せば source が増える)
//   ② Transform: サービス固有 (各リポが実装)。段階 5 で移す
//   ③ Sink     : 保存先へ verbatim 書込。段階 5 で移す
// TS の async は同期にする (呼んだスレッドで待つ)。失敗は例外 (TS の reject と同じ扱い)。
#include <string_view>
#include <vector>

#include "canalis/core/raw_record.hpp"

namespace canalis::core {

/** ①Crawl/Scrape — 取得元から生レコードを取得する。Config は adapter 固有の型。 */
template <class Config>
class Source {
public:
    virtual ~Source() = default;
    /** adapter 名。manifest の crawl.sources[].adapter と突き合わせる。 */
    virtual std::string_view name() const = 0;
    virtual std::vector<RawRecord> crawl(const Config& config) = 0;
};

} // namespace canalis::core
