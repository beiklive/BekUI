#pragma once

// AppletFrame：应用的宿主框架（对应 borealis 的 views/applet_frame）。
//
//   ┌──────────────────────────────┐
//   │ Header（标题/副标题，可隐藏）  │
//   ├──────────────────────────────┤
//   │ 内容区（当前内容，撑满剩余空间）│  ← **内容栈**：push/pop，B 键返回上一层
//   ├──────────────────────────────┤
//   │ BottomBar（提示行，可隐藏）    │
//   └──────────────────────────────┘
//
// 和 TabFrame 的区别：TabFrame 是平级切换，AppletFrame 是**层级下钻**
// （设置 → 子页 → 子子页，B 逐层返回）。页头样式有 Regular / Popup 两种，
// Popup 走 `widgets/header.popup.json` 这份外观（variant 机制，不用改代码）。

#include <functional>
#include <string>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/core/time.hpp"
#include "bklv/widgets/bottom_bar.hpp"
#include "bklv/widgets/header.hpp"

namespace bklv
{

enum class AppletHeaderStyle
{
    REGULAR,
    POPUP,
};

class AppletFrame : public Box
{
  public:
    explicit AppletFrame(View* contentView = nullptr);
    ~AppletFrame() override;

    // 下钻 / 返回。内容只脱离不销毁，返回时状态还在。
    void pushContentView(View* view);
    void popContentView(std::function<void()> callback = nullptr);

    View* getContentView() const { return this->contentView; }
    size_t getContentStackSize() const { return this->contentStack.size(); }

    void setTitle(const std::string& title);
    void setSubtitle(const std::string& subtitle);

    // ---- 全局显隐（页头 / 底栏是外壳级的，所有页面共用同一份）----
    //
    // 隐藏时用 GONE（不占位），内容区会自动吃掉那部分高度；
    // 外壳里没有"残留间距"——页头下边的留白写在页头自己的 padding 里。
    void setHeaderVisible(bool visible);
    void setBottomBarVisible(bool visible);
    bool isHeaderVisible() const { return this->headerVisible; }
    bool isBottomBarVisible() const { return this->bottomBarVisible; }

    // 显隐时是否走补间（默认开）；关掉就是立刻切换
    void setBarsAnimated(bool animated) { this->barsAnimated = animated; }
    bool isBarsAnimated() const { return this->barsAnimated; }
    // 补间时长（秒）
    void setBarAnimationDuration(float seconds) { this->barDuration = seconds; }

    // 兼容旧的语义：VISIBLE / GONE 两态
    void setHeaderVisibility(Visibility visibility);
    void setFooterVisibility(Visibility visibility);
    void setHeaderStyle(AppletHeaderStyle style);

    // 已经在最外层还按 B 时调用（比如关掉这个框架所在的弹层）
    void setOnDismiss(std::function<void()> handler) { this->onDismiss = std::move(handler); }

    Header* getHeader() const { return this->header; }
    BottomBar* getFooter() const { return this->footer; }
    Box* getContentBox() const { return this->contentBox; }

    void update() override;

    // 外壳铺页面底色（color.surface.page，对应 borealis 的 brls/background）
    void applyTheme() override;

    std::string getClassString() const override { return "bklv::AppletFrame"; }

  private:
    void attachContentView(View* view);
    void refreshFooter();

    // 折叠动画：从整高补到 0（隐藏）/ 从 0 补到整高（显示）
    void startBarAnimation();
    void updateBars();
    void finishBarAnimation();

    Header* header       = nullptr;
    BottomBar* footer    = nullptr;

    bool headerVisible    = true;
    bool bottomBarVisible = true;
    bool barsAnimated     = true;
    float barDuration     = 0.18f; // 秒
    Animatable headerAnim{ 1.0f };
    Animatable footerAnim{ 1.0f };
    bool animating        = false;
    Box* contentBox      = nullptr;
    View* contentView    = nullptr;
    std::vector<View*> contentStack;
    std::function<void()> onDismiss;
    std::function<void()> popCallback;
};

} // namespace bklv
