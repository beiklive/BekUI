#pragma once

// 外观描述：控件的绘制结构与状态动效完全由 JSON 描述。
//
// 一份外观（`resources/theme/<theme>/widgets/<name>.json`）长这样：
//
//   {
//     "layers": [
//       { "op": "rect", "id": "bg", "radius": "@theme/metric.radius.md",
//         "fill": "@theme/color.surface.raised",
//         "stroke": "@theme/color.border.default", "strokeWidth": 0 },
//       { "op": "text", "id": "title", "data_id": "text",
//         "color": "@theme/color.text.primary",
//         "fontSize": "@theme/metric.font.size.md",
//         "align": "center", "valign": "center", "inset": [0, 12, 0, 12] }
//     ],
//     "states": {
//       "focused": {
//         "bg.fill":     { "to": "@theme/color.state.focus", "duration": 120, "easing": "quadraticOut" },
//         "title.color": { "to": "@theme/color.text.onAccent", "duration": 120 }
//       },
//       "pressed":  { "bg.strokeWidth": { "to": 2, "duration": 60 } },
//       "disabled": { "bg.fill": { "to": "@theme/color.state.disabled", "duration": 150 } }
//     }
//   }
//
// 分层：
//   Appearance          编译结果，不可变、可共享、按（主题, 控件名）缓存
//   AppearanceInstance  每个 View 一份：各属性当前值 + 正在跑的补间
//
// 补间用 tweeny：每个属性/通道一个 tween（颜色 4 个通道），状态变化时从**当前值**
// 补到目标值——所以连点两下按钮不会跳变。tweeny 藏在 .cpp 里（pimpl），
// 不让它污染这个头文件。

#include <memory>
#include <array>
#include <string>
#include <vector>

#include "bklv/core/gradient.hpp"

#include "imgui.h"

namespace bklv
{

class Box;

class ThemeData;

// 可插值的属性。颜色占 4 个通道，标量占 1 个。
enum class AppearanceProp
{
    FILL,
    STROKE,
    TEXT_COLOR,
    STROKE_WIDTH,
    RADIUS,
    OPACITY,
    OFFSET_X,
    OFFSET_Y,
    FEATHER,
    SPREAD,
    INSET_TOP,
    INSET_RIGHT,
    INSET_BOTTOM,
    INSET_LEFT,
    FONT_SIZE,
};

// 属性名（JSON 里那种小驼峰）与枚举互转；名字不认识时返回 false。
bool appearancePropFromName(const std::string& name, AppearanceProp& out);
bool appearancePropIsColor(AppearanceProp prop);

// ---------------------------------------------------------------------------
// 图层参数（扩展层的"自己的字段"）
// ---------------------------------------------------------------------------
//
// 背景：内置 6 种层的字段是**引擎级**的（要参与几何计算与补间），所以它们是
// `AppearanceLayer` 上的具名成员。第三方注册的新层没有具名成员可用 —— 之前只能
// 借引擎字段放自己的参数（`demo/contrib/stripe_layer.cpp` 把角度存进 feather、
// 线宽存进 spread），既容易撞车也读不懂。
//
// 现在注册时声明一份**参数模式**（`compile::LayerParam`），引擎按它自动解析 / 缩放 / 校验，
// 值落在这里的参数槽里。加一个层自己的参数不需要改引擎任何代码。

// 参数取值类型（决定怎么解析、要不要按 720p 缩放、设计器给什么控件）
//
// 没有 STRING：参数槽是定长 POD（`LayerValue` 只有数值/颜色/布尔），塞 std::string
// 会让每一层都带一次堆分配（layer 是热路径数据）。需要"模式"这类字符串语义时声明成
// ENUM —— 解析后存的是**候选值下标**，绘制钩子用 `enumParam(ctx, "名字")` 取回字符串。
enum class LayerParamType
{
    NUMBER, // 数字（不缩放：系数、角度、比例这类）
    PIXELS, // 像素量（乘 uiScale，和主题 metric 一致）
    COLOR,  // "#RRGGBB" 或 "@theme/color.a.b"
    BOOL,   // true / false
    ENUM,   // 候选取值之一（存下标；拼错只警告并保持默认值）
};

// 一个参数槽的当前值。哪个成员有意义由参数模式里的 type 决定
// （定长结构 + 成员常驻，换来热路径零分配、零变体访问开销）。
struct LayerValue
{
    float number = 0.0f;  // NUMBER / PIXELS；ENUM 时是 enumValues 的下标
    ImU32 color  = 0;     // COLOR
    bool boolean = false; // BOOL
};

// 每层参数上限：定长存放。超了在**注册期**报错（不是运行期静默丢弃）。
constexpr size_t kMaxLayerParams = 8;

// 一个绘制层。op 决定用到哪些字段。
struct AppearanceLayer
{
    enum class Op
    {
        RECT,   // fill / stroke / strokeWidth / radius / gradient（渐变优先于 fill）
        TEXT,   // source(数据名) / color / fontSize / align / valign / inset
        IMAGE,  // source(数据名) / scaling / radius / inset
        SHADOW, // 阴影：fill(色) / feather(羽化) / spread(外扩) / radius / offset(x,y) / cutout(挖空内圈)
        GLASS,  // 液态玻璃（外观层声明"这块要玻璃"）。
                // **必须排在自己的内容层之前** —— 玻璃画在下面，内容压在上面。
                // 可覆盖的样式：fill(染色，带 alpha) / opacity / radius(圆角) / inset 几何；
                // 帧级参数（blurRadius / quality）与总开关仍只由 Shell 决定（两条后端共用一份背景模糊）。
        CUSTOM, // 交给控件自己画（语义性内容：文本排版、图片缩放、进度弧、滑块轨道…）
        // 注册表里注册的扩展 op（第三方在 bklv 之外加的）；名字记在 extensionType
        EXTENSION,
    };

    Op op = Op::RECT;
    std::string extensionType; // op == EXTENSION 时：注册表里的类型名（\"stripe\"）
    std::string id;
    std::string dataId; // Text/Image/Custom 的数据来源：内容槽 id（JSON 键 data_id）
    // 字体种类声明（JSON 键 font_kind）："icon" 用图标字体、"text" 用正文字体。
    // 空 = 正文字体（不再按层 id 推断）。
    std::string fontKind;
    // 图片来源（可选）：相对主题根，如 "img/switch/switch_on.svg"；
    // 解析期会补成 "resources/theme/<主题>/<src>"。给了 src 就不需要内容槽。
    std::string src;

    // 初值（编译时已把 @theme/... 解析成具体值）
    // RECT 层的渐变（`gradient` 对象）。写了就以它为准，没写用上面的 fill 单色。
    // 停在层里定长存放（不进 ALL_PROPS —— 渐变不是标量通道，不做补间）。
    Gradient gradient;
    ImU32 fill        = 0;
    ImU32 stroke      = 0;
    ImU32 textColor   = 0;
    float strokeWidth = 0.0f;
    float radius      = 0.0f;
    float opacity     = 1.0f;
    float fontSize    = 0.0f;
    float inset[4]    = { 0.0f, 0.0f, 0.0f, 0.0f }; // top / right / bottom / left
    float offset[2]   = { 0.0f, 0.0f };
    // SHADOW 层：羽化半径与外扩量（都按 720p 缩放）
    float feather     = 0.0f;
    float spread      = 0.0f;
    // SHADOW 层：挖掉控件自身矩形，只画外圈（borealis 的 NVG_HOLE 做法）。
    // 关掉时阴影是一整块（垫在控件下面），如果底色是半透明的会透出阴影；
    // 打开后内圈不画，后面加的层即使是透明色也不会叠出阴影。
    bool cutout       = false;
    // SHADOW 层：画**内阴影**（贴着内缘最暗、往内渐透的一圈带子）而不是外投影。
    // **没有方向**：`offset` 对内阴影无效（它不是投影，是内侧渐变）；
    // 它也不吃 `cutout`（本来就是空心的）—— 带子始终待在形状内部。
    bool inner        = false;
    // 显式宽高（0 表示用 inset 收缩后的整个范围）。
    // 装饰件要用：色条（width 4）、下边线（height 1）。给定后按 align/valign 摆放。
    float width       = 0.0f;
    float height      = 0.0f;
    std::string align     = "left";
    std::string valign    = "center";
    std::string scaling   = "fit";

    // 扩展层的参数槽：下标与注册时声明的参数模式（compile::LayerType::params）**一一对应**。
    // 绘制钩子按 `LayerDrawContext::type->params[i].name` 取名字、按 `layer.params[i]` 取值。
    // 内置 6 种不用它（它们有上面的具名成员）。
    std::array<LayerValue, kMaxLayerParams> params{};
    uint8_t paramCount = 0;
};

// 状态切换时对某个属性的目标描述。property 写成 "<layerId>.<属性名>"。
struct AppearanceTransition
{
    std::string layerId;
    AppearanceProp prop      = AppearanceProp::FILL;
    float target[4]          = { 0.0f, 0.0f, 0.0f, 0.0f }; // 颜色用 4 个通道，标量用 [0]
    float durationMs         = 0.0f;
    std::string easing       = "linear";
    // 过渡（transitions）比 states 多两个字段：显式起点与延迟。
    // states 路径不用它们（hasSource=false、delayMs=0），所以行为完全不变。
    float source[4]          = { 0.0f, 0.0f, 0.0f, 0.0f };
    bool hasSource           = false;
    float delayMs            = 0.0f;
};

// ---- 视图过渡（外观 JSON 的顶层 `transitions` 段）----
//
// 和 `states` 的分工：states 管"控件内部状态"（选中/聚焦/按下…，按层的标量通道补间），
// transitions 管"视图生命周期"（出现 / 消失），并且多一类**视图级**通道 ——
// 整棵子树一起动的绘制期变换（位移 / 缩放 / 整体透明度），零重排。
//
// **不做预设**：没有 `"type": "slideFade"` 这种枚举，效果直接写通道名：
//
//   "transitions": {
//     "enter": {
//       "duration": 200, "easing": "quadraticOut",
//       "translateX": { "from": -60, "to": 0 },
//       "alpha":      { "from": 0,   "to": 1 },
//       "layers": {
//         "title": { "when": "after", "offsetX": { "from": -20, "to": 0 } }
//       }
//     },
//     "exit": { ... }, "show": { ... }, "hide": { ... }
//   }
//
// 四个事件、两条规则：
//   · 外观里**没写** transitions → 调用方用默认的**淡入淡出**（时长取主题 metric.anim.normal）
//   · 事件里**没写**某个层 → 那一层不动，跟着整树一起淡
//   · `duration: 0` = 瞬变（想显式关掉动画就用它）
enum class TransitionEvent
{
    ENTER,
    EXIT,
    SHOW,
    HIDE,
    COUNT,
};

const char* transitionEventName(TransitionEvent event);
bool transitionEventFromName(const std::string& name, TransitionEvent& out);

// 视图级通道：作用于整棵子树（绘制期变换）
enum class ViewChannel
{
    TRANSLATE_X,
    TRANSLATE_Y,
    SCALE_X,
    SCALE_Y,
    ALPHA,
    ROTATE, // 弧度（不缩放）
    SKEW_X, // 弧度（不缩放）
    SKEW_Y,
    COUNT,
};

const char* viewChannelName(ViewChannel channel);
bool viewChannelFromName(const std::string& name, ViewChannel& out);
// 位移按 720p 缩放；缩放倍数与透明度是系数，不缩放
bool viewChannelIsScaled(ViewChannel channel);

// 一条视图级通道动画
struct ViewChannelClip
{
    ViewChannel channel = ViewChannel::ALPHA;
    float source        = 0.0f;
    float target        = 0.0f;
    bool hasSource      = false; // 没写 from 就用视图当前值
    float delayMs       = 0.0f;
    float durationMs    = 0.0f;
    std::string easing  = "quadraticOut";
};

// 一个事件的完整描述。解析期就把时间算好：
//   · `when: "after"` 的层内条目，delay 已经加上视图级总时长
//   · `totalMs` = max(事件时长, 所有条目 delay + duration)
//     调用方（Shell / Pager）拿 totalMs 决定"这段动画播完了"的时机 ——
//     出场播完才摘页/摘弹层，报早了动画会被砍。
struct TransitionEventSpec
{
    bool defined       = false;
    float durationMs   = 0.0f; // 事件默认时长（条目没写 duration 时用它）
    float delayMs      = 0.0f; // 事件默认延迟
    std::string easing = "quadraticOut";
    float viewTotalMs  = 0.0f; // 视图级动画的名义总时长（when:after 的基准）
    float totalMs      = 0.0f;
    std::vector<ViewChannelClip> viewClips;
    std::vector<AppearanceTransition> layerClips;
};

// 控件的盒模型/布局样式：和 layers 一样写在控件外观 JSON 里（顶层 `"box"` 段）。
//
//   "box": { "height": "@theme/metric.button.height", "minWidth": 120,
//            "padding": [15, 25, 15, 25], "gap": 8, "grow": 1,
//            "justifyContent": "center", "alignItems": "center",
//            "backgroundColor": "@theme/color.surface.card", "cornerRadius": 8 }
//
// 数值按 720p 缩放（百分比字符串按百分比处理，"auto" 表示由内容决定）。
// 只有写了的字段才会被套用，所以控件自己的默认值不会被空 box 冲掉。
struct AppearanceBox
{
    bool hasWidth = false, hasHeight = false, hasMinWidth = false, hasMinHeight = false;
    bool hasPadding = false, hasMargin = false, hasGap = false;
    bool hasGrow = false, hasShrink = false;
    bool hasJustify = false, hasAlign = false, hasAxis = false;
    bool hasBackground = false, hasCornerRadius = false;
    // 固有尺寸测量方式：默认取"最宽的那一层"（max）；组合外观（一行里放图标+文字+信息）
    // 要按行**求和**，否则量出来只有最宽的一层，文字会溢出控件（写 measureContent: "sum"）
    bool measureSum = false;
    bool hasBorderThickness = false, hasBorderColor = false;

    float width = 0.0f, height = 0.0f, minWidth = 0.0f, minHeight = 0.0f;
    bool widthPercent = false, heightPercent = false;
    bool widthAuto = false, heightAuto = false;
    float padding[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // top / right / bottom / left
    float margin[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
    float gap        = 0.0f;
    float grow = 0.0f, shrink = 1.0f;
    std::string justify;
    std::string align;
    std::string axis; // row / column
    ImU32 background   = 0;
    float cornerRadius = 0.0f;
    float borderThickness = 0.0f;
    ImU32 borderColor     = 0;
};

// 外观里的**子节点声明**（`children` 段）。
// 注意：这是编译产物里的纯数据 —— 建视图是 widgets 的事，解析不建视图。
struct AppearanceChild
{
    enum class Kind
    {
        APPEARANCE,    // "appearance": "<外观名>" -> 一个 AppearanceView
        BOX,           // 只写 "box" -> 一个纯布局 Box（递归嵌套的载体）
        VIEW_TEMPLATE, // "ref": "<视图模板名>" -> 用主题 view/ 的模板实例化（任意控件类型）
    };

    Kind kind = Kind::BOX;
    std::string name;               // appearance 名 或 视图模板名（BOX 时为空）
    AppearanceBox box;              // 写了的字段才覆盖（沿用"没写不动"的既有约定）
    std::string text, icon, info;   // 内容槽（可空；实例化后套到 AppearanceView 的既有槽位）
    std::string id;                 // 可选：子视图 id（findViewById 用）
    std::vector<AppearanceChild> children; // 递归
};
class Appearance
{
  public:
    // 编译一份外观 JSON。失败返回 nullptr，error 里是人话（含出错的 key 路径）。
    // source 只用于**诊断**（JSON 错误策略里"是哪个文件坏了"），不影响解析。
    static std::shared_ptr<const Appearance> compile(const std::string& json, const ThemeData& theme,
        std::string& error, const std::string& source = std::string());

    // 编译 + 按错误策略回退：policy == FALLBACK_DEFAULT 且编译失败时，用 **default 主题**
    // 里的同名外观（widget，如 "base/widget/button.text"）再编一次。
    // 注意回退的是"外观定义"，`@theme` 引用仍按**当前主题**解析 —— 否则那一块会突然变成
    // default 的配色，和旁边对不上。
    static std::shared_ptr<const Appearance> compileWithFallback(const std::string& json,
        const std::string& widget, const ThemeData& theme, std::string& error);

    const std::vector<AppearanceLayer>& getLayers() const { return this->layers; }
    const std::vector<AppearanceChild>& getChildren() const { return this->children; }
    const AppearanceBox& getBox() const { return this->box; }
    const std::vector<std::string>& getStateNames() const { return this->stateNames; }
    const std::vector<AppearanceTransition>& getTransitions(const std::string& state) const;

    // 状态里的**视图级**通道（`states.<状态>.self`）：整棵子树一起动的绘制期变换。
    const std::vector<ViewChannelClip>& getStateViewClips(const std::string& state) const;

    // 视图过渡：外观里有没有写 transitions / 取某个事件的描述
    bool hasTransitions() const;
    const TransitionEventSpec& getTransition(TransitionEvent event) const;
    TransitionEventSpec& transitionsFor(TransitionEvent event);

    // 层索引；找不到返回 -1
    int indexOfLayer(const std::string& layerId) const;

    // 外观里写的事件音效覆盖（states.click.sound）；没有返回空
    std::string getSoundOverride(const std::string& trigger) const;
    void addSoundOverride(const std::string& trigger, const std::string& path);

    // ---- 外观契约 ----
    // 代码声明"这份外观必须有哪些内容槽/几何层"，校验在编译后做。
    struct Contract
    {
        std::vector<std::string> content;             // data_id，op 必须是 text
        std::vector<std::pair<std::string, int>> layers; // layer id + 期望 op（AppearanceLayer::Op 的整数值）
    };
    static void registerContract(const std::string& appearance, Contract contract);
    // 校验 compiled 是否满足 appearance 的契约；没有注册契约时返回 true
    static bool checkContract(const std::string& appearance, const Appearance& compiled,
        std::string& error);

  private:
    std::vector<AppearanceLayer> layers;
    std::vector<AppearanceChild> children;
    AppearanceBox box;
    std::vector<std::string> stateNames;
    std::vector<std::pair<std::string, std::vector<AppearanceTransition>>> states;
    std::vector<std::pair<std::string, std::vector<ViewChannelClip>>> stateViewClips;
    std::vector<std::pair<std::string, std::string>> soundOverrides;
    std::vector<std::string> requiredContent;
    std::vector<std::string> requiredLayers;
    // 四个生命周期事件（外观 JSON 的 transitions 段）；没写就是 defined=false
    std::array<TransitionEventSpec, (size_t)TransitionEvent::COUNT> transitions;
};

// 当前主题下某个控件的外观（带缓存）。主题里没有该文件时返回 nullptr。
std::shared_ptr<const Appearance> getWidgetAppearance(const std::string& widget);

// 把外观里的盒模型段套到一个 Box 上（AppearanceView 在 applyTheme 里自动调；
// 容器类控件——Sidebar / Shell 这种不是 AppearanceView 的——也能自己调）。
void applyAppearanceBox(Box* view, const AppearanceBox& box);

// 取 widgets/<name>.json 并套用它的 box 段；variant 非空时取 <name>.<variant>.json。
// 返回 false 表示主题里没有这份外观。
bool applyWidgetBox(Box* view, const std::string& name, const std::string& variant = "");

// 换主题后清掉外观缓存（setTheme 会调）。
void clearAppearanceCache();

// 运行时换肤：重新装载主题 + 清外观缓存。
// 调用方（App::setTheme）还要遍历视图树重新套用外观并重排。
// 返回 false 表示主题没装载成功（保持当前主题）。
bool setTheme(const std::string& name);

// 把当前主题**登记过的每一份外观**都编译一遍，失败返回 false 并给出第一个错误。
// 保存/重载前跑它：避免把一份写坏的外观应用上去，界面变空白还查不出原因。
bool validateThemeAppearances(std::string& error);

// 可插值的属性名清单（fill / opacity / radius / ...）。
// 编辑器做"给状态加通道"的下拉直接用这个，别在外面再抄一份。
const std::vector<std::string>& appearancePropNames();

// ---------------------------------------------------------------------------
// 运行期状态（每个 View 一份）
// ---------------------------------------------------------------------------

class AppearanceInstance
{
  public:
    AppearanceInstance();
    ~AppearanceInstance();
    AppearanceInstance(AppearanceInstance&&) noexcept;
    AppearanceInstance& operator=(AppearanceInstance&&) noexcept;
    AppearanceInstance(const AppearanceInstance&) = delete;
    AppearanceInstance& operator=(const AppearanceInstance&) = delete;

    // 绑定一份外观：抄初值进来、清掉补间。重复绑定同一份是空操作。
    void setAppearance(std::shared_ptr<const Appearance> appearance);

    const Appearance& getAppearance() const;

    // 打开/关闭一个状态。会重算目标值，对变化了的属性从当前值启动补间。
    void setState(const std::string& state, bool active);
    bool isStateActive(const std::string& state) const;
    void clearStates();

    // 每帧推进补间（dtSeconds 是帧间隔）。
    void update(float dtSeconds);
    bool isAnimating() const;

    // 当前值。layer 越界时返回兜底值。
    ImU32 getColor(int layer, AppearanceProp prop) const;
    float getNumber(int layer, AppearanceProp prop) const;

    // ---- 视图过渡（transitions）用 ----
    // 直接写某条通道的当前值。状态动效和过渡不能同时对同一条通道下手，
    // 所以过渡开始时调用方先 cancelTween() 接管这条通道。
    void setNumber(int layer, AppearanceProp prop, float value);
    void setColor(int layer, AppearanceProp prop, const float rgba[4]);
    void cancelTween(int layer, AppearanceProp prop);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace bklv
