#pragma once

// JSON 行为注册表：视图 JSON 里只写"名字"，C++ 里注册真正的行为。
//
//   { "type": "Button", "text": "保存", "onClick": "save" }
//   registerJsonAction("save", [](View& view) { ... });
//
// 与 Recycler 的 data/itemBinder 同一分工：结构归 JSON，行为归 C++。

#include <functional>
#include <string>

namespace bklv
{

class View;

using JsonAction = std::function<void(View&)>;

// 重名返回 false 并写 error（不覆盖）。
bool registerJsonAction(const std::string& name, JsonAction action, std::string& error);
void registerJsonAction(const std::string& name, JsonAction action);
bool hasJsonAction(const std::string& name);
// 执行；名字不存在返回 false 并写 error。
bool invokeJsonAction(const std::string& name, View& view, std::string& error);
void clearJsonActionsForTest();

} // namespace bklv
