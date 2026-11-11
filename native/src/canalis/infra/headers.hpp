#pragma once
// HTTP ヘッダの集まり (名前は大文字小文字を区別しない)。Boost には依存しない。
// 出自: Discutere native src/infra/http.hpp の Headers (e367eac 系)。サーバ側の Request / Router は Canalis に要らないので写していない。
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace canalis::http {

class Headers {
public:
    /** 大文字小文字を無視して最初の値。 */
    std::optional<std::string> get(std::string_view name) const;
    bool has(std::string_view name) const;
    /** 同名を消してから入れる。 */
    void set(std::string name, std::string value);
    void add(std::string name, std::string value);
    void remove(std::string_view name);
    const std::vector<std::pair<std::string, std::string>>& items() const { return items_; }

private:
    std::vector<std::pair<std::string, std::string>> items_;
};

} // namespace canalis::http
