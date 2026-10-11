#pragma once

// 日志：控制台输出 + 内存环形缓冲，供界面上的日志窗口显示。
//
// 设计参考 borealis 的 core/logger：
//   - 四个级别（Debug / Info / Warning / Error），带级别过滤
//   - 带时间戳（毫秒精度）与 文件:行号
//   - 对外暴露日志序列与变更计数，界面自己决定怎么画
//
// 与 borealis 的差别：**不**在每条日志里创建视图。
// borealis 的 DebugLayer 每条日志 new 一个 Box + 3 个 Label 挂到 View 树上，
// 日志量大时会不断触发 Yoga 重排。这里只往环形缓冲里写一条记录，
// 界面用 ImGui 窗口按需读取（见 views/debug_window.hpp）。

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>

namespace bklv
{

enum class LogLevel : int
{
    DEBUG = 0,
    INFO,
    WARNING,
    ERROR,
};

struct LogEntry
{
    std::string time; // "HH:MM:SS.mmm"
    LogLevel level = LogLevel::INFO;
    std::string location; // "文件名:行号"
    std::string message;
};

class Logger
{
  public:
    // 低于这个级别的日志直接丢弃（也不进缓冲）。默认 INFO。
    static void setLogLevel(LogLevel level);
    static LogLevel getLogLevel();

    // 缓冲最多保留多少条，默认 1000。
    static void setMaxEntries(size_t count);

    // 控制台输出开关，默认开。
    static void setConsoleOutput(bool enabled);

    // 追加写入日志文件。Switch 上没有终端、nxlink 又依赖电脑侧连接，
    // 用它把日志落到 sdmc:/bklv.log，初始化失败或崩溃后拔卡也能看到。
    // path 传空字符串表示关闭。返回是否成功打开。
    static bool setFileOutput(const std::string& path);

    static void log(LogLevel level, const std::string& location, const std::string& message);

    // 便捷接口（无 文件:行号）
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);

    // 日志序列。返回值在下一次写入前有效。
    static const std::deque<LogEntry>& getLogs();

    // 每次写入自增。界面用它判断有没有新日志，避免每帧遍历。
    static uint64_t getVersion();

    static void clear();

    // 只取文件名，去掉目录前缀（__FILE__ 会是完整路径）。
    static std::string shortLocation(const char* file);

  private:
    static void write(LogLevel level, const std::string& location, const std::string& message);
};

// 带 文件:行号 的便捷宏。参数按 printf 风格传给消息：
//   BKLV_LOG_INFO("加载了 %d 个页面", count)
//   BKLV_LOG_INFO("界面已就绪")
#define BKLV_LOG_DEBUG(...)                                                                            \
    ::bklv::Logger::log(::bklv::LogLevel::DEBUG,                                                       \
        ::bklv::Logger::shortLocation(__FILE__) + ":" + std::to_string(__LINE__),                      \
        ::bklv::detail::makeLogMessage(__VA_ARGS__))
#define BKLV_LOG_INFO(...)                                                                             \
    ::bklv::Logger::log(::bklv::LogLevel::INFO,                                                        \
        ::bklv::Logger::shortLocation(__FILE__) + ":" + std::to_string(__LINE__),                      \
        ::bklv::detail::makeLogMessage(__VA_ARGS__))
#define BKLV_LOG_WARNING(...)                                                                          \
    ::bklv::Logger::log(::bklv::LogLevel::WARNING,                                                     \
        ::bklv::Logger::shortLocation(__FILE__) + ":" + std::to_string(__LINE__),                      \
        ::bklv::detail::makeLogMessage(__VA_ARGS__))
#define BKLV_LOG_ERROR(...)                                                                            \
    ::bklv::Logger::log(::bklv::LogLevel::ERROR,                                                       \
        ::bklv::Logger::shortLocation(__FILE__) + ":" + std::to_string(__LINE__),                      \
        ::bklv::detail::makeLogMessage(__VA_ARGS__))

namespace detail
{

// 无参数：原样返回。单独重载是为了不让 snprintf 接到非字面量的格式串
// （否则触发 -Wformat-security）。
inline std::string makeLogMessage(const std::string& message)
{
    return message;
}

inline std::string makeLogMessage(const char* message)
{
    return std::string(message);
}

// 带参数：printf 风格格式化。缓冲区 1024，超出的部分截断。
template <typename T, typename... Rest>
std::string makeLogMessage(const char* format, T value, Rest... rest)
{
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), format, value, rest...);
    return std::string(buffer);
}

} // namespace detail

} // namespace bklv
