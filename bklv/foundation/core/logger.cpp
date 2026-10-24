#include "bklv/foundation/core/logger.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace bklv
{

namespace
{

std::mutex g_mutex;
std::deque<LogEntry> g_entries;
size_t g_maxEntries  = 1000;
LogLevel g_level     = LogLevel::INFO;
bool g_consoleOutput = true;
std::FILE* g_file    = nullptr;
uint64_t g_version   = 0;

const char* levelName(LogLevel level)
{
    switch (level)
    {
        case LogLevel::DEBUG: return "debug";
        case LogLevel::INFO: return "info";
        case LogLevel::WARNING: return "warn";
        case LogLevel::ERROR: return "error";
    }
    return "info";
}

std::string makeTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm {};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03d", tm.tm_hour, tm.tm_min, tm.tm_sec,
        static_cast<int>(ms));
    return std::string(buffer);
}

} // namespace

std::string Logger::shortLocation(const char* file)
{
    if (file == nullptr)
        return std::string();

    const std::string path(file);
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

void Logger::setLogLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_level = level;
}

LogLevel Logger::getLogLevel()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_level;
}

void Logger::setMaxEntries(size_t count)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_maxEntries = count == 0 ? 1 : count;

    while (g_entries.size() > g_maxEntries)
        g_entries.pop_front();
}

void Logger::setConsoleOutput(bool enabled)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_consoleOutput = enabled;
}

bool Logger::setFileOutput(const std::string& path)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_file != nullptr)
    {
        std::fclose(g_file);
        g_file = nullptr;
    }

    if (path.empty())
        return true;

    // "w"：每次启动重开，文件里就是本次运行的完整日志。
    g_file = std::fopen(path.c_str(), "w");
    return g_file != nullptr;
}

void Logger::log(LogLevel level, const std::string& location, const std::string& message)
{
    write(level, location, message);
}

void Logger::write(LogLevel level, const std::string& location, const std::string& message)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    if (level < g_level)
        return;

    LogEntry entry;
    entry.time     = makeTimestamp();
    entry.level    = level;
    entry.location = location;
    entry.message  = message;

    if (g_consoleOutput)
    {
        std::FILE* out = level == LogLevel::ERROR ? stderr : stdout;
        std::fprintf(out, "[bklv][%s] %s\n", levelName(level), message.c_str());
        // 立刻刷出去：程序被强杀或崩溃时，缓冲里的日志会丢。
        std::fflush(out);
    }

    if (g_file != nullptr)
    {
        // 带时间戳与位置，和界面上的日志窗口格式一致。
        std::fprintf(g_file, "%s [%s] %s%s%s\n", entry.time.c_str(), levelName(level),
            entry.location.empty() ? "" : entry.location.c_str(),
            entry.location.empty() ? "" : " ", entry.message.c_str());
        // 同上：崩溃时最后一行必须已经在盘上。
        std::fflush(g_file);
    }

    g_entries.push_back(std::move(entry));
    while (g_entries.size() > g_maxEntries)
        g_entries.pop_front();

    g_version++;
}

void Logger::debug(const std::string& message)
{
    write(LogLevel::DEBUG, std::string(), message);
}

void Logger::info(const std::string& message)
{
    write(LogLevel::INFO, std::string(), message);
}

void Logger::warning(const std::string& message)
{
    write(LogLevel::WARNING, std::string(), message);
}

void Logger::error(const std::string& message)
{
    write(LogLevel::ERROR, std::string(), message);
}

const std::deque<LogEntry>& Logger::getLogs()
{
    return g_entries;
}

uint64_t Logger::getVersion()
{
    return g_version;
}

void Logger::clear()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_entries.clear();
    g_version++;
}

} // namespace bklv
