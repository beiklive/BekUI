#pragma once

// 每个页面一个构建函数。页面只负责"把自己的内容搭出来"，
// 怎么被放进内容区、什么时候显示由 page_host 决定。

#include "bklv/core/box.hpp"
#include "bklv/views/label.hpp"
#include "bklv/views/recycler.hpp"

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

// 页 5：布局与调试
struct DebugPage
{
    bklv::Box* view           = nullptr;
    bklv::RecyclerFrame* list = nullptr; // 内嵌的短列表
    bklv::Label* stats        = nullptr; // 每帧刷新的统计文字
};

DebugPage buildDebugPage();

} // namespace demo
