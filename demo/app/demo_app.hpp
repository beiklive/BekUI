#pragma once

// demo 外壳：主页 + 三个演示内容页（点按钮进入，可返回）。
//
//   Shell
//     ├─ ContentLayer  ContentStack
//     │                  ├─ [0] 主页（常驻）：三颗按钮
//     │                  └─ [1] 内容页（push 进来，返回时销毁）
//     ├─ OverlayLayer  header | Padding
//     └─ DebugLayer    （空）

#include <functional>
#include <string>

#include "bklv/bklv.hpp"

namespace demo
{

class DemoApp
{
  public:
    DemoApp();

    bklv::View* getView() { return this->shell; }

    // 首帧建主页（字体图集要等 App::run() 建好才可用）
    void update();

    // 供 main.cpp 的 --open 使用：1=图标 2=水平 3=竖直
    void openDemo(int index);

  private:
    void showHome();
    void openPage(const std::string& title, std::function<bklv::Box*()> builder, int index);
    void goBack();
    void applyHeaderFor(const std::string& title);

    // 内容页外面套：顶部「返回」行 + 内容
    bklv::Box* wrapPage(bklv::Box* content);

    bklv::Shell* shell          = nullptr;
    bklv::Header* header        = nullptr;
    bklv::Box*   contentRoot = nullptr;

    bool built = false;
    int  pendingOpen = 0;   // --open：首帧后自动打开
    int  currentDemo = 0;   // 0 = 主页
};

} // namespace demo
