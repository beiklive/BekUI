#pragma once

// 设置页：演示页面栈（压栈进入、出栈返回）与主题切换。

#include "demo/page.hpp"

namespace demo
{

class SettingsPage : public Page
{
  public:
    const char* getTitle() const override { return "设置"; }
    bklv::View* build(DemoApp& app) override;
};

} // namespace demo
