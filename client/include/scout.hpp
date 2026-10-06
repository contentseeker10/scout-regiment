#pragma once

#include <string>
#include <cstdint>
#include <optional>

namespace scoutreg {

    struct SystemInfo {
        std::string hostname;
        std::string username;
        std::string cpu_arch;
        uint32_t cpu_cores{};
        uint64_t total_ram{};
        uint64_t available_ram{};
    };

    void load_hostname(SystemInfo&);
    void load_username(SystemInfo&);
    void load_cpu(SystemInfo&);
    void load_ram(SystemInfo&);
    
    std::optional<SystemInfo> collect_system_info();

}