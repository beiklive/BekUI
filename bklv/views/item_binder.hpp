#pragma once

// 「名字 -> item 绑定函数」注册表（**纯数据层**）。
//
// 用途：JSON 描述列表的 item 模板时，只写**绑定函数的名字**（`"itemBinder": "pokedexCell"`），
// 真正的"把第 N 项数据套进视图"在 C++ 里由应用注册。这样 item 的组织方式在 JSON、
// 数据在 C++，两边不互相污染。
//
// 分层红线：这一层**不认识 JSON、ImGui、widgets** —— 它只认识 `View` 与前向声明的请求结构。

#include <functional>
#include <string>
#include <vector>

namespace bklv
{

class View;

namespace binding
{

// 绑定函数的形状：把第 index 项的数据套进 item 视图
using ItemBinder = std::function<void(View& item, size_t index)>;

// 重名、空名字、空函数都返回 false 并写 error（不覆盖）
bool registerItemBinder(const std::string& name, ItemBinder binder, std::string& error);

// 按名字取绑定函数；没注册过返回空 function（调用方自己判断）
ItemBinder findItemBinder(const std::string& name);

bool hasItemBinder(const std::string& name);
std::vector<std::string> itemBinderNames();

// 测试用
void clearItemBindersForTest();

} // namespace binding
} // namespace bklv
