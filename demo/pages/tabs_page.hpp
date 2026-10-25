#pragma once

// 标签页演示：一个 TabBar（顶部水平 / 左侧竖直两套排列可切换）+ 一个 TabView。
//
// 这个页面演示的是"**同级切换、页面常驻**"：
//   切走再切回来，列表的滚动位置和焦点都还在（TabView 负责记忆）。
// 和"层级下钻"（ContentStack，出场就释放）是两件事。

#include "demo/page.hpp"

namespace demo
{

class TabsPage : public Page
{
  public:
    const char* getTitle() const override { return this->vertical ? "标签页（左侧）" : "标签页（顶部）"; }

    // 开局就用左侧竖直排布（给 --page tabs-left 用）
    void setVertical(bool value) { this->vertical = value; }
    bklv::View* build(DemoApp& app) override;

  private:
    bool vertical = false;
};

} // namespace demo
