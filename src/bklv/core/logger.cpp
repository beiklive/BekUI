#include "bklv/core/logger.hpp"

#include <cstdio>

namespace bklv
{

void Logger::info(const std::string& message)
{
    std::printf("[bklv][info] %s\n", message.c_str());
}

void Logger::warning(const std::string& message)
{
    std::printf("[bklv][warn] %s\n", message.c_str());
}

void Logger::error(const std::string& message)
{
    std::fprintf(stderr, "[bklv][error] %s\n", message.c_str());
}

} // namespace bklv
