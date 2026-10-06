#include "scout.hpp"

#include <unistd.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <thread>
#include <array>

namespace scoutreg {

    std::optional<SystemInfo> collect_system_info() {
        SystemInfo info;

        load_hostname(info);
        load_username(info);
        load_cpu(info);
        load_ram(info);
        
        return info;
    }

    void load_hostname(SystemInfo& info) {
        std::array<char, 256> host_buffer{};
        if (gethostname(host_buffer.data(), host_buffer.size()) == 0) {
            info.hostname = host_buffer.data();
        }
    }

    void load_username(SystemInfo& info) {
        if (const auto* pw = getpwuid(geteuid()); pw != nullptr && pw->pw_name != nullptr) {
            info.username = pw->pw_name;
        } else if (const char* user_env = getenv("USER"); user_env != nullptr) {
            info.username = user_env;
        }
    }

    void load_cpu(SystemInfo& info) {
        info.cpu_cores = std::thread::hardware_concurrency();
        if (info.cpu_cores == 0) {
            info.cpu_cores = static_cast<uint32_t>(sysconf(_SC_NPROCESSORS_ONLN));
        }
        struct utsname uname_data{};
        if (uname(&uname_data) == 0) {
            info.cpu_arch = uname_data.machine;
        }
    }

    void load_ram(SystemInfo& info) {
        struct sysinfo mem_info{};
        if (sysinfo(&mem_info) == 0) {
            const uint64_t unit = mem_info.mem_unit > 0 ? mem_info.mem_unit : 1;
            info.total_ram = (mem_info.totalram * unit) / (1024 * 1024);
            info.available_ram = (mem_info.freeram * unit) / (1024 * 1024);
        }
    }

}