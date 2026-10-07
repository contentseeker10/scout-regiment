#include "network.hpp"

#include <curl/curl.h>

namespace {

    std::string json_escape(const std::string& value)
    {
        std::string result;
        result.reserve(value.size() + 2);

        for (const char c : value) {
            switch (c) {
                case '"':
                    result += "\\\"";
                    break;

                case '\\':
                    result += "\\\\";
                    break;

                case '\b':
                    result += "\\b";
                    break;

                case '\f':
                    result += "\\f";
                    break;

                case '\n':
                    result += "\\n";
                    break;

                case '\r':
                    result += "\\r";
                    break;

                case '\t':
                    result += "\\t";
                    break;

                default:
                    result += c;
                    break;
            }
        }

        return result;
    }

    std::string system_info_json(const scoutreg::SystemInfo& info)
    {
        return "{"
        "\"hostname\":\"" + json_escape(info.hostname) + "\","
        "\"username\":\"" + json_escape(info.username) + "\","
        "\"cpu_arch\":\"" + json_escape(info.cpu_arch) + "\","
        "\"cpu_cores\":" + std::to_string(info.cpu_cores) + ","
        "\"total_ram\":" + std::to_string(info.total_ram) + ","
        "\"available_ram\":" + std::to_string(info.available_ram) +
        "}";
    }

} // namespace

namespace scoutreg::network {

    bool post(
        const std::string& url,
        const scoutreg::SystemInfo& info,
        const std::vector<std::byte>& file,
        long* http_status
    ) {
        const std::string json = system_info_json(info);

        CURL* curl = curl_easy_init();

        if (!curl) {
            return false;
        }

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(
            headers,
            "Content-Type: application/json"
        );

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.data());
        curl_easy_setopt(
            curl,
            CURLOPT_POSTFIELDSIZE,
            static_cast<long>(json.size())
        );

        curl_easy_setopt(
            curl,
            CURLOPT_WRITEFUNCTION,
            +[](char*, size_t size, size_t count, void*) -> size_t {
                return size * count;
            }
        );

        const CURLcode result = curl_easy_perform(curl);

        bool success = false;

        if (result == CURLE_OK) {
            long status = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

            if (http_status) {
                *http_status = status;
            }

            success = status >= 200 && status < 300;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        return success;
    }

}
