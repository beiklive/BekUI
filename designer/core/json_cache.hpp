#pragma once

// 小 JSON 文件缓存：同一个路径只读盘 + 解析一次。
//
// 用途：视图树展开 `include` 时每帧都会去读被包含的文件 —— 不缓存的话每帧都在
// 读盘 + 解析（展开一个 include 就够把帧率拖下来）。缓存可以在文件被改后 clear()。

#include <map>
#include <string>

#include <nlohmann/json.hpp>

namespace designer
{

class JsonCache
{
  public:
    using Json = nlohmann::ordered_json;

    // 读不到 / 解析失败返回 nullptr（并记住这次失败，不再重复读盘）
    const Json* get(const std::string& path);

    void clear() { this->entries.clear(); }
    size_t size() const { return this->entries.size(); }

  private:
    std::map<std::string, Json> entries; // 存的是"是否解析成功"，失败用 discarded 标记
};

} // namespace designer
