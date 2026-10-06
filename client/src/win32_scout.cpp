#include "scout.hpp"

#include <Windows.h>

namespace scoutreg {

    std::optional<SystemInfo> collect_system_info() {
        SystemInfo info;

        get_hostname(info);
        
        return info;
    }
    
    // DNS Hostname
    void get_hostname(SystemInfo& info) {
        wchar_t host_buffer[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD host_len = MAX_COMPUTERNAME_LENGTH + 1;
        if (GetComputerNameExW(ComputerNameDnsHostname, host_buffer, &host_len)) {
            info.hostname = wide_to_utf8({ host_buffer, host_len });
        }
    }

}

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