#pragma once

#include <string>
#include <cstdint>
#include <optional>

using std::string;

namespace scoutreg {

    struct SystemInfo {
        string hostname;
        string username;
        string cpu_arch;
        uint32_t cpu_cores{};
        uint64_t total_ram_mb{};
        uint64_t available_ram_mb{};
    };

    std::optional<SystemInfo> collect_system_info();

    void get_hostname(SystemInfo&);

}

namespace {
    std::string wide_to_utf8(std::wstring_view);
}