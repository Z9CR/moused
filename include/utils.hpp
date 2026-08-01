#pragma once

/// Create all directories in `path`. Throws std::filesystem_error on failure.
void mkdirs(const char* path);

/// Write a printf-style log message to the platform log file:
///   Windows: %TEMP%/moused.log
///   Linux/BSD/macOS: /tmp/moused.log
void log_msg(const char* fmt, ...);
