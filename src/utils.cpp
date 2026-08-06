#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <utils.hpp>

void mkdirs(const std::string& path) {
    std::error_code ec{};
    if (!std::filesystem::create_directories(path, ec) && ec)
        throw std::filesystem::filesystem_error(
            "failed to create directories", std::filesystem::path(path), ec);
}

void log_msg(const char* fmt, ...) {
    static const std::string log_path = [] {
#ifdef _WIN32
        const char* tmp = std::getenv("TEMP");
        if (!tmp) tmp = std::getenv("TMP");
        return std::string(tmp ? tmp : ".") + "\\moused.log";
#else
        return std::string("/tmp/moused.log");
#endif
    }();

    FILE* f = std::fopen(log_path.c_str(), "a");
    if (!f) return;

    va_list args;
    va_start(args, fmt);
    std::vfprintf(f, fmt, args);
    va_end(args);
    std::fclose(f);
}