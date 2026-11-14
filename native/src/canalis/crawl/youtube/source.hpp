#pragma once
// ①Crawl adapter: YouTube Data API v3 でコメントスレッドを取得し RawRecord へ変換する
// (src/crawl/youtube/{types,source}.ts の移植)。API キーは URL のクエリに入るので、エラー文に URL を出さない。
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "canalis/core/pipeline.hpp"
#include "canalis/crawl/fetch.hpp"

namespace canalis::crawl::youtube {

struct YouTubeSourceConfig {
    std::string video_id;
    std::string api_key;
    /** コメントの最大取得数 (既定 100)。 */
    std::optional<int> max_results;
    /** "plainText" | "html" (既定 "plainText")。 */
    std::optional<std::string> text_format;
};

struct YouTubeSourceDeps {
    FetchFn fetch;
    std::function<std::string()> now;
};

class YouTubeSource final : public core::Source<YouTubeSourceConfig> {
public:
    explicit YouTubeSource(YouTubeSourceDeps deps = {});
    std::string_view name() const override { return "youtube"; }
    std::vector<core::RawRecord> crawl(const YouTubeSourceConfig& config) override;

private:
    YouTubeSourceDeps deps_;
};

} // namespace canalis::crawl::youtube
