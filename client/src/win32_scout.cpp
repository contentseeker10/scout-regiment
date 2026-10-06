#include "scout.hpp"

#include <Windows.h>

namespace {

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

}