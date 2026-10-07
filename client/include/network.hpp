#pragma once

#include "scout.hpp"
#include <vector>
#include <string>

namespace scoutreg::network {

    bool post(
        const std::string& url,
        const scoutreg::SystemInfo& info,
        const std::vector<std::byte>& file,
        long* http_status
    );

}
