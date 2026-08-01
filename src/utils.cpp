#include <utils.hpp>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

void mkdirs(const char *path)
{
    std::error_code ec{};
    if (!std::filesystem::create_directories(path, ec) && ec)
        throw std::filesystem::filesystem_error("failed to create directories", std::filesystem::path(path), ec);
}

void log_msg(const char* fmt, ...)
{
    static const char* log_path = nullptr;
    if (!log_path)
    {
#ifdef _WIN32
        static char path[1024];
        const char* tmp = std::getenv("TEMP");
        if (!tmp) tmp = std::getenv("TMP");
        std::snprintf(path, sizeof(path), "%s\\moused.log", tmp ? tmp : ".");
        log_path = path;
#else
        log_path = "/tmp/moused.log";
#endif
    }

    FILE* f = std::fopen(log_path, "a");
    if (!f) return;

    va_list args;
    va_start(args, fmt);
    std::vfprintf(f, fmt, args);
    va_end(args);
    std::fclose(f);
}
