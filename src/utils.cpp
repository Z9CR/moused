#include <utils.hpp>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

bool mkdirs(const char *path)
{
    std::error_code ec{};
    bool status = std::filesystem::create_directories(path, ec);
    if (!status && ec)
    {
        if (ec == std::errc::file_exists)
            fprintf(stderr, "there is a file has same name as `%s`", path);
        else if (ec == std::errc::not_a_directory)
            fprintf(stderr, "there is a file in the parent paths of `%s`", path);
        else if (ec == std::errc::permission_denied)
            fprintf(stderr, "permission denied when opening `%s`", path);
        else if (ec == std::errc::read_only_file_system)
            fprintf(stderr, "filesystem which `%s` at is Read Only", path);
        else
            fprintf(stderr, "unknown error (%d) on `%s`", ec.value(), path);
        return false;
    }
    return true;
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
