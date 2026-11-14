#pragma once
// `canalis crawl <adapter> --config <JSON>` の adapter 名と、JSON の config を各 adapter の C++ の Config に写す対応。
// JSON のキーは TS の各 adapter の Config と同じ (camelCase)。
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "canalis/base/json.hpp"
#include "canalis/core/raw_record.hpp"

namespace canalis::cli {

/** 使える adapter 名 (usage に出す順)。 */
const std::vector<std::string_view>& adapter_names();

/** adapter で取得する。未知の adapter なら nullopt。取得の失敗は例外。 */
std::optional<std::vector<core::RawRecord>> crawl_adapter(std::string_view adapter, const Json& config);

} // namespace canalis::cli
