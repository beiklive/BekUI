#pragma once

// 视图基类，对应 borealis 的 core/view.hpp。
//
// 核心设计（与 borealis 一致）：**没有独立的渲染树**，一个 View 持有一个 YGNode。
//   View::ygNode  ──YGNodeSetContext──▶  View*
// 布局由 Yoga 递归计算，绘制由 View::draw 递归完成，两者共用同一棵树。
//
// 与 borealis 的差异（本轮刻意简化）：
//   - 绘制后端从 NVGcontext* 换成 ImDrawList*
//   - 去掉阴影、渐变、焦点高亮、点击动画等 NanoVG 专有能力
//   - 去掉 XML 反序列化与主题文件叠加

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <yoga/Yoga.h>

#include "bklv/core/attributes.hpp"
#include "bklv/core/geometry.hpp"
#include "bklv/core/gesture.hpp"
#include "bklv/core/transform.hpp"
#include "bklv/core/input.hpp"

namespace bklv
{

class Box;
class InputManager;

// 每帧共享的上下文，对应 borealis 的 FrameContext（去掉了 fontStash）。
class FrameContext
{
  public:
    ImDrawList* drawList = nullptr; // 本帧的绘制目标，等价于 borealis 的 NVGcontext*
    float pixelRatio     = 1.0f;
};

// 主轴方向，对应 borealis 的 Box::Axis。
enum class Axis
{
    ROW,    // 横向排列
    COLUMN, // 纵向排列
};

// 主轴排列方向，对应 borealis 的 Direction。
enum class Direction
{
    INHERIT,
    LEFT_TO_RIGHT,
    RIGHT_TO_LEFT,
};

// 主轴分布，对应 borealis 的 JustifyContent。
enum class JustifyContent
{
    DEFAULT, // 等于 FLEX_START
    FLEX_START,
    CENTER,
    FLEX_END,
    SPACE_BETWEEN,
    SPACE_AROUND,
    SPACE_EVENLY,
};

// 交叉轴对齐，对应 borealis 的 AlignItems。
enum class AlignItems
{
    DEFAULT, // 等于 STRETCH
    AUTO,
    FLEX_START,
    CENTER,
    FLEX_END,
    STRETCH,
    BASELINE,
};

// 单个子项的自定义对齐，对应 borealis 的 AlignSelf。
enum class AlignSelf
{
    AUTO,
    FLEX_START,
    CENTER,
    FLEX_END,
    STRETCH,
    BASELINE,
};

// 定位方式，对应 borealis 的 PositionType。
enum class PositionType
{
    RELATIVE,
    ABSOLUTE,
};

// 显示与隐藏，对应 borealis 的 Visibility。
enum class Visibility
{
    VISIBLE,
    INVISIBLE, // 占位但不绘制、不响应输入
    GONE,      // 不占位，等于 display:none
};

// 尺寸特例：AUTO 表示由内容决定，对应 Yoga 的 YGUndefined。
constexpr float AUTO = NAN;

class View
{
  public:
    View();
    virtual ~View();

    View(const View&)            = delete;
    View& operator=(const View&) = delete;

    // ---- 布局：尺寸 ----
    void setWidth(float width);
    void setHeight(float height);
    void setDimensions(float width, float height);
    void setSize(Size size);
    void setWidthPercentage(float percentage);
    void setHeightPercentage(float percentage);

    void setMinWidth(float minWidth);
    void setMinHeight(float minHeight);
    void setMaxWidth(float maxWidth);
    void setMaxHeight(float maxHeight);
    void setMinWidthPercentage(float percentage);
    void setMinHeightPercentage(float percentage);
    void setMaxWidthPercentage(float percentage);
    void setMaxHeightPercentage(float percentage);

    // ---- 布局：弹性 ----
    void setGrow(float grow);

    // flex 基准尺寸。grow 要和 basis 0 一起用才是"占满剩余空间"：
    // basis 默认 auto（=内容尺寸）时，内容一大，Yoga 会按比例连兄弟节点一起压缩
    // （踩过：内容区里的滚动器把同列的按钮行从 48 压到 28）。
    void setFlexBasis(float basis);
    void setShrink(float shrink);
    float getGrow() const;

    // ---- 布局：外边距 ----
    void setMargins(float top, float right, float bottom, float left);
    void setMargin(float margin);
    void setMarginTop(float top);
    void setMarginRight(float right);
    void setMarginBottom(float bottom);
    void setMarginLeft(float left);
    float getMarginTop() const;
    float getMarginRight() const;
    float getMarginBottom() const;
    float getMarginLeft() const;

    // ---- 布局：绝对定位偏移 ----
    void setPositionTop(float pos);
    void setPositionRight(float pos);
    void setPositionBottom(float pos);
    void setPositionLeft(float pos);
    void setPositionTopPercentage(float percentage);
    void setPositionRightPercentage(float percentage);
    void setPositionBottomPercentage(float percentage);
    void setPositionLeftPercentage(float percentage);
    void setPositionType(PositionType type);

    // ---- 布局：自身对齐 ----
    void setAlignSelf(AlignSelf align);

    // ---- 布局：显示 ----
    void setVisibility(Visibility visibility);
    Visibility getVisibility() const { return this->visibility; }

    // ---- 布局结果读取（Yoga 坐标系：相对父节点的偏移） ----
    Rect getFrame() const;
    Rect getLocalFrame() const;
    float getX() const;
    float getY() const;
    float getLocalX() const;
    float getLocalY() const;
    float getWidth() const;
    float getHeight() const;

    // ---- 外观（本轮只保留 ImDrawList 能直接画的部分） ----
    void setBackgroundColor(ImU32 color);
    ImU32 getBackgroundColor() const { return this->backgroundColor; }

    void setBorderColor(ImU32 color);
    void setBorderThickness(float thickness);
    float getBorderThickness() const { return this->borderThickness; }

    void setCornerRadius(float radius);
    float getCornerRadius() const { return this->cornerRadius; }

    void setAlpha(float alpha);
    float getAlpha(bool child = false) const;

    void setClipsToBounds(bool clips) { this->clipsToBounds = clips; }
    bool getClipsToBounds() const { return this->clipsToBounds; }

    void setDetached(bool detached) { this->detached = detached; }
    bool isDetached() const { return this->detached; }

    // 额外偏移，用于滚动、拖拽等不改布局的位移。
    void setTranslation(float x, float y);
    void setTranslationX(float x) { this->setTranslation(x, this->translation.y); }
    void setTranslationY(float y) { this->setTranslation(this->translation.x, y); }
    Point getTranslation() const { return this->translation; }

    // ---- 绘制期变换（缩放 / 旋转 / 斜切 / 位移，字形也一起变）----
    //
    // 和上面的 setTranslation 不同：setTranslation 参与布局（回收器就用它摆 cell），
    // 这里的变换**只管画**，布局与命中测试仍按原矩形（和 borealis 一致）。
    void setTranslateX(float x) { this->transform.translateX = x; }
    void setTranslateY(float y) { this->transform.translateY = y; }
    void setScaleX(float value) { this->transform.scaleX = value; }
    void setScaleY(float value) { this->transform.scaleY = value; }
    void setSkewX(float radians) { this->transform.skewX = radians; }
    void setSkewY(float radians) { this->transform.skewY = radians; }
    void setRotate(float radians) { this->transform.rotation = radians; }
    // 变换中心（不设则用控件中心）
    void setTransformOrigin(float x, float y)
    {
        this->transform.origin    = Point(x, y);
        this->transform.hasOrigin = true;
    }

    float getScaleX() const { return this->transform.scaleX; }
    float getScaleY() const { return this->transform.scaleY; }
    float getSkewX() const { return this->transform.skewX; }
    float getSkewY() const { return this->transform.skewY; }
    float getRotate() const { return this->transform.rotation; }
    const Transform2D& getTransform() const { return this->transform; }
    void resetTransform() { this->transform.reset(); }

    // ---- 手势（指针事件由 InputManager 派发过来）----
    //
    // 生命周期归 View：收 unique_ptr，调用方不用管释放。
    void addGesture(std::unique_ptr<GestureRecognizer> gesture);
    void clearGestures() { this->gestures.clear(); }
    const std::vector<std::unique_ptr<GestureRecognizer>>& getGestures() const
    {
        return this->gestures;
    }

    // 把一次手势事件交给自己（再沿父链往上传）。返回 true 表示有人吃掉了。
    // 顺序：自己 → 父 → ... → 根；每层内部按添加顺序问识别器。
    bool dispatchGesture(GestureState state, const Point& position, const Point& delta);

    // ---- 焦点 ----

    // 这个 View 能否成为焦点（可点、可操作）。
    virtual bool isFocusable() const { return false; }

    // 进入这个容器时默认选中的子视图。
    virtual View* getDefaultFocus() { return nullptr; }

    // 按方向找下一个焦点；默认实现交给父节点处理。
    virtual View* getNextFocus(FocusDirection direction, View* currentView);

    // 命中测试（屏幕坐标）。
    virtual View* hitTest(Point point);

    virtual void onFocusGained() { }
    virtual void onFocusLost() { }
    bool isFocused() const { return this->focused; }

    // ---- 动作 ----

    ActionIdentifier registerAction(ControllerButton button, const std::string& hintText,
        const ActionListener& listener, bool hidden = false);
    void unregisterAction(ActionIdentifier identifier);
    // 返回是否消费了这次按键。
    // pointer = true 表示由鼠标点击触发。控件可以用它区分"点一下就执行"和"只选中"。
    bool triggerAction(ControllerButton button, bool pointer = false);

    const std::vector<InputAction>& getActions() const { return this->actions; }

    // ---- 视图树 ----
    void setId(const std::string& id);
    const std::string& getId() const { return this->id; }
    virtual std::string getClassString() const;
    std::string describe() const;

    // ---- 主题（见 core/appearance.hpp）----

    // 换肤时被调用：把主题值取到成员里（之后 draw 只读成员，不查主题）。
    // 默认空实现；控件重写。只应该覆盖**未被显式设置过**的值——
    // 用户代码 setFontSize(18) 之后，换肤不该把它冲掉。
    virtual void applyTheme() { }

    // 递归套用（含子节点）。App::setTheme 会调。
    void applyThemeRecursive();

    // 递归把整棵子树的测量标脏（字体/UI 缩放变了要重算文本尺寸）
    void markMeasureDirtyRecursive();

    // ---- JSON 视图描述用的属性系统（见 core/attributes.hpp）----

    // 该类的属性表。子类重写成返回自己的静态表；基类返回空表。
    virtual const AttributeTable& attributes() const;

    // 应用一个属性：先查子类表，再查公共表（尺寸/间距/外观/可见性等，见 view.cpp）。
    // NotHandled 表示两张表都不认识这个属性，由调用方决定记账方式。
    // Failed 表示认识但值不合法，error 里是给人看的说明。
    ApplyResult applyAttribute(const std::string& name, const Value& value, std::string& error);

    Box* getParent() { return this->parent; }
    const Box* getParent() const { return this->parent; }
    bool hasParent() const { return this->parent != nullptr; }

    // 按 id 在自己的子树里查找，找不到返回 nullptr。
    virtual View* getView(const std::string& id);

    // 类型判断用虚函数而不是 dynamic_cast：
    // Switch 工具链按 devkitPro 惯例带 -fno-rtti，用不了 dynamic_cast。
    virtual Box* asBox() { return nullptr; }

    // ---- 布局失效 ----
    // 修改任何影响布局的属性后必须调用，冒泡到根后触发一次全树重算。
    virtual void invalidate();

    // 布局计算完成后调用。用 Yoga 的测量函数算出最终宽度时，文本换行等
    // "依赖最终尺寸"的工作放在这里做。
    virtual void onLayout() { }

    // 每帧逻辑推进（滚动、动画等）。不负责绘制。
    virtual void update() { }

    // 递归通知整棵子树：布局已更新。
    void notifyLayout();

    // 默认字体加载完成后由 App 调用：若字体版本变了就冒泡到根做一次全树重算。
    // 第一次构建视图树时字体还没加载，文本会测出 0 宽并被 Yoga 缓存，必须重算。
    void refreshFontDependentLayout();

    // ---- 自定义测量 ----
    // 需要按内容决定尺寸的 View（如文本）在构造函数里注册测量函数。
    YGSize measure(float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode);

    // 由 Box 在增删子节点时调用，维护 parent 指针。子类可以重写以感知父子关系变化。
    virtual void setParent(Box* parent) { this->parent = parent; }

    // ---- 绘制入口 ----
    // 等价于 borealis 的 View::frame：先画通用外观，再调用子类的 draw。
    virtual void frame(FrameContext* ctx);

    YGNode* getYGNode() const { return this->ygNode; }

  protected:
    // 鼠标点击本视图时是否直接执行动作（默认 false：点击只聚焦，不执行）。
    // 按钮类控件覆写为 true 更符合直觉；列表项保持默认，避免点一下就被触发。
    bool onPointerAction = false;

    // 自定义测量函数。第一个参数是触发测量的节点，实现里一般用不到。
    using MeasureFunction = std::function<YGSize(const void* node, float width, YGMeasureMode widthMode,
        float height, YGMeasureMode heightMode)>;

    // 注册后该 View 会被 Yoga 当作叶子节点，由回调决定尺寸。
    void setMeasureFunction(MeasureFunction function);

    // 子类唯一需要实现的绘制函数，坐标系原点在本 View 左上角。
    virtual void draw(ImDrawList* drawList, float x, float y, float width, float height, FrameContext* ctx) = 0;

    // 递归绘制子节点，只有 Box 会重写它。
    virtual void drawChildren(ImDrawList*, float, float, float, float, FrameContext*) { }

    // 在裁剪区域内绘制自己的内容，子类一般调用它而不是直接重写 frame。
    void drawBackground(ImDrawList* drawList, float x, float y, float width, float height);
    void drawBorder(ImDrawList* drawList, float x, float y, float width, float height);

    // 布局需要重新计算时由子类调用；与 invalidate 的区别是它会额外把自己的测量函数标脏。
    void markMeasureDirty();

    Box* parent = nullptr;

    YGNode* ygNode = nullptr;

    MeasureFunction measureFunction;

  private:
    // 输入层需要读写焦点状态、翻译偏移和动作表。
    friend class InputManager;

    std::string id;
    Visibility visibility = Visibility::VISIBLE;

    ImU32 backgroundColor = 0; // 0 表示不画背景
    ImU32 borderColor     = 0;
    float borderThickness = 0.0f;
    float cornerRadius    = 0.0f;
    float alpha           = 1.0f;
    bool detached         = false;
    unsigned int fontGeneration = 0; // 上一次布局所用的字体版本
    Point translation;               // 滚动/拖拽偏移（参与布局）
    Transform2D transform;           // 绘制期变换（不影响布局）
    std::vector<std::unique_ptr<GestureRecognizer>> gestures;
    bool focused = false;
    std::vector<InputAction> actions;
    ActionIdentifier nextActionIdentifier = 1;

  protected:
    // 子类做剔除时要知道自己是否裁剪。
    bool clipsToBounds = false;
};

} // namespace bklv
