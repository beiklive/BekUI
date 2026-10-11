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

class ThemeData;

// 可插值的属性。颜色占 4 个通道，标量占 1 个。
enum class AppearanceProp
{
    Fill,
    Stroke,
    TextColor,
    StrokeWidth,
    Radius,
    Opacity,
    OffsetX,
    OffsetY,
    InsetTop,
    InsetRight,
    InsetBottom,
    InsetLeft,
    FontSize,
};

// 属性名（JSON 里那种小驼峰）与枚举互转；名字不认识时返回 false。
bool appearancePropFromName(const std::string& name, AppearanceProp& out);
bool appearancePropIsColor(AppearanceProp prop);

// 一个绘制层。op 决定用到哪些字段。
struct AppearanceLayer
{
    enum class Op
    {
        Rect,   // fill / stroke / strokeWidth / radius
        Text,   // source(数据名) / color / fontSize / align / valign / inset
        Image,  // source(数据名) / scaling / radius / inset
        Custom, // 交给控件自己画（语义性内容：文本排版、图片缩放、进度弧、滑块轨道…）
    };

    Op op = Op::Rect;
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
    AppearanceProp prop      = AppearanceProp::Fill;
    float target[4]          = { 0.0f, 0.0f, 0.0f, 0.0f }; // 颜色用 4 个通道，标量用 [0]
    float durationMs         = 0.0f;
    std::string easing       = "linear";
};

class Appearance
{
  public:
    // 编译一份外观 JSON。失败返回 nullptr，error 里是人话（含出错的 key 路径）。
    static std::shared_ptr<const Appearance> compile(const std::string& json, const ThemeData& theme,
        std::string& error);

    const std::vector<AppearanceLayer>& getLayers() const { return this->layers; }
    const std::vector<std::string>& getStateNames() const { return this->stateNames; }
    const std::vector<AppearanceTransition>& getTransitions(const std::string& state) const;

    // 层索引；找不到返回 -1
    int indexOfLayer(const std::string& layerId) const;

  private:
    std::vector<AppearanceLayer> layers;
    std::vector<std::string> stateNames;
    std::vector<std::pair<std::string, std::vector<AppearanceTransition>>> states;
};

// 当前主题下某个控件的外观（带缓存）。主题里没有该文件时返回 nullptr。
std::shared_ptr<const Appearance> getWidgetAppearance(const std::string& widget);

// 换主题后清掉外观缓存（setTheme 会调）。
void clearAppearanceCache();

// 运行时换肤：重新装载主题 + 清外观缓存。
// 调用方（App::setTheme）还要遍历视图树重新套用外观并重排。
// 返回 false 表示主题没装载成功（保持当前主题）。
bool setTheme(const std::string& name);

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
