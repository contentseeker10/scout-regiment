#include <iostream>

#include "scout.hpp"

void log_msg(const std::string_view& msg) {
    std::cout << "[Scout Regiment] " << msg << std::endl;
}

int main() {
    log_msg("System initialized");

    log_msg("Collecting system information");
    auto info_opt = scoutreg::collect_system_info();

    if (!info_opt) {
        log_msg("Failed to collect system inforamtion");
        return 1;
    }
    else log_msg("Successfully collected system information\n");

    auto info = info_opt.value();

    log_msg("Hostname: " + info.hostname);

    return 0;
}