#pragma once

#include "bklv/core/box.hpp"
#include "bklv/core/geometry.hpp"
#include "bklv/core/input.hpp"
// 转场（ContentTransition + ViewTransition）：入场/出场两个方向都在这里
#include "bklv/core/transition.hpp"
#include "bklv/core/time.hpp"

#include <functional>
#include <map>
#include <string>
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
//   DebugLayer     调试层   **在 HudLayer 之上**，永远最后绘制；日志窗口等调试 UI
//
// 契约：
//  - 只有内容层与覆盖层参与重排；背景层、浮层、调试层绝对定位，跟随视口尺寸
//  - 页头 / 底栏是**纯显示控件，永不参与焦点**
//  - 内容避让**不自动**：页面自己调 getHeaderInset()/getFooterInset()
//  - 页头显隐动画期间 insets 返回**目标值**，避免逐帧重排
class Shell : public Box
{
  public:
    Shell();
    ~Shell() override;

    Box* getBackdropLayer() const { return this->backdropLayer; }
    Box* getContentLayer() const { return this->contentLayer; }
    Box* getOverlayLayer() const { return this->overlayLayer; }
    Box* getHudLayer() const { return this->hudLayer; }
    // 调试层：**在 HudLayer 之上**（子节点顺序里排在最后 = 最后绘制），
    // 专门给调试 UI 用（日志窗口挂这里）。与 HudLayer 的区别：Hud 是业务浮层
    // （toast / 常驻指示，后加的东西会盖住前面的），调试层保证永远在最上面。
    // 它是个普通 Box：不可聚焦、不参与焦点链，所以挂进去的视图也不会吃到输入。
    Box* getDebugLayer() const { return this->debugLayer; }

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

    // ---- 按页背景（切页时淡入，见方案第 6 步）----
    //
    // 每个页面 id 一份背景视图，全部挂在背景层里，只有当前那份是 VISIBLE。
    // 切页时新的从 0 淡入、旧的垫在下面，补间结束才把旧的 GONE 掉。
    void setBackdropFor(const std::string& pageId, View* backdrop);
    void showBackdropFor(const std::string& pageId);
    View* getBackdropFor(const std::string& pageId) const;
    View* getCurrentBackdrop() const { return this->currentBackdrop; }
    const std::string& getCurrentBackdropId() const { return this->currentBackdropId; }
    void setBackdropTransitionDuration(float seconds) { this->backdropDuration = seconds; }
    float getBackdropTransitionDuration() const { return this->backdropDuration; }

    // ---- 过渡层（**仅动画期间存在**）----
    //
    // 跨层飞行元素挂这里，**不要**挂 HudLayer：Hud 是常驻浮层（toast/指示），
    // 飞行元素混进去会污染它，而且动画结束没人负责清理。
    // 第一次调 getTransitionHost() 时创建（挂在最上层 = 最后绘制），
    // 一旦层内没有子节点，下一次 update() 就把它从树上摘掉并释放。
    Box* getTransitionHost();
    bool hasTransitionHost() const { return this->transitionHost != nullptr; }
    // 立刻清空并释放过渡层（动画被打断/切页时用）
    void clearTransitionHost();

    // 角落 / 边缘定位（内部即绝对定位 + 边偏移；居中类会用一层铺满的包装盒）
    void addToCorner(View* view, Corner corner, float insetX, float insetY);
    void addToEdge(View* view, Edge edge, float inset);
    void fillLayer(View* view); // 四边 0，铺满该层（全屏遮罩/loading）

    // ---- 内容层的层级下钻 ----
    //
    // **同级换页**是 `Pager::show(id)` 的事（同一个槽里换内容）；
    // 这里只管"往上叠一层 / 退回上一层"这种层级导航（列表 → 详情，B 逐层返回）。
    // 栈顶那一层挂在内容层里；下面几层**只脱离树**（不删、不画、不进焦点链），
    // 所以"完全放弃旧 AppletFrame 的三段式"之后，`getContentLayer()->getChildrenCount()`
    // 仍然只反映当前那一层。
    void pushContentView(View* view);
    // 弹出栈顶并**释放**它（下钻出来的详情页是临时页）。转场播完才真正摘/删。
    // 已经在最外层时什么都不做，只执行回调。
    void popContentView(std::function<void()> callback = nullptr);
    View* getContentView() const
    {
        return this->contentStack.empty() ? nullptr : this->contentStack.back();
    }
    size_t getContentStackSize() const { return this->contentStack.size(); }

    // ---- 内容栈的转场：**入场 / 出场分开**（时间单位是秒）----
    //
    // 入场 = 新层滑入/淡入；出场 = 旧层滑出/淡出，**出场播完才摘/删它**
    // （所以下钻/返回都能看到两层各动一次，不再是"旧层瞬间消失"）。
    void setEnterTransition(ContentTransition value)
    {
        this->transition.setEnter(value, this->transition.getEnterDuration());
    }
    ContentTransition getEnterTransition() const { return this->transition.getEnter(); }
    void setExitTransition(ContentTransition value)
    {
        this->transition.setExit(value, this->transition.getExitDuration());
    }
    ContentTransition getExitTransition() const { return this->transition.getExit(); }

    // 入场/出场各一段时长
    void setTransitionDuration(float seconds)
    {
        this->transition.setBoth(this->transition.getEnter(), seconds);
    }
    float getTransitionDuration() const { return this->transition.getEnterDuration(); }

    // 旧的"一个开关"写法：两边都设成同一个（保留给已有调用方/测试）
    void setContentTransition(ContentTransition value)
    {
        this->transition.setBoth(value, this->transition.getEnterDuration());
    }
    ContentTransition getContentTransition() const { return this->transition.getEnter(); }
    void setContentTransitionDuration(float seconds) { this->setTransitionDuration(seconds); }
    float getContentTransitionDuration() const { return this->getTransitionDuration(); }

    // 直接对某个视图播一次入场/出场（宿主也能用；出场回调里可以接着摘/删它）
    void playEnterTransition(View* view);
    void playExitTransition(View* view, std::function<void()> onFinished = nullptr);

    bool isEntering() const { return this->transition.isEntering(); }
    bool isExiting() const { return this->transition.isExiting(); }
    bool isTransitioning() const { return this->transition.isActive(); }
    // 正在转场？（转场期间焦点先不挪，等落位再给新页）
    bool isContentTransitioning() const { return this->transition.isActive(); }

    // 推进转场：update() 内部按帧间隔调；测试可以直接喂固定 dt（确定性）
    void stepContentTransition(float deltaSeconds);
    // 立刻落位（把正在跑的那段收尾，含出场回调）
    void finishContentTransition() { this->transition.finishNow(); }

    // 正在出场的那个视图（入场期间是 nullptr；调试/测试用）
    View* getOutgoingContentView() const { return this->transition.getOutgoing(); }

    void update() override;

    // 外壳自己的尺寸（铺满）与页面底色来自 widgets/applet_frame.json（原 AppletFrame 的外观）
    void applyTheme() override;

    // 背景淡入的确定性步进（正常由 update() 按帧间隔调；测试直接喂固定 dt，
    // 和 AppletFrame::stepContentTransition 同一套路）
    void stepBackdropTransition(float deltaSeconds);

  protected:
    void applySafeAreaToLayers();

  private:
    // 模态子树里没人消费按键时的兜底：目前只处理 closeOnBack（按 B 关掉最上层弹窗）。
    bool handleModalUnhandled(ControllerButton button);

    // 过渡层空了就摘掉释放（仅动画期存在）
    void releaseTransitionHostIfEmpty();

    // 内容栈内部：把 view 挂到内容层、并把上一任摘下来（不删）
    void attachContentView(View* view);
    // 摘掉当前挂着的层（不删），把 contentView 置空 —— 出场结束后的收尾
    View* detachContentView();

    Box* backdropLayer = nullptr;
    Box* contentLayer  = nullptr;
    Box* overlayLayer  = nullptr;
    Box* hudLayer      = nullptr;
    Box* debugLayer    = nullptr;

    std::vector<View*> dialogStack;
    std::vector<View*> dimStack;
    // 内容层的下钻栈（栈底是基础内容，栈顶是当前显示的层）
    std::vector<View*> contentStack;
    // 当前**挂在树里**的那一层（== contentStack.back()；转场期间旧层已脱离）
    View* contentView = nullptr;

    // 内容栈转场（入场/出场两个方向；一次只跑一段）
    ViewTransition transition;
    std::function<void()> pendingPopCallback;
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

    // 按页背景：id → 视图；只有 currentBackdrop 是 VISIBLE
    std::map<std::string, View*> backdrops;
    View* currentBackdrop    = nullptr;
    View* previousBackdrop   = nullptr; // 淡入期间垫在下面的旧背景
    std::string currentBackdropId;
    float backdropDuration   = 0.22f;    // 秒
    Animatable backdropAnim{ 1.0f };

    // 过渡层（懒创建；空了就释放）
    Box* transitionHost = nullptr;
};

} // namespace bklv
