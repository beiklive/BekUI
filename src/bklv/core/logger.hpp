#pragma once

// 极简日志，对应 borealis 的 core/logger.hpp。只保留学习项目需要的三个级别。

#include <string>

namespace bklv
{

class Logger
{
  public:
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);
};

} // namespace bklv
