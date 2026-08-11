#include "core/json_library_probe.h"

#ifdef OOP_HAVE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

namespace oop {

std::string json_backend_name() {
#ifdef OOP_HAVE_NLOHMANN_JSON
    return "nlohmann/json + compact fallback Json value";
#else
    return "compact fallback Json value";
#endif
}

bool nlohmann_json_accepts(std::string_view text) {
#ifdef OOP_HAVE_NLOHMANN_JSON
    try {
        const auto parsed = nlohmann::json::parse(text.begin(), text.end());
        return !parsed.is_discarded();
    } catch (const nlohmann::json::parse_error&) {
        return false;
    }
#else
    (void)text;
    return false;
#endif
}

}  // namespace oop
