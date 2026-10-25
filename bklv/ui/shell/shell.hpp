#pragma once

// 应用外壳：给宿主一套**固定顺序的层容器**，让"背景 / 内容 / 覆盖 / 浮层 / 调试"
// 不用宿主自己摆顺序、也不会互相盖错。
//
// 绘制顺序 = 子节点顺序（`Box::drawChildren` 按 addView 顺序画）：
//
//   BackdropLayer  背景层   绝对定位；图片背景、视频、按页底图
//   ContentLayer   内容层   正式内容摆这层（页面、页面栈）
//   OverlayLayer   覆盖层   页头 / 底栏 / 弹窗
//   HudLayer       浮层     不吃输入；toast、常驻指示、焦点框
//   DebugLayer     调试层   **在浮层之上**，永远最后绘制；日志窗口等调试 UI
//
// 契约（改这里之前先读一遍）：
//  - 五个层都是绝对定位 + 四边 0，铺满外壳，**不参与彼此的 flex 排布**；
//  - 只有内容层与覆盖层里的东西按正常布局排，其余层靠绝对定位摆；
//  - 焦点框归外壳所有（挂在浮层上，永远在内容/覆盖层之上），控件侧不用管；
//  - 过渡层（`getTransitionHost()`）**只在动画期间存在**，空了下一帧自动摘掉释放，
//    别把常驻东西挂进去；
//  - 安全区五层统一内缩（Switch 的圆角/刘海），用 `setSafeAreaInsets()` 设一次即可。
//
// 用法：
//   auto* shell = new bklv::Shell();
//   shell->getContentLayer()->addView(page);
//   shell->getOverlayLayer()->addView(header);
//   app.setRootView(shell);

#include "bklv/foundation/geom/geometry.hpp"
#include "bklv/graphics/render/glass/glass_service.hpp"
#include "bklv/ui/shell/content_stack.hpp"
#include "bklv/ui/shell/focus_ring_view.hpp"
#include "bklv/ui/view/box.hpp"

namespace bklv
{

class Shell : public Box
{
  public:
    Shell();
    ~Shell() override;

    // 调试层按这个类型名显示（不重写就只剩基类的 "bklv::View"）。
    std::string getClassString() const override { return "bklv::Shell"; }

    // 五个层（都由外壳持有，宿主只往里 addView，不要自己 delete）
    Box* getBackdropLayer() const { return this->backdropLayer; }
    Box* getContentLayer() const { return this->contentLayer; }
    Box* getOverlayLayer() const { return this->overlayLayer; }

    // 弹层挂在覆盖层，浮层（焦点框）与调试层仍然在它之上。
    Box* getOverlayHost() override { return this->overlayLayer; }

    // 通知这类浮层挂在 HUD 层：在覆盖层（模态）之上、调试层之下。
    Box* getHudHost() override { return this->hudLayer; }

    Box* getHudLayer() const { return this->hudLayer; }
    Box* getDebugLayer() const { return this->debugLayer; }

    // ---- 焦点框（流光框）----
    // 外壳是焦点框的所有者：它挂在浮层上，永远画在最上层。
    // 开关 / 换类型 / 调参数都从这里走，控件侧完全不用管。
    void setFocusRingEnabled(bool enabled);
    bool isFocusRingEnabled() const;

    void setFocusRingType(focus_ring::Type type);
    focus_ring::Type getFocusRingType() const;

    // 细调（线宽/缝/流动速度/透明度/分段）；颜色永远来自 LUT 贴图，这里改不了。
    focus_ring::Style& getFocusRingStyle();
    const focus_ring::Style& getFocusRingStyle() const;

    // 需要自己画法时（`focus_ring::Type::CUSTOM`）直接拿这个视图挂渲染器
    FocusRingView* getFocusRingView() const { return this->focusRingView; }

    // ---- 液态玻璃（外观层）----
    // 外壳是面向使用者的所有者：开关 / 档位 / 默认参数 / 能力查询都从这里走。
    // 样式参数的优先级：**外观层 JSON 写了就用 JSON，没写才用这里的 GlassTuning**；
    // `enabled` 与 `quality` 只由这里决定（开关与性能，不是样式）。
    void setGlassEnabled(bool enabled);
    bool isGlassEnabled() const;

    void setGlassQuality(glass::GlassQuality quality);
    glass::GlassQuality getGlassQuality() const;

    glass::GlassTuning& getGlassTuning();
    const glass::GlassTuning& getGlassTuning() const;

    // 后端能力：拿不到就别把玻璃开关暴露给用户
    glass::EffectCapabilities getGlassCapabilities() const;
    bool isGlassSupported() const;

    // ---- 内容层：层级导航（下钻 / 返回）----
    // 叠一层时**旧层先出场（还挂在树上，所以看得到）→ 播完才摘它、挂新层、给焦点 → 新层入场**；
    // 下钻出来的详情页在出场播完才释放。同级换页用 `replaceContentView()`。
    // 层级与焦点、滚动位置的记忆由 ContentStack 负责（key 取视图的 id）。
    void pushContentView(View* view, const std::string& memoryKey = std::string());
    void replaceContentView(View* view, const std::string& memoryKey = std::string());
    // 弹出栈顶并**释放**它；已经在最外层时什么都不做，只执行回调。
    void popContentView(std::function<void()> onFinished = nullptr);
    View* getContentView() const { return this->contentStack.getTop(); }
    size_t getContentStackSize() const { return this->contentStack.size(); }

    // 入场 / 出场分开配（秒；0 = 用主题的动画时长）
    void setEnterTransition(ContentTransition value) { this->contentStack.setEnter(value); }
    ContentTransition getEnterTransition() const { return this->contentStack.getEnter(); }
    void setExitTransition(ContentTransition value) { this->contentStack.setExit(value); }
    ContentTransition getExitTransition() const { return this->contentStack.getExit(); }
    // 两边时长一起设（入场/出场各一段，时长相同）
    void setTransitionDuration(float seconds) { this->contentStack.setBoth(
        this->contentStack.getEnter(), seconds); }
    // 报**实际**时长：外观里写了 transitions 时是 spec 的总时长（出场播完才摘页靠它）
    float getTransitionDuration() const { return this->contentStack.getDuration(); }

    // 直接对某个视图播一次入场/出场（宿主也能用；出场回调里可以接着摘/删它）
    void playEnterTransition(View* view) { this->contentStack.playEnter(view); }
    void playExitTransition(View* view, std::function<void()> onFinished = nullptr)
    {
        this->contentStack.playExit(view, std::move(onFinished));
    }

    bool isEntering() const { return this->contentStack.isEntering(); }
    bool isExiting() const { return this->contentStack.isExiting(); }
    bool isTransitioning() const { return this->contentStack.isActive(); }
    // 正在出场的那个视图（入场期间是 nullptr；调试/测试用）
    View* getOutgoingContentView() const { return this->contentStack.getOutgoing(); }

    // 推进转场：update() 内部按帧间隔调；测试可以直接喂固定 dt（确定性）
    void stepContentTransition(float deltaSeconds) { this->contentStack.step(deltaSeconds); }
    // 立刻落位（把正在跑的那段收尾，含出场回调）
    void finishContentTransition() { this->contentStack.finishNow(); }

    // ---- B 键第 3 级（非模态）路由 ----
    // 模态子树和内容树里都没人处理 B 时冒泡到外壳，由它决定（返回上一页 / 退出）。
    // 注册后底栏提示会自动出现「⊖ 返回」；传 nullptr 注销。
    // handler 返回 true 表示已处理。
    void setOnBackRequested(std::function<bool()> handler);
    bool hasBackRequestedHandler() const { return this->onBackRequested != nullptr; }
    // 主动走一次 B 路由（宿主按键/快捷方式用），返回是否被处理
    bool requestBack();

    // ---- 安全区：五层统一内缩（Switch 的圆角 / 刘海）----
    void setSafeAreaInsets(float top, float right, float bottom, float left);
    void setSafeAreaInsets(const EdgeInsets& insets)
    {
        this->setSafeAreaInsets(insets.top, insets.right, insets.bottom, insets.left);
    }
    float getSafeAreaTop() const { return this->safeTop; }
    float getSafeAreaRight() const { return this->safeRight; }
    float getSafeAreaBottom() const { return this->safeBottom; }
    float getSafeAreaLeft() const { return this->safeLeft; }

    // ---- 过渡层（**仅动画期间存在**）----
    // 跨层飞行元素挂这里，别挂浮层：浮层是常驻的，飞行元素混进去会污染它，
    // 而且动画结束没人负责清理。第一次调用时创建（挂在最上层 = 最后绘制），
    // 层里没有子节点后，下一次 update() 把它从树上摘掉并释放。
    Box* getTransitionHost();
    bool hasTransitionHost() const { return this->transitionHost != nullptr; }
    void clearTransitionHost();

    // 把某个视图铺满它所在的那一层（四边 0 的绝对定位）。
    void fillLayer(View* view);

    void update() override;

  protected:
    void applySafeAreaToLayers();

  private:
    // 过渡层空了就摘掉释放（仅动画期存在）
    void releaseTransitionHostIfEmpty();

    Box* backdropLayer = nullptr;
    Box* contentLayer  = nullptr;
    Box* overlayLayer  = nullptr;
    Box* hudLayer      = nullptr;
    Box* debugLayer    = nullptr;

    FocusRingView* focusRingView = nullptr;
    Box* transitionHost          = nullptr;

    // 内容层的层级栈（下钻 + 转场 + 焦点/滚动记忆）
    ContentStack contentStack;

    // B 键路由：handler 为空时不注册动作（否则底栏会一直显示「返回」）
    std::function<bool()> onBackRequested;
    ActionIdentifier backActionId;
    bool backActionRegistered = false;

    float safeTop    = 0.0f;
    float safeRight  = 0.0f;
    float safeBottom = 0.0f;
    float safeLeft   = 0.0f;
};

} // namespace bklv
