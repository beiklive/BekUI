#pragma once

// demo 的页面基类。
//
// 一个页面只做一件事：用库里的基础视图（Box / Label / Button …）拼出一棵子树，
// 交回给 DemoApp。页面不持有窗口、不碰 App 的帧循环，也不负责导航 ——
// 导航是 DemoApp 的事，页面只管"长什么样"。
//
// 页面在栈里每次进入都会**重新 build 一次**（旧子树随出栈销毁），所以页面对象
// 如果需要跨次保留数据，就自己存成员变量，别存在视图里。

#include "bklv/ui/view/view.hpp"

namespace demo
{

class DemoApp;

class Page
{
  public:
    virtual ~Page() = default;

    // 顶栏标题。
    virtual const char* getTitle() const = 0;

    // 建一棵子树。返回的视图由调用方（DemoApp 的内容区）接管所有权。
    // 每次进入页面都会调用一次，返回 null 视为空页面。
    virtual bklv::View* build(DemoApp& app) = 0;
};

} // namespace demo
