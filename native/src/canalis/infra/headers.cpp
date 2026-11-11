#include "canalis/infra/headers.hpp"

#include "canalis/base/strings.hpp"

namespace canalis::http {

std::optional<std::string> Headers::get(std::string_view name) const {
    for (const auto& [k, v] : items_)
        if (str::iequals_ascii(k, name)) return v;
    return std::nullopt;
}

bool Headers::has(std::string_view name) const { return get(name).has_value(); }

void Headers::set(std::string name, std::string value) {
    remove(name);
    items_.emplace_back(std::move(name), std::move(value));
}

void Headers::add(std::string name, std::string value) { items_.emplace_back(std::move(name), std::move(value)); }

void Headers::remove(std::string_view name) {
    std::erase_if(items_, [&](const auto& kv) { return str::iequals_ascii(kv.first, name); });
}

} // namespace canalis::http
