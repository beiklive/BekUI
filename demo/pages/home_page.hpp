#pragma once

// 首页：demo 的起点，放几个按钮演示"导航 / 换肤 / 通知"三条基础链路。

#include "demo/page.hpp"

namespace demo
{

class HomePage : public Page
{
  public:
    const char* getTitle() const override { return "首页"; }
    bklv::View* build(DemoApp& app) override;
};

} // namespace demo
