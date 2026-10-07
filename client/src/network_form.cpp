#include "network.hpp"

#include <curl/curl.h>

namespace scoutreg::network {

    bool post(
        const std::string& url,
        const scoutreg::SystemInfo& info,
        const std::vector<std::byte>& file,
        long* http_status
    ) {
        CURL* curl = curl_easy_init();

        if (!curl) {
            return false;
        }

        curl_mime* mime = curl_mime_init(curl);

        if (!mime) {
            curl_easy_cleanup(curl);
            return false;
        }

        // Keep converted numeric values alive until curl_easy_perform().
        const std::string cpu_cores = std::to_string(info.cpu_cores);
        const std::string total_ram = std::to_string(info.total_ram);
        const std::string available_ram = std::to_string(info.available_ram);

        // hostname
        curl_mimepart* part = curl_mime_addpart(mime);
        curl_mime_name(part, "hostname");
        curl_mime_data(
            part,
            info.hostname.c_str(),
                       CURL_ZERO_TERMINATED
        );

        // username
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "username");
        curl_mime_data(
            part,
            info.username.c_str(),
                       CURL_ZERO_TERMINATED
        );

        // cpu_arch
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "cpu_arch");
        curl_mime_data(
            part,
            info.cpu_arch.c_str(),
                       CURL_ZERO_TERMINATED
        );

        // cpu_cores
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "cpu_cores");
        curl_mime_data(
            part,
            cpu_cores.c_str(),
                       CURL_ZERO_TERMINATED
        );

        // total_ram
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "total_ram");
        curl_mime_data(
            part,
            total_ram.c_str(),
                       CURL_ZERO_TERMINATED
        );

        // available_ram
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "available_ram");
        curl_mime_data(
            part,
            available_ram.c_str(),
                       CURL_ZERO_TERMINATED
        );

        // Optional ZIP file.
        if (!file.empty()) {
            part = curl_mime_addpart(mime);
            curl_mime_name(part, "file");
            curl_mime_filename(part, "scoutreg.zip");
            curl_mime_type(part, "application/zip");

            curl_mime_data(
                part,
                reinterpret_cast<const char*>(file.data()),
                           file.size()
            );
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

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

            curl_easy_getinfo(
                curl,
                CURLINFO_RESPONSE_CODE,
                &status
            );

            if (http_status) {
                *http_status = status;
            }

            success = status >= 200 && status < 300;
        }

        curl_mime_free(mime);
        curl_easy_cleanup(curl);

        return success;
    }

}
