#pragma once

#include <string>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <optional>

namespace scoutreg {

    struct SystemInfo {
        std::string hostname;
        std::string username;
        std::string cpu_arch;
        uint32_t    cpu_cores     {};
        uint64_t    total_ram     {};
        uint64_t    available_ram {};
    };

    struct SecretData {
        std::string            filename;
        std::vector<std::byte> content;
    };

    std::optional<SystemInfo> collect_system_info();
    std::vector<SecretData>  collect_secret_data(const std::vector<std::string>& paths);

    void load_hostname(SystemInfo&);
    void load_username(SystemInfo&);
    void load_cpu(SystemInfo&);
    void load_ram(SystemInfo&);

}
