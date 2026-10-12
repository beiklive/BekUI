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
//       { "op": "text", "id": "title", "source": "text",
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
#include <string>
#include <vector>

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

// 一个绘制层。op 决定用到哪些字段。
struct AppearanceLayer
{
    enum class Op
    {
        RECT,   // fill / stroke / strokeWidth / radius
        TEXT,   // source(数据名) / color / fontSize / align / valign / inset
        IMAGE,  // source(数据名) / scaling / radius / inset
        SHADOW, // 阴影：fill(色) / feather(羽化) / spread(外扩) / radius / offset(x,y) / cutout(挖空内圈)
        CUSTOM, // 交给控件自己画（语义性内容：文本排版、图片缩放、进度弧、滑块轨道…）
    };

    Op op = Op::RECT;
    std::string id;
    std::string source; // Text/Image/Custom 的数据来源名

    // 初值（编译时已把 @theme/... 解析成具体值）
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
    // 显式宽高（0 表示用 inset 收缩后的整个范围）。
    // 装饰件要用：色条（width 4）、下边线（height 1）。给定后按 align/valign 摆放。
    float width       = 0.0f;
    float height      = 0.0f;
    std::string align     = "left";
    std::string valign    = "center";
    std::string scaling   = "fit";
};

// 状态切换时对某个属性的目标描述。property 写成 "<layerId>.<属性名>"。
struct AppearanceTransition
{
    std::string layerId;
    AppearanceProp prop      = AppearanceProp::FILL;
    float target[4]          = { 0.0f, 0.0f, 0.0f, 0.0f }; // 颜色用 4 个通道，标量用 [0]
    float durationMs         = 0.0f;
    std::string easing       = "linear";
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

class Appearance
{
  public:
    // 编译一份外观 JSON。失败返回 nullptr，error 里是人话（含出错的 key 路径）。
    static std::shared_ptr<const Appearance> compile(const std::string& json, const ThemeData& theme,
        std::string& error);

    const std::vector<AppearanceLayer>& getLayers() const { return this->layers; }
    const AppearanceBox& getBox() const { return this->box; }
    const std::vector<std::string>& getStateNames() const { return this->stateNames; }
    const std::vector<AppearanceTransition>& getTransitions(const std::string& state) const;

    // 层索引；找不到返回 -1
    int indexOfLayer(const std::string& layerId) const;

  private:
    std::vector<AppearanceLayer> layers;
    AppearanceBox box;
    std::vector<std::string> stateNames;
    std::vector<std::pair<std::string, std::vector<AppearanceTransition>>> states;
};

// 当前主题下某个控件的外观（带缓存）。主题里没有该文件时返回 nullptr。
std::shared_ptr<const Appearance> getWidgetAppearance(const std::string& widget);

// 把外观里的盒模型段套到一个 Box 上（AppearanceView 在 applyTheme 里自动调；
// 容器类控件——Sidebar / AppletFrame 这种不是 AppearanceView 的——也能自己调）。
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

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace bklv
