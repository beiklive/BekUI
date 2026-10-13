#pragma once

// 每个页面一个构建函数。页面只负责"把自己的内容搭出来"，
// 怎么被放进内容区、什么时候显示由 page_host 决定。

#include "bklv/bklv.hpp"

namespace bklv
{
class App;
}

namespace demo
{

// 页 1：基础控件（Label、自定义开关、进度、可聚焦列表项）
bklv::Box* buildWidgetsPage();

// 页 2：文本（换行、单行截断、对齐）
bklv::Box* buildTextPage();

// 页 3：布局（justifyContent / alignItems / grow）
bklv::Box* buildLayoutPage();

// 页 4：虚拟列表
struct ListPage
{
    bklv::Box* view          = nullptr;
    bklv::RecyclerFrame* list = nullptr;
    bool selfScrolling    = false; // 这一页内部已有 ScrollingFrame
};

ListPage buildListPage();

// 页 5：整块界面 + 列表项模板都来自 JSON（属性表与数据绑定）
bklv::Box* buildJsonPage();

// 页 6：主题与外观驱动的控件（换肤、外观 JSON 描述的绘制结构、状态动效）
bklv::Box* buildThemePage();

// 页 7：导航（库级 TabFrame + Sidebar）
bklv::Box* buildNavPage();

// 页 8：TabFrame 导航 + 事件总线实时显示
bklv::Box* buildAppletPage();

// 主题页要能切主题，所以需要 App；main.cpp 在建 PageHost 之前调用。
void setThemeSwitchApp(bklv::App* app);

} // namespace demo
