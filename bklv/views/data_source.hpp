#pragma once

// 「名字 -> 数据源工厂」注册表（**纯数据层**）。
//
// 用途：JSON 描述列表/网格时只写数据源的**名字**（`"data": "@data/pokedex"`），
// 真正的数据在 C++ 里由应用注册。这样"结构交给 JSON、数据与行为留给 C++"才能成立。
//
// 分层红线（docs/json-syntax.md 有分工表）：这一层**不认识 JSON、ImGui、widgets**，
// 它只认识 `RecyclerDataSource` 这个纯接口。

#include <functional>
#include <string>
#include <vector>

#include "bklv/views/recycler.hpp"

namespace bklv
{
namespace data
{

using SourceFactory = std::function<RecyclerDataSource*()>;

// 注册一个数据源工厂。重名、空名字、空工厂都返回 false 并写 error（不覆盖）。
bool registerSource(const std::string& name, SourceFactory factory, std::string& error);

// 按名字建一个数据源实例（**调用方拥有**返回值）；名字没注册返回 nullptr。
RecyclerDataSource* createSource(const std::string& name);

bool hasSource(const std::string& name);

// 已注册的名字（按注册顺序；设计器/文档/测试用）
std::vector<std::string> sourceNames();

// 测试用：清空注册表
void clearSourcesForTest();

} // namespace data
} // namespace bklv
