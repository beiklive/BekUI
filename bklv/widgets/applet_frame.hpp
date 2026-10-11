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
#include "bklv/widgets/bottom_bar.hpp"
#include "bklv/widgets/hints_bar.hpp"
#include "bklv/widgets/header.hpp"

namespace bklv
{

enum class AppletHeaderStyle
{
    Regular,
    Popup,
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

    void setHeaderVisibility(Visibility visibility);
    void setFooterVisibility(Visibility visibility);
    void setHeaderStyle(AppletHeaderStyle style);

    // 已经在最外层还按 B 时调用（比如关掉这个框架所在的弹层）
    void setOnDismiss(std::function<void()> handler) { this->onDismiss = std::move(handler); }

    Header* getHeader() const { return this->header; }
    HintsBar* getHintsBar() const { return this->hintsBar; }

    // 想要"上边线 + 一行文字"的底栏时用它（提示栏与文字底栏二选一，默认提示栏）
    BottomBar* getFooter() const { return this->footer; }
    void setUseTextFooter(bool value);
    Box* getContentBox() const { return this->contentBox; }

    void update() override;

    std::string getClassString() const override { return "bklv::AppletFrame"; }

  private:
    void attachContentView(View* view);
    void refreshFooter();

    Header* header       = nullptr;
    HintsBar* hintsBar   = nullptr;
    BottomBar* footer    = nullptr;
    bool useTextFooter   = false;
    Box* contentBox      = nullptr;
    View* contentView    = nullptr;
    std::vector<View*> contentStack;
    std::function<void()> onDismiss;
    std::function<void()> popCallback;
};

} // namespace bklv
