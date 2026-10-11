# wiliwili 的分层架构：Yoga 与绘制是如何被逐层封装的

> 这份是动手前的架构分析记录，写的时候仓库里还有 `demo_01`~`demo_07`
> 那批学习示例。那些示例后来已删除，仓库结构也改成了 `bklv/`（库）+ `demo/`（展示）。
> 文中的路径与 demo 编号按当时的写法保留，作为决策依据，不代表当前状态。

阅读对象：`~/Code/C++/example/wiliwili`（GPL-3.0）及其子模块 `library/borealis`
（`https://github.com/xfangfang/borealis`，commit `5f08b286`，分支 `wiliwili`）。

结论先行：**wiliwili 自己完全不碰 Yoga 和绘制**。它用的是 borealis 这套声明式 UI 框架，
由 borealis 把 Yoga（布局）、NanoVG（矢量绘制）、平台窗口/输入（GLFW/SDL3/switch 等）
封装成 `View` 树。wiliwili 只做三件事：继承 `View`/`Box` 写业务控件、写 XML 布局、
用 `BRLS_BIND` 把控件和逻辑接起来。

---

## 1. 总体分层

```
┌──────────────────────────────────────────────────────────────┐
│ wiliwili 应用层（GPL-3.0）                                    │
│   Activity（页面）→ Fragment（子页面）                        │
│   include/view/*.hpp：RecyclingGrid/VideoView/TextBox/...     │
│   resources/xml/**：101 个 XML 布局                            │
│   只写业务，不直接调 Yoga / NanoVG（个别地方例外）             │
├──────────────────────────────────────────────────────────────┤
│ borealis 框架层（Apache-2.0）                                 │
│   core/view.hpp/.cpp    View 树 = Yoga 树 + 绘制 + 输入 + 焦点 │
│   core/box.hpp/.cpp     容器：子节点增删、绘制遍历、裁剪剔除   │
│   views/*.hpp/.cpp      内建控件：Label/Image/Button/Slider/…  │
│   core/style|theme      样式与主题（JSON/XML 驱动）            │
│   core/application.cpp  主循环、活动栈、帧管线、全局焦点        │
│   core/animation+time   基于时间的补间动画（替代 tweeny 的角色）│
├──────────────────────────────────────────────────────────────┤
│ 抽象 / 第三方                                                │
│   lib/extern/yoga       Flexbox 布局引擎（源码内嵌）           │
│   lib/extern/nanovg     矢量绘制（含 dk_renderer 扩展）        │
│   libretro-common       stb/图像解码等                        │
├──────────────────────────────────────────────────────────────┤
│ 平台层 platforms/{glfw,sdl,switch,ps4,psv,android}            │
│   VideoContext（窗口+GL 上下文+nvg 后端）、InputManager、      │
│   FontLoader、AudioPlayer、ImeManager                         │
└──────────────────────────────────────────────────────────────┘
```

一句话概括体积分布：`view.cpp` 2590 行 + `box.cpp` 775 行是这套封装的真正核心；
`application.cpp` 1340 行负责把一切串起来；`views/` 下每个控件 100~700 行。

---

## 2. Node 与 Render 合一：`View` 同时是 Yoga 节点和绘制单元

这是整套封装最关键的决策：**没有独立的 render tree**。

`library/include/borealis/core/view.hpp:231`

```cpp
class View
{
protected:
    YGNode* ygNode;                                  // :346 一个 View 持有一个 Yoga 节点
    virtual void draw(NVGcontext* vg, float x, float y,
                      float width, float height,
                      Style style, FrameContext* ctx) = 0;   // :1271 唯一的绘制入口（纯虚）
    virtual void onLayout() {}                        // :418 布局完成后的回调
public:
    YGNode* getYGNode();                              // :1226 交给 Box 插入父子关系
    void invalidate();                                 // :413 脏标记 + 触发重算
};
```

构造时即建立绑定（`view.cpp:65`）：

```cpp
View::View()
{
    this->ygNode = YGNodeNew();
    YGNodeSetContext(this->ygNode, this);   // ← 反向指针：Yoga 回调里能拿回 View
    YGNodeStyleSetWidthAuto(this->ygNode);
    YGNodeStyleSetHeightAuto(this->ygNode);
    ...
}
```

`YGNodeSetContext(this, ...)` 是全部魔法的根：后文所有 Yoga → C++ 的回调
（measure 函数、NodeLayout 事件）都靠它把 `YGNode*` 还原成 `View*`。

### 布局 API 逐条透传

`View` 把 Yoga 的样式 API 一对一暴露成 setter，语义几乎无损：

| View 方法 | Yoga 调用（view.cpp） |
|---|---|
| `setWidth/setHeight` | `YGNodeStyleSetWidth/Height` |
| `setWidthPercentage` | `YGNodeStyleSetWidthPercent` |
| `setMin/MaxWidth(Percentage)` | `YGNodeStyleSetMin/MaxWidth(+Percent)` |
| `setGrow/setShrink` | `YGNodeStyleSetFlexGrow/FlexShrink` |
| `setMargins(top,right,bottom,left)` | `YGNodeStyleSetMargin` × 4 边 |
| `setPositionTop/…(Percentage)` | `YGNodeStyleSetPosition` / `SetPositionPercent` |
| `setPositionType` | `YGNodeStyleSetPositionType`（`Relative`/`Absolute`） |
| `setAlignSelf` | `YGNodeStyleSetAlignSelf` |
| `setVisibility` | `YGNodeStyleSetDisplay` |
| `AUTO` 常量（`= NAN`，view.hpp:384） | `YGUndefined`，即"由内容决定" |

`Box` 额外提供容器级样式（box.cpp）：`setPadding*` → `YGNodeStyleSetPadding`、
`setAxis` → `YGNodeStyleSetFlexDirection`、`setJustifyContent` / `setAlignItems` /
`setDirection` → 枚举映射（`YGFlexDirection*` / `YGJustify*` / `YGAlign*` / `YGDirection*`）。

所以对使用者来说，写 `box->setGrow(1.0f)` 就等于写 Yoga，但不用管 `YGNodeRef` 生命周期、
不用手动 `YGNodeFreeRecursive`（`View` 析构时统一处理）。

---

## 3. 布局管线：按需重算 + 事件回调

### 3.1 触发链

`view.cpp:1212`

```cpp
void View::invalidate()
{
    if (YGNodeHasMeasureFunc(this->ygNode))
        YGNodeMarkDirty(this->ygNode);          // 文本类节点要重跑 measure

    if (this->hasParent() && !this->detached)
        this->getParent()->invalidate();        // 一路冒泡到根
    else
        YGNodeCalculateLayout(this->ygNode, YGUndefined, YGUndefined, YGDirectionLTR);
}
```

要点：

- **全工程只有这一处** `YGNodeCalculateLayout`（已核实 `library/` 下除 yoga 自带
  tests 外仅此 1 处）。任何尺寸/内容变化都是"改样式 → `invalidate()` → 冒泡到根 → 从根重算"，
  没有增量布局。
- 宽高都传 `YGUndefined`，由根节点自身样式（`setDimensions`）决定。
- 用户自定义 measure 函数（如文本）配合 `YGNodeMarkDirty` 才能让 Yoga 重跑测量。

### 3.2 根节点尺寸从哪来

窗口尺寸 → 活动 → 根视图，三段式：

```cpp
// application.cpp:1167
void Application::setWindowSize(int width, int height)
{
    Application::windowWidth  = width;
    Application::windowHeight = height;
    Application::windowScale   = (float)width / (float)ORIGINAL_WINDOW_WIDTH; // 1280
    Application::contentWidth  = (float)ORIGINAL_WINDOW_WIDTH;                 // 逻辑宽恒定 1280
    Application::contentHeight = std::ceil((float)height / Application::windowScale);
    for (Activity* activity : Application::activitiesStack)
        activity->onWindowSizeChanged();
}

// activity.cpp:104 / activity.cpp:52
void Activity::onWindowSizeChanged() { this->resizeToFitWindow(); }
void Activity::resizeToFitWindow()  { this->contentView->setDimensions(
                                          Application::contentWidth, Application::contentHeight); }
```

即 **UI 按固定逻辑宽 1280 布局**，其余靠 `nvgScale(vg, windowScale, windowScale)` 放大
（application.cpp:764）。窗口缩放走 `onWindowResized` → `brls::delay(100, ...)` 防抖后再设尺寸。

### 3.3 布局完成回调

不给每个节点写回调，而是订阅 Yoga 的全局事件（application.cpp:148）：

```cpp
YGConfig* defaultConfig = YGConfigGetDefault();
defaultConfig->setUseWebDefaults(true);     // flex-direction 默认 row、flex-shrink 默认 1
facebook::yoga::Event::subscribe([](const YGNode& node,
                                    facebook::yoga::Event::Type eventType,
                                    facebook::yoga::Event::Data eventData) {
    View* view = (View*)node.getContext();               // ← 回到 §2 的 context 绑定
    if (!view) return;
    if (eventType == facebook::yoga::Event::NodeLayout)
        view->onLayout();                                 // 每个节点布局后收到通知
});
```

`onLayout()` 的典型用法是"布局算完之后再做只能在这时做的事"：

- `label.cpp:583` `Label::onLayout()`：拿到最终宽度后，用 `nvgTextGlyphPositions` 逐个字形
  比较，算出省略号截断位置（`truncatedText`），并决定是否启动跑马灯。
- `scrolling_frame.cpp:319` `ScrollingFrame::onLayout()`：把自己的宽度同步给 `contentView`
  并再次 `invalidate()`。

---

## 4. 测量（measure）：让 Yoga 知道文本有多宽

Yoga 只管几何，不认字形。文本尺寸通过自定义 measure 函数注入，**measure 内部直接调 NanoVG**
（`views/label.cpp:74`）：

```cpp
static YGSize labelMeasureFunc(YGNodeRef node, float width, YGMeasureMode widthMode,
                               float height, YGMeasureMode heightMode)
{
    NVGcontext* vg = Application::getNVGContext();
    auto* label    = (Label*)YGNodeGetContext(node);      // context 反查
    ...
    nvgFontSize(vg, label->getFontSize());
    nvgFontFaceId(vg, label->getFont());
    nvgTextBounds(vg, 0, 0, fullText.c_str(), nullptr, bounds);   // 量宽度
    ...
    if (availableWidth < requiredWidth && !label->isSingleLine()) {
        nvgTextBoxBounds(vg, 0, 0, availableWidth, fullText.c_str(), nullptr, boxBounds); // 量换行高度
        ...
    }
    return size;                                          // 回填给 Yoga
}
```

注册在构造函数里，并附带兜底样式（`label.cpp:223`）：

```cpp
YGNodeSetMeasureFunc(this->ygNode, labelMeasureFunc);
YGNodeStyleSetMaxWidthPercent(this->ygNode, 100);    // 防止文本溢出父容器
YGNodeStyleSetMaxHeightPercent(this->ygNode, 100);
```

副作用也一并封在这里：measure 顺手把 `ellipsisWidth`、`requiredWidth` 存进 View，
给后面的 `onLayout()` 和 `draw()` 复用——**一次测量，三处使用**。

---

## 5. 绘制管线：一次 nvgBeginFrame，递归 frame 树

### 5.1 帧入口

`application.cpp:746` 是整帧的唯一入口，顺序固定：

```cpp
void Application::frame()
{
    FrameContext frameContext;                      // { vg, pixelRatio, fontStash, theme }
    videoContext->beginFrame();
    videoContext->clear(theme.getColor("brls/clear"));
    nvgBeginFrame(vg, windowWidth, windowHeight, scaleFactor);
    nvgScale(vg, Application::windowScale, Application::windowScale);

    // 从活动栈顶往下收集：直到遇到非半透明活动为止（半透明活动会叠加显示）
    std::vector<View*> viewsToDraw;                 // application.cpp:766-780
    for (i...) { viewsToDraw.push_back(activity->getContentView());
                 if (!activity->isTranslucent()) break; }

    for (i...) viewsToDraw[...]->frame(&frameContext);   // 画活动内容
    if (currentFocus && inputType != TOUCH) currentFocus->frameHighlight(&frameContext);
    notificationManager->frame(&frameContext);      // 通知
    if (isDrawCursor()) inputManager->drawCursor(vg); // 触摸光标
    if (debuggingViewEnabled) debugLayer->frame(&frameContext);  // 线框/性能层

    nvgResetTransform(vg);
    nvgEndFrame(vg);
    videoContext->endFrame();
}
```

### 5.2 单个 View 的绘制是"模板方法"

`view.cpp:162` `View::frame()` 定义了固定的绘制次序，子类只实现中间的 `draw()`：

```
nvgSave
 ├─ drawBackground()        背景（颜色/渐变 + 圆角）
 ├─ drawShadow()            阴影（4 种 ShadowType）
 ├─ drawBorder()            边框
 ├─ drawLine()              4 边独立线宽
 ├─ drawHighlight()         焦点高亮底（focus 才有，alpha 补间）
 ├─ drawClickAnimation()    按下反馈
 ├─ [nvgIntersectScissor]   collapseState<1 或 clipsToBounds 时裁剪
 ├─ draw()  ← 子类实现，收到 (x, y, width, height)
 ├─ drawWireframe()         调试线框
 └─ [nvgRestore]
nvgRestore
```

子类完全不关心圆角、阴影、边框、裁剪、透明度这些通用视觉，只负责"我这块矩形里画什么"。
`View::a(color)`（view.cpp:104）递归乘上父链 alpha，实现整棵子树的淡入淡出：

```cpp
float View::getAlpha(bool child) { return alpha * (parent ? parent->getAlpha(true) : 1.0f); }
```

### 5.3 容器绘制 + 剔除

`box.cpp:121` `Box::draw()` 只做一件事：按子节点顺序递归 `child->frame(ctx)`，
中途对**叶子节点**做多层边界剔除（`isCulled()` 且不在所有祖先包围盒内就跳过），
嵌套 Box 不做剔除（自己会做）。

坐标系是**相对父节点的增量**，不是绝对坐标（`view.cpp:1228`）：

```cpp
float View::getX() {
    if (hasParent()) return getParent()->getX() + YGNodeLayoutGetLeft(ygNode)
                                             + translation.x + (detached ? detachedOrigin.x : 0);
    return YGNodeLayoutGetLeft(ygNode) + translation.x;
}
```

`YGNodeLayoutGet*` 读的是父节点坐标系里的相对偏移，逐级累加得到屏幕坐标。
`translation.x/y` 供动画/滚动叠加偏移，`detached` 供弹窗类脱离布局流使用。

---

## 6. 输入 / 焦点：和布局同源的遍历

- **命中测试** `Box::hitTest(Point)`（box.cpp:384）沿 View 树自顶向下找可见且包含该点的 View。
- **手柄/键盘焦点** `Box::getNextFocus(FocusDirection, View*)`（box.cpp:409）按
  `getParentNavigationDecision` 在树里做方向导航；`View` 自己持有 `highlightAlpha`
  等焦点动画状态，绘制阶段统一处理。
- **动作绑定** `View::registerAction(hint, ButtonType, listener)`（view.cpp:765）
  注册后自动出现在底部提示栏（`Hints`），不需要业务层自己画提示。
- 手势在 `core/touch/`、`core/gesture.hpp`，`gestureRecognizerRequest` 沿父链冒泡。

---

## 7. 声明式层：XML + 绑定

### 7.1 注册工厂

`application.cpp:1299` `registerBuiltInXMLViews()` 把 `brls:Box`、`brls:Label`、
`brls:Image`、`brls:ScrollingFrame`、各种 Cell 等注册进全局表；
wiliwili 自己再注册业务控件（`Register::initCustomView()`，main.cpp:51）。
每个 View 类提供 `static View* create()` 作为工厂。

### 7.2 XML 就是 Flexbox

属性名和 View 的 setter 同名，所以 XML 属性 → `setXxx()` 是一层薄薄的反射
（`View::registerFloatXMLAttribute` / `registerStringXMLAttribute` /
`BRLS_REGISTER_ENUM_XML_ATTRIBUTE`，见 label.cpp:231-273）：

```xml
<!-- resources/xml/views/inbox_user.xml -->
<brls:Box alignItems="center" paddingLeft="15" paddingRight="20"
         width="auto" height="auto">
    <brls:Rectangle id="badge" width="8" height="8" cornerRadius="8"
                    visibility="invisible" color="@theme/color/tip/red"/>
    <UserInfoView height="100%" grow="1" id="chat/talker"/>
    <brls:Label id="inbox/lastTime" minWidth="50" fontSize="16" shrink="0"
                horizontalAlign="right" textColor="@theme/font/grey"/>
</brls:Box>
```

`width="auto"` = `AUTO` = `YGUndefined`；`grow="1"` = `YGNodeStyleSetFlexGrow`；
`@theme/...`、`@style/...` 分别是主题/样式引用（`resources/xml` 共 101 个文件）。

### 7.3 C++ 侧取控件引用

```cpp
class MainActivity : public brls::Activity
{
public:
    CONTENT_FROM_XML_RES("activity/main.xml");   // 本页内容来自 XML
    void onContentAvailable() override;
private:
    BRLS_BIND(CustomButton, settingBtn, "main/setting");   // 按 XML 里的 id 类型安全绑定
    BRLS_BIND(AutoTabFrame, tabFrame, "main/tabFrame");
};
```

`BRLS_BIND(Type, name, id)` 由 `getView(id)` + `dynamic_cast` 实现，绑定失败即报错。

---

## 8. 业务层怎么扩展（wiliwili 的真实做法）

| 扩展点 | 做法 | 例 |
|---|---|---|
| 新控件，走布局 | 继承 `brls::Box` 组合现成控件 | `RecyclingGridItem`、`VideoCard`、`CustomButton` |
| 重写视觉 | 继承 `View`/`Label` 重写 `draw()` | `BiliCheckBox`（`View`）、`TextBox`（`Label`） |
| 需要按内容算尺寸 | 在构造函数里 `YGNodeSetMeasureFunc` + 重写 `onLayout()` | `TextBox`（text_box.cpp:114） |
| 直接改布局算法 | 直接调 Yoga API（极少） | `pgc_index_activity.cpp:88` `YGNodeStyleSetFlexWrap(...YGWrapWrap)` |
| 列表/虚拟滚动 | 继承 `brls::ScrollingFrame` | `RecyclingGrid : public brls::ScrollingFrame` |
| 页面 | 继承 `brls::Activity` + `CONTENT_FROM_XML_RES` | `MainActivity`、`PlayerActivity` |
| 子页面 | 继承 `brls::Box`，被 Tab/Activity 挂载 | `Fragment`（首页/动态/我的…） |

wiliwili 里直接 `#include <yoga/YGNode.h>` 的只有 2 个文件
（`text_box.cpp`、`pgc_index_activity.cpp`）——其余 50+ 个自定义 View 全部通过
`brls::Box`/`brls::View` 的封装层间接使用 Yoga。这就是"逐层封装"的验收标准。

---

## 9. 与你 imgui_learn 项目的对照

| 关注点 | borealis + wiliwili | imgui_learn 现状 |
|---|---|---|
| 布局与绘制的关系 | Yoga 树 ⇄ View 树一一对应，绘制时读 `YGNodeLayoutGet*` | demo_04 里 Yoga 树是临时对象，画完 `YGNodeFreeRecursive` |
| 布局重算 | 按需 `invalidate()` 冒泡 + 全树重算，有 measure 回调 | 每帧重建整棵树，全量重算 |
| Node/Render 分离 | 不分离，`View` 二者合一 | 不分离，但树是"一次性"的 |
| 文本测量 | `YGNodeSetMeasureFunc` + `nvgTextBounds` | 无（ImGui 自己算） |
| 绘制入口 | `View::draw(vg, x, y, w, h, style, ctx)`，父链提供坐标系 | `ImDrawList::AddXxx(绝对坐标)` |
| 容器递归 | `Box::draw()` 递归 + 多层剔除 + scissor 裁剪 | demo_04 手动算 3 张卡片坐标 |
| 样式 | `Style`/`Theme` + XML 属性 | 硬编码颜色常量 |
| 动画 | `core/animation.hpp` 时间轴补间 | demo_05 用 Tweeny |

如果想把 imgui_learn 往这个架构靠，最小增量是三步（不需要抄全部）：

1. 建 `View` 基类持有 `YGNode*`，构造时 `YGNodeSetContext(node, this)`，
   提供 `setWidth/setGrow/setMargin…` 透传 + `virtual void draw(ImDrawList*, Rect) = 0`。
2. 建 `Box : View`，`addView` 里 `YGNodeInsertChild`，`draw` 里递归子节点。
3. 根部每帧（或变更时）`YGNodeCalculateLayout(root, W, H, LTR)`，把 `W/H` 换成
   上一节说的主画布尺寸（`GetMainViewport()->WorkSize`）。

相对 borealis 需要砍掉的部分：nanovg（换 ImDrawList）、input/focus/gesture（ImGui 自带
hit-test 与导航）、XML/theme/style（学习项目用不上）。

---

## 附：关键文件索引（相对于 `library/borealis`）

| 文件 | 行数 | 职责 |
|---|---|---|
| `library/include/borealis/core/view.hpp` | 1559 | `View` 声明，Yoga 样式 API 透传、XML 属性注册宏 |
| `library/lib/core/view.cpp` | 2590 | `View` 实现：`frame()` 绘制模板、`invalidate()`、`getX/Y()` |
| `library/lib/core/box.cpp` | 775 | 容器：子节点增删（同步 `YGNodeInsertChild`）、绘制递归、剔除、焦点导航 |
| `library/lib/core/application.cpp` | 1340 | 主循环、活动栈、`frame()` 帧管线、Yoga 全局事件订阅、窗口缩放 |
| `library/lib/core/activity.cpp` | 188 | 页面基类、`resizeToFitWindow()` |
| `library/lib/views/label.cpp` | 681 | 文本 measure 函数 + 截断/换行/跑马灯的完整示例 |
| `library/include/borealis/core/frame_context.hpp` | — | `{ vg, pixelRatio, fontStash, theme }` |
| `library/lib/core/style.cpp` | 435 | 样式表（`brls/label/default_font_size` 等） |
| `library/lib/core/theme.cpp` | 193 | 主题色板 |
