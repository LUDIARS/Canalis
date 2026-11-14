#pragma once
// 3 ステージの抽象契約 (src/core/pipeline.ts の移植)。
//   ① Source   : 取得元から RawRecord を得る (汎用 — adapter を足せば source が増える)
//   ② Transform: RawRecord を sink ネイティブのエンベロープ (SinkBatch) に整形する (サービス固有 — 各リポが実装)
//   ③ Sink     : raw / エンベロープを保存先へ verbatim で書く (汎用 — writer を足せば sink が増える)
// TS の async は同期にする (呼んだスレッドで待つ)。失敗は例外 (TS の reject と同じ扱い)。
#include <string_view>
#include <vector>

#include "canalis/base/json.hpp"
#include "canalis/core/envelope.hpp"
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

/** manifest から使う Source (config を JSON のまま受け取る)。runner はこれを adapter 名で引く。 */
using JsonSource = Source<Json>;

/** ②Clean/Categorize — RawRecord を sink ネイティブのエンベロープへ整形する。1 入力から複数 sink 向けを出してよい。 */
class Transform {
public:
    virtual ~Transform() = default;
    virtual std::string_view name() const = 0;
    virtual std::vector<SinkBatch> run(const std::vector<RawRecord>& records) = 0;
};

/**
 * ③Save/Import — 渡されたものを保存先へ verbatim で書く。
 * accepts() が "raw" の sink は write_raw を、それ以外 ("rdb" | "kg" | "ft") は write_batch を受ける。
 * 受けない側を呼ぶと std::logic_error。
 */
class Sink {
public:
    virtual ~Sink() = default;
    virtual std::string_view name() const = 0;
    virtual std::string_view accepts() const = 0;
    virtual void write_raw(const std::vector<RawRecord>& records);
    virtual void write_batch(const SinkBatch& batch);
};

} // namespace canalis::core
