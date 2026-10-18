#pragma once

// demo 外壳（清理后只保留图标页）：
//
//   Shell
//     ├─ ContentLayer  [ ScrollingFrame（Pager 的槽） | TabBar ]
//     ├─ OverlayLayer  header | Padding
//     └─ DebugLayer    （空）
//
// 页面是工厂：切页时 Pager 现建、旧页现删。

#include "bklv/bklv.hpp"

namespace demo
{

class DemoApp
{
  public:
    DemoApp();
    ~DemoApp();

    bklv::View* getView() { return this->shell; }

    // 首帧选页：图标字形要等 App::run() 建好字体图集之后才扫得到，
    // 构造期建页会拿到空表（整页空白）。
    void update();

  private:
    bklv::Shell* shell      = nullptr;
    bklv::Header* header    = nullptr;
    bklv::Box* contentRoot  = nullptr;
    bklv::TabFrame* frame   = nullptr;

    bool pageSelected = false;
};

} // namespace demo
