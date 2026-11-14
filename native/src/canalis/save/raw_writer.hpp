#pragma once
// ③Save の raw sink: ①の出力 RawRecord を 1 行 1 件の JSONL で verbatim 保存する (src/save/raw-writer.ts の移植)。
// 読み戻し (replay の起点) も持つ。段階 3 では CLI の notion-crawl が使う分を先に移す (ほかの sink は段階 5)。
#include <optional>
#include <string>
#include <vector>

#include "canalis/core/raw_record.hpp"

namespace canalis::save {

struct JsonlRawSinkConfig {
    /** 出力ディレクトリ。 */
    std::string dir;
    /** ファイル名に付けるタイムスタンプ。省略時は呼び出し時刻 (ISO8601、":" は "-" に置き換える)。 */
    std::optional<std::string> stamp;
};

class JsonlRawSink {
public:
    explicit JsonlRawSink(JsonlRawSinkConfig config);
    /** <dir>/<stamp>.jsonl に書く (親ディレクトリは作る)。書いたファイルのパスを返す。失敗は std::runtime_error。 */
    std::string write(const std::vector<core::RawRecord>& records) const;

private:
    JsonlRawSinkConfig config_;
};

/** JSONL 1 本を読み戻す。空行は飛ばす。壊れた行・必須の項目が無い行は std::runtime_error。 */
std::vector<core::RawRecord> load_raw_records_from_file(const std::string& file);

/** ディレクトリ内の *.jsonl を名前順 (= 時系列順) に読んで連結する。 */
std::vector<core::RawRecord> load_raw_records_from_dir(const std::string& dir);

} // namespace canalis::save
