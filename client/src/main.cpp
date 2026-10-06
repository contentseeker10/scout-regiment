#include <iostream>

#include "scout.hpp"

void log_msg(const auto& msg) {
    std::cout << "[Scout Regiment] " << msg << std::endl;
}

static const auto info_opt = scoutreg::collect_system_info();

int main() {
    log_msg("System initialized");

    log_msg("Collecting system information");

    if (!info_opt) {
        log_msg("Failed to collect system inforamtion");
        return 1;
    }
    else log_msg("Successfully collected system information\n");

    auto info = info_opt.value();

    log_msg("Hostname: ");
    log_msg(info.hostname);

    log_msg("Username: ");
    log_msg(info.username);

    log_msg("CPU Architecture: ");
    log_msg(info.cpu_arch);

    log_msg("CPU Cores: ");
    log_msg(info.cpu_cores);

    log_msg("RAM Total: ");
    log_msg(info.total_ram);

    log_msg("RAM Available: ");
    log_msg(info.available_ram);

    return 0;
}