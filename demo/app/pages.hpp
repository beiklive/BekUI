#pragma once

// demo 的页面清单：每个页面一个构建函数，返回页面根视图。
// 页面只负责"把自己的内容搭出来"，怎么显示、什么时候显示由 demo_app 决定。

#include "bklv/bklv.hpp"

namespace bklv
{
class App;
class AppletFrame;
}

namespace demo
{

// 主题页要能切主题，需要 App 实例；main.cpp 在建 DemoApp 之前调用
void setThemeSwitchApp(bklv::App* app);

// 1. 主题与外观：换肤 + 外观驱动的控件 + 状态动效
bklv::Box* buildThemePage();

// 2. 基础控件：Button / Slider / Dropdown / ProgressSpinner / Image
bklv::Box* buildWidgetsPage();

// 3. 文本：截断、换行、对齐、字号
bklv::Box* buildTextPage();

// 4. 布局：Flexbox 能力
bklv::Box* buildLayoutPage();

// 5. 列表：RecyclerFrame（1000 条）
struct ListPageResult
{
    bklv::Box* view          = nullptr;
    bklv::View* list         = nullptr;
};
ListPageResult buildListPage();

// 6. JSON 视图描述：整页 + 列表项模板都来自 JSON
bklv::Box* buildJsonPage();

// 7. 导航：TabFrame
bklv::Box* buildNavPage();

// 8. 弹层与输入：Dialog / EditTextDialog
bklv::Box* buildOverlayPage();

// 12. 图鉴：分段回收列表 + 详情页导航（对标 borealis 的 Pokedex / RecyclingListTab）
bklv::Box* buildPokedexPage();
// 详情页（段号 + 段内下标）；填到内容栈的上一层
bklv::View* buildPokedexDetail(size_t section, size_t index);

// 详情页目标外壳：图鉴把详情推到它的内容栈上（B 返回）
void setDetailShell(bklv::AppletFrame* shell);
bklv::AppletFrame* getDetailShell();

// 11. 滚动：独立滚动容器 + 焦点跟随滚动（对标 borealis 的 Scroll test）
bklv::Box* buildScrollPage();

// 10. 变换：绘制期缩放/旋转/斜切/位移 + 多段缓动补间（对标 borealis 的 Transform test）
bklv::Box* buildTransformPage();

// 9. 设置：列表型 Cell 全家桶（detail / bool / radio / selector / slider / input）
//    页里的开关要能操作外壳的页头/底栏，所以把外壳传进来
void setSettingsShell(bklv::AppletFrame* shell);
bklv::Box* buildSettingsPage();

} // namespace demo
