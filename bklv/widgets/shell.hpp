#pragma once

#include "bklv/core/box.hpp"
#include "bklv/core/geometry.hpp"
#include "bklv/core/input.hpp"

#include <functional>
#include <vector>

namespace bklv
{

// 角落（九个方位）。角落定位用绝对定位实现：真角落直接给两条边偏移；
// 居中类（*_CENTER / CENTER_* / CENTER）套一层铺满的包装盒，用 justifyContent/alignItems 居中。
enum class Corner
{
    TOP_LEFT,
    TOP_CENTER,
    TOP_RIGHT,
    CENTER_LEFT,
    CENTER,
    CENTER_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_CENTER,
    BOTTOM_RIGHT,
};

// 贴哪条边
enum class Edge
{
    TOP,
    RIGHT,
    BOTTOM,
    LEFT,
};

// 弹窗选项。放在类外：C++ 不允许把"带默认成员初值的嵌套类型"用作同类内的默认实参。
struct DialogOptions
{
    bool dim             = true;   // 弹窗后加遮罩
    bool closeOnDimClick = true;   // 点弹窗之外的区域关闭（弹窗内的空白不关）
    bool closeOnBack     = true;   // 按 B 关闭（弹窗内没人处理 B 时生效）
    // 遮罩底色。默认半透明黑；上层应传主题色 `@theme/color.common.overlay` 以跟随换肤。
    unsigned int dimColor = 0x8C000000u; // IM_COL32(0,0,0,140)
};

// 应用外壳：四层结构（取代旧 AppletFrame 的三段式）。
//
//   BackdropLayer  背景层   绝对定位；图片背景（视频后续）
//   ContentLayer   内容层   正式布局在这层（页面栈）
//   OverlayLayer   覆盖层   页头 / 底栏 / 弹窗
//   HudLayer       浮层     不吃输入；toast、常驻指示
//
// 契约：
//  - 只有内容层与覆盖层参与重排；背景层与浮层绝对定位，跟随视口尺寸
//  - 页头 / 底栏是**纯显示控件，永不参与焦点**
//  - 内容避让**不自动**：页面自己调 getHeaderInset()/getFooterInset()
//  - 页头显隐动画期间 insets 返回**目标值**，避免逐帧重排
class Shell : public Box
{
  public:
    Shell();

    Box* getBackdropLayer() const { return this->backdropLayer; }
    Box* getContentLayer() const { return this->contentLayer; }
    Box* getOverlayLayer() const { return this->overlayLayer; }
    Box* getHudLayer() const { return this->hudLayer; }

    // 页头 / 底栏视图（由上层建好交给外壳）；显隐时走 GONE（不占位）
    void setHeaderView(View* view);
    void setFooterView(View* view);
    View* getHeaderView() const { return this->headerView; }
    View* getFooterView() const { return this->footerView; }

    void setHeaderVisible(bool visible);
    void setFooterVisible(bool visible);
    bool isHeaderVisible() const { return this->headerVisible; }
    bool isFooterVisible() const { return this->footerVisible; }

    // 页面显式查询留白（动画期间返回目标值）
    void setHeaderInset(float inset);
    void setFooterInset(float inset);
    float getHeaderInset() const { return this->headerInset; }
    float getFooterInset() const { return this->footerInset; }

    // 安全区：四层统一内缩（Switch 的圆角/刘海）
    void setSafeAreaInsets(float top, float right, float bottom, float left);
    void setSafeAreaInsets(const EdgeInsets& insets)
    {
        this->setSafeAreaInsets(insets.top, insets.right, insets.bottom, insets.left);
    }
    float getSafeAreaTop() const { return this->safeTop; }
    float getSafeAreaRight() const { return this->safeRight; }
    float getSafeAreaBottom() const { return this->safeBottom; }
    float getSafeAreaLeft() const { return this->safeLeft; }

    // 弹窗：挂到**覆盖层顶部**，并打**模态标记**（复用 InputManager 的模态栈，
    // 它负责"指针命中与方向导航限制在模态子树内 + 关闭时恢复焦点"）。
    //
    // 遮罩（dim）与"点弹窗之外关闭"（closeOnDimClick）、"B 关闭"（closeOnBack）
    // 都在这里接好：B 的读取点是模态登记的 onUnhandled 兜底（见 input.hpp pushModal）。
    void pushDialog(View* dialog, const DialogOptions& options = DialogOptions());
    // 弹出并**默认释放**弹窗（与 `App::popOverlay(overlay, free = true)` 同一约定：
    // 挂上去之后所有权就归外壳）。还想自己留着这个视图就传 free=false。
    // 遮罩是外壳自己 new 的，永远归外壳释放。
    void popDialog(View* dialog, bool free = true);
    // 遮罩层（dim=true 时生成；插在弹窗**之前**入树 = 绘制在弹窗之下）。栈里与弹窗一一对应。
    View* getDimLayer() const { return this->dimStack.empty() ? nullptr : this->dimStack.back(); }
    bool hasDialog() const { return !this->dialogStack.empty(); }
    View* getTopDialog() const { return this->dialogStack.empty() ? nullptr : this->dialogStack.back(); }

    // B 路由的第 3 级（非模态）：内容树里没人处理 B 时冒泡到外壳，由它决定。
    // 返回 true 表示已处理（例如返回上一页 / 退出）。
    void setOnBackRequested(std::function<bool()> handler);
    bool hasBackRequestedHandler() const { return this->onBackRequested != nullptr; }

    // 角落 / 边缘定位（内部即绝对定位 + 边偏移；居中类会用一层铺满的包装盒）
    void addToCorner(View* view, Corner corner, float insetX, float insetY);
    void addToEdge(View* view, Edge edge, float inset);
    void fillLayer(View* view); // 四边 0，铺满该层（全屏遮罩/loading）

  protected:
    void applySafeAreaToLayers();

  private:
    // 模态子树里没人消费按键时的兜底：目前只处理 closeOnBack（按 B 关掉最上层弹窗）。
    bool handleModalUnhandled(ControllerButton button);

    Box* backdropLayer = nullptr;
    Box* contentLayer  = nullptr;
    Box* overlayLayer  = nullptr;
    Box* hudLayer      = nullptr;

    std::vector<View*> dialogStack;
    std::vector<View*> dimStack;
    // 与 dialogStack 一一对应的选项（closeOnBack 的读取点在这里）
    std::vector<DialogOptions> dialogOptionStack;

    std::function<bool()> onBackRequested;
    bool backActionRegistered = false;

    View* headerView = nullptr;
    View* footerView = nullptr;
    bool headerVisible = true;
    bool footerVisible = true;

    float headerInset = 0.0f;
    float footerInset = 0.0f;

    float safeTop    = 0.0f;
    float safeRight  = 0.0f;
    float safeBottom = 0.0f;
    float safeLeft   = 0.0f;
};

} // namespace bklv
