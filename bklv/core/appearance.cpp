#include "bklv/core/appearance.hpp"
#include "bklv/core/box.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include <tweeny/tweeny.h>

#include "bklv/core/logger.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/ui_scale.hpp"

namespace bklv
{

namespace
{

// 外观 JSON 里的像素量（圆角/描边/显式宽高/字号/内边距/位移）要跟随 720p 缩放；
// opacity 是系数、color 是颜色、duration 是毫秒，都不缩放。
float scalePixels(float value)
{
    return value * getUiScale();
}

// 主题引用里的 key 归一：`color/a.b` 与 `color.a.b` 都当成 `color.a.b`。
//
// 分组名（color / metric）后面应该是点，写成斜杠的人不少（我自己就反复写错），
// 所以这里容错，但会警告一句——**真正不存在的 key 仍然硬失败**，
// 所以容错不会退回"静默变洋红"。
std::string normalizeThemeKey(const std::string& key, const std::string& where)
{
    const size_t slash = key.find('/');
    if (slash == std::string::npos)
        return key;

    std::string normalized = key;
    normalized[slash]      = '.';

    BKLV_LOG_WARNING("主题引用写法应带点：@theme/%s（%s）；已按 @theme/%s 解析", key.c_str(),
        where.c_str(), normalized.c_str());
    return normalized;
}


using Json = nlohmann::json;

constexpr int PROP_COUNT = 13;

int propIndex(AppearanceProp prop)
{
    return (int)prop;
}

// 属性名 -> 枚举。JSON 里用小驼峰，和 View 的属性表命名保持一致。
struct PropName
{
    const char* name;
    AppearanceProp prop;
};

// 全部可插值属性，每个枚举值一条（没有别名，遍历时不用去重）
const AppearanceProp ALL_PROPS[] = {
    AppearanceProp::Fill,
    AppearanceProp::Stroke,
    AppearanceProp::TextColor,
    AppearanceProp::StrokeWidth,
    AppearanceProp::Radius,
    AppearanceProp::Opacity,
    AppearanceProp::OffsetX,
    AppearanceProp::OffsetY,
    AppearanceProp::InsetTop,
    AppearanceProp::InsetRight,
    AppearanceProp::InsetBottom,
    AppearanceProp::InsetLeft,
    AppearanceProp::FontSize,
};

const PropName PROP_NAMES[] = {
    { "fill", AppearanceProp::Fill },
    { "stroke", AppearanceProp::Stroke },
    { "color", AppearanceProp::TextColor },
    { "textColor", AppearanceProp::TextColor },
    { "strokeWidth", AppearanceProp::StrokeWidth },
    { "radius", AppearanceProp::Radius },
    { "opacity", AppearanceProp::Opacity },
    { "offsetX", AppearanceProp::OffsetX },
    { "offsetY", AppearanceProp::OffsetY },
    { "insetTop", AppearanceProp::InsetTop },
    { "insetRight", AppearanceProp::InsetRight },
    { "insetBottom", AppearanceProp::InsetBottom },
    { "insetLeft", AppearanceProp::InsetLeft },
    { "fontSize", AppearanceProp::FontSize },
};

bool parseHex(const std::string& text, ImU32& out)
{
    if (text.size() != 7 && text.size() != 9)
        return false;
    if (text[0] != '#')
        return false;
    for (size_t i = 1; i < text.size(); i++)
        if (!std::isxdigit((unsigned char)text[i]))
            return false;

    const unsigned long value = std::strtoul(text.c_str() + 1, nullptr, 16);
    if (text.size() == 7)
        out = IM_COL32((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, 255);
    else
        out = IM_COL32((value >> 24) & 0xFF, (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
    return true;
}

void splitColor(ImU32 color, float out[4])
{
    out[0] = (float)(color & 0xFF);
    out[1] = (float)((color >> 8) & 0xFF);
    out[2] = (float)((color >> 16) & 0xFF);
    out[3] = (float)((color >> 24) & 0xFF);
}

ImU32 joinColor(const float in[4])
{
    auto clamp255 = [](float v) {
        return (unsigned int)std::max(0.0f, std::min(255.0f, std::round(v)));
    };
    return IM_COL32(clamp255(in[0]), clamp255(in[1]), clamp255(in[2]), clamp255(in[3]));
}

// 颜色项：`"#RRGGBB"` 或 `"@theme/color.xxx"`
bool resolveColor(const Json& node, const ThemeData& theme, ImU32& out, std::string& error,
    const std::string& where)
{
    if (!node.is_string())
    {
        error = where + " 期望颜色字符串（\"#RRGGBB\" 或 \"@theme/color.x.y\"）";
        return false;
    }

    const std::string text = node.get<std::string>();
    if (text.rfind("@theme/", 0) == 0)
    {
        const std::string key = normalizeThemeKey(text.substr(7), where);
        if (!theme.hasColor(key))
        {
            // 不静默回退：写错 key 会变成一块洋红，比报错难查得多
            error = where + " 引用了主题里不存在的颜色：@theme/" + key
                + "（当前主题 " + theme.getName() + "；写法是 @theme/color.a.b）";
            return false;
        }
        out = theme.color(key);
        return true;
    }
    if (parseHex(text, out))
        return true;

    error = where + " 不是合法颜色：" + text;
    return false;
}

// 数值项：数字 或 `"@theme/metric.x.y"`
bool resolveNumber(const Json& node, const ThemeData& theme, float& out, std::string& error,
    const std::string& where)
{
    if (node.is_number())
    {
        out = node.get<float>();
        return true;
    }
    if (node.is_string())
    {
        const std::string text = node.get<std::string>();
        if (text.rfind("@theme/", 0) == 0)
        {
            const std::string key = normalizeThemeKey(text.substr(7), where);
            if (!theme.hasNumber(key))
            {
                error = where + " 引用了主题里不存在的数值：@theme/" + key
                    + "（当前主题 " + theme.getName() + "；写法是 @theme/metric.a.b）";
                return false;
            }
            out = theme.number(key);
            return true;
        }
        char* end = nullptr;
        const double value = std::strtod(text.c_str(), &end);
        if (end != text.c_str() && *end == '\0')
        {
            out = (float)value;
            return true;
        }
    }

    error = where + " 期望数值（数字或 \"@theme/metric.x.y\"）";
    return false;
}

AppearanceLayer::Op parseOp(const std::string& text, bool& ok)
{
    ok = true;
    if (text == "rect")
        return AppearanceLayer::Op::Rect;
    if (text == "text")
        return AppearanceLayer::Op::Text;
    if (text == "image")
        return AppearanceLayer::Op::Image;
    if (text == "custom")
        return AppearanceLayer::Op::Custom;
    ok = false;
    return AppearanceLayer::Op::Rect;
}

// 状态优先级：同一属性被多个状态同时管着时，优先级高的说了算。
int priorityOf(const std::string& state)
{
    if (state == "disabled")
        return 0;
    if (state == "focused")
        return 1;
    if (state == "hover")
        return 2;
    if (state == "pressed")
        return 3;
    if (state == "selected")
        return 4;
    return 1;
}

// 按名字建补间。tweeny 的 easing 对象是 inline constexpr 的具名对象，
// 不方便存进容器再赋值，所以这里直接 switch —— 名字不认识时退回 linear。
tweeny::tween<float> makeTween(float from, float to, uint32_t frames, const std::string& easing)
{
    using namespace tweeny::easing;

    if (easing == "quadraticIn")
        return tweeny::from(from).to(to).via(quadraticIn).during(frames).build();
    if (easing == "quadraticOut")
        return tweeny::from(from).to(to).via(quadraticOut).during(frames).build();
    if (easing == "quadraticInOut")
        return tweeny::from(from).to(to).via(quadraticInOut).during(frames).build();
    if (easing == "cubicIn")
        return tweeny::from(from).to(to).via(cubicIn).during(frames).build();
    if (easing == "cubicOut")
        return tweeny::from(from).to(to).via(cubicOut).during(frames).build();
    if (easing == "cubicInOut")
        return tweeny::from(from).to(to).via(cubicInOut).during(frames).build();
    if (easing == "quarticOut")
        return tweeny::from(from).to(to).via(quarticOut).during(frames).build();
    if (easing == "quinticOut")
        return tweeny::from(from).to(to).via(quinticOut).during(frames).build();
    if (easing == "sinusoidalIn")
        return tweeny::from(from).to(to).via(sinusoidalIn).during(frames).build();
    if (easing == "sinusoidalOut")
        return tweeny::from(from).to(to).via(sinusoidalOut).during(frames).build();
    if (easing == "sinusoidalInOut")
        return tweeny::from(from).to(to).via(sinusoidalInOut).during(frames).build();
    if (easing == "exponentialOut")
        return tweeny::from(from).to(to).via(exponentialOut).during(frames).build();
    if (easing == "circularOut")
        return tweeny::from(from).to(to).via(circularOut).during(frames).build();
    if (easing == "backOut")
        return tweeny::from(from).to(to).via(backOut).during(frames).build();
    if (easing == "backInOut")
        return tweeny::from(from).to(to).via(backInOut).during(frames).build();
    if (easing == "bounceOut")
        return tweeny::from(from).to(to).via(bounceOut).during(frames).build();
    if (easing == "elasticOut")
        return tweeny::from(from).to(to).via(elasticOut).during(frames).build();
    if (easing == "stepped")
        return tweeny::from(from).to(to).via(stepped).during(frames).build();

    return tweeny::from(from).to(to).via(linear).during(frames).build();
}

// 外观缓存：key 是 "主题名/控件名"
std::unordered_map<std::string, std::shared_ptr<const Appearance>>& appearanceCache()
{
    static std::unordered_map<std::string, std::shared_ptr<const Appearance>> cache;
    return cache;
}

} // namespace

bool appearancePropFromName(const std::string& name, AppearanceProp& out)
{
    for (const PropName& entry : PROP_NAMES)
    {
        if (name == entry.name)
        {
            out = entry.prop;
            return true;
        }
    }
    return false;
}

bool appearancePropIsColor(AppearanceProp prop)
{
    return prop == AppearanceProp::Fill || prop == AppearanceProp::Stroke || prop == AppearanceProp::TextColor;
}

const std::vector<AppearanceTransition>& Appearance::getTransitions(const std::string& state) const
{
    static const std::vector<AppearanceTransition> empty;
    for (const auto& entry : this->states)
        if (entry.first == state)
            return entry.second;
    return empty;
}

int Appearance::indexOfLayer(const std::string& layerId) const
{
    for (size_t i = 0; i < this->layers.size(); i++)
        if (this->layers[i].id == layerId)
            return (int)i;
    return -1;
}

std::shared_ptr<const Appearance> Appearance::compile(const std::string& json, const ThemeData& theme,
    std::string& error)
{
    Json doc;
    try
    {
        doc = Json::parse(json);
    }
    catch (const Json::parse_error& e)
    {
        size_t line = 1;
        for (size_t i = 0; i < e.byte && i < json.size(); i++)
            if (json[i] == '\n')
                line++;
        error = "外观 JSON 语法错误（第 " + std::to_string(line) + " 行）：" + e.what();
        return nullptr;
    }

    if (!doc.is_object() || !doc.contains("layers") || !doc["layers"].is_array())
    {
        error = "外观 JSON 缺少 layers 数组";
        return nullptr;
    }

    auto appearance = std::shared_ptr<Appearance>(new Appearance());

    for (size_t i = 0; i < doc["layers"].size(); i++)
    {
        const Json& node = doc["layers"][i];
        const std::string where = "layers[" + std::to_string(i) + "]";

        if (!node.is_object())
        {
            error = where + " 必须是对象";
            return nullptr;
        }

        AppearanceLayer layer;
        // 没写 id 时用 op + 序号当 id（状态里引用它）
        const std::string opName = node.value("op", "rect");
        bool opOk               = false;
        layer.op                = parseOp(opName, opOk);
        if (!opOk)
        {
            error = where + ".op 不认识：" + opName + "（可用 rect / text / image / custom）";
            return nullptr;
        }

        const char* defaultId = layer.op == AppearanceLayer::Op::Rect      ? "rect"
                                : layer.op == AppearanceLayer::Op::Text    ? "text"
                                : layer.op == AppearanceLayer::Op::Image   ? "image"
                                                                           : "custom";
        layer.id     = node.value("id", std::string(defaultId) + std::to_string(i));
        layer.source = node.value("source", "");
        layer.align  = node.value("align", "left");
        layer.valign = node.value("valign", "center");
        layer.scaling = node.value("scaling", "fit");

        // 颜色 / 数值字段
        struct ColorField
        {
            const char* key;
            ImU32* target;
        };
        const ColorField colorFields[] = {
            { "fill", &layer.fill },
            { "stroke", &layer.stroke },
            { "color", &layer.textColor },
            { "textColor", &layer.textColor },
        };
        for (const ColorField& field : colorFields)
        {
            if (!node.contains(field.key))
                continue;
            if (!resolveColor(node[field.key], theme, *field.target, error, where + "." + field.key))
                return nullptr;
        }

        struct NumberField
        {
            const char* key;
            float* target;
            bool pixels; // 像素量要跟随 720p 缩放；opacity 是系数不缩放
        };
        const NumberField numberFields[] = {
            { "strokeWidth", &layer.strokeWidth, true },
            { "radius", &layer.radius, true },
            { "opacity", &layer.opacity, false },
            { "fontSize", &layer.fontSize, true },
            { "width", &layer.width, true },
            { "height", &layer.height, true },
        };
        for (const NumberField& field : numberFields)
        {
            if (!node.contains(field.key))
                continue;
            if (!resolveNumber(node[field.key], theme, *field.target, error, where + "." + field.key))
                return nullptr;
            if (field.pixels)
                *field.target = scalePixels(*field.target);
        }

        if (node.contains("inset") && node["inset"].is_array() && node["inset"].size() == 4)
            for (size_t k = 0; k < 4; k++)
            {
                if (!resolveNumber(node["inset"][k], theme, layer.inset[k], error,
                        where + ".inset[" + std::to_string(k) + "]"))
                    return nullptr;
                layer.inset[k] = scalePixels(layer.inset[k]);
            }

        if (node.contains("offset") && node["offset"].is_array() && node["offset"].size() == 2)
            for (size_t k = 0; k < 2; k++)
            {
                if (!resolveNumber(node["offset"][k], theme, layer.offset[k], error,
                        where + ".offset[" + std::to_string(k) + "]"))
                    return nullptr;
                layer.offset[k] = scalePixels(layer.offset[k]);
            }

        appearance->layers.push_back(layer);
    }

    // ---- 盒模型/布局（顶层 "box" 段）----
    if (doc.contains("box") && doc["box"].is_object())
    {
        const Json& boxNode = doc["box"];
        AppearanceBox& box  = appearance->box;

        // 长度值：数字=像素（按 720p 缩放）、"50%"=百分比、"auto"=由内容决定、
        // "@theme/metric.a.b"=主题数值
        auto readLength = [&](const Json& node, float& out, bool& percent, bool& autoFlag,
                              const std::string& where) -> bool {
            percent  = false;
            autoFlag = false;
            if (node.is_string())
            {
                const std::string text = node.get<std::string>();
                if (text == "auto")
                {
                    autoFlag = true;
                    return true;
                }
                if (!text.empty() && text.back() == '%')
                {
                    out      = std::strtof(text.c_str(), nullptr);
                    percent  = true;
                    return true;
                }
            }
            if (!resolveNumber(node, theme, out, error, where))
                return false;
            out = scalePixels(out);
            return true;
        };

        auto readEdges = [&](const Json& node, float* edges, const std::string& where) -> bool {
            // 单值（数字或 "@theme/metric.x" 引用）：四边同值
            if (node.is_number() || node.is_string())
            {
                float all = 0.0f;
                if (!resolveNumber(node, theme, all, error, where))
                    return false;
                all = scalePixels(all);
                for (int i = 0; i < 4; i++)
                    edges[i] = all;
                return true;
            }
            if (!node.is_array() || node.size() != 4)
            {
                error = where + " 要写成数字、@theme/ 引用或 [上, 右, 下, 左]";
                return false;
            }
            for (size_t i = 0; i < 4; i++)
            {
                if (!resolveNumber(node[i], theme, edges[i], error,
                        where + "[" + std::to_string(i) + "]"))
                    return false;
                edges[i] = scalePixels(edges[i]);
            }
            return true;
        };

        auto readNumber = [&](const char* key, float& out, bool& has, const std::string& where) {
            if (!boxNode.contains(key))
                return true;
            if (!resolveNumber(boxNode[key], theme, out, error, where + "." + key))
                return false;
            out = scalePixels(out);
            has = true;
            return true;
        };

        const std::string where = "box";
        if (boxNode.contains("width") && !readLength(boxNode["width"], box.width, box.widthPercent,
                                          box.widthAuto, where + ".width"))
            return nullptr;
        box.hasWidth = boxNode.contains("width");

        if (boxNode.contains("height") && !readLength(boxNode["height"], box.height, box.heightPercent,
                                           box.heightAuto, where + ".height"))
            return nullptr;
        box.hasHeight = boxNode.contains("height");

        if (!readNumber("minWidth", box.minWidth, box.hasMinWidth, where))
            return nullptr;
        if (!readNumber("minHeight", box.minHeight, box.hasMinHeight, where))
            return nullptr;
        if (!readNumber("gap", box.gap, box.hasGap, where))
            return nullptr;
        if (!readNumber("grow", box.grow, box.hasGrow, where))
            return nullptr;
        if (!readNumber("shrink", box.shrink, box.hasShrink, where))
            return nullptr;
        if (!readNumber("cornerRadius", box.cornerRadius, box.hasCornerRadius, where))
            return nullptr;
        if (!readNumber("borderThickness", box.borderThickness, box.hasBorderThickness, where))
            return nullptr;

        if (boxNode.contains("padding"))
        {
            if (!readEdges(boxNode["padding"], box.padding, where + ".padding"))
                return nullptr;
            box.hasPadding = true;
        }
        if (boxNode.contains("margin"))
        {
            if (!readEdges(boxNode["margin"], box.margin, where + ".margin"))
                return nullptr;
            box.hasMargin = true;
        }
        if (boxNode.contains("justifyContent") && boxNode["justifyContent"].is_string())
        {
            box.justify   = boxNode["justifyContent"].get<std::string>();
            box.hasJustify = true;
        }
        if (boxNode.contains("alignItems") && boxNode["alignItems"].is_string())
        {
            box.align   = boxNode["alignItems"].get<std::string>();
            box.hasAlign = true;
        }
        if (boxNode.contains("backgroundColor"))
        {
            if (!resolveColor(boxNode["backgroundColor"], theme, box.background, error,
                    where + ".backgroundColor"))
                return nullptr;
            box.hasBackground = true;
        }
        if (boxNode.contains("borderColor"))
        {
            if (!resolveColor(boxNode["borderColor"], theme, box.borderColor, error,
                    where + ".borderColor"))
                return nullptr;
            box.hasBorderColor = true;
        }
    }

    // 状态
    if (doc.contains("states") && doc["states"].is_object())
    {
        for (auto stateIt = doc["states"].begin(); stateIt != doc["states"].end(); ++stateIt)
        {
            const std::string stateName = stateIt.key();
            const Json& stateNode       = stateIt.value();
            if (!stateNode.is_object())
            {
                error = "states." + stateName + " 必须是对象";
                return nullptr;
            }

            std::vector<AppearanceTransition> transitions;
            for (auto propIt = stateNode.begin(); propIt != stateNode.end(); ++propIt)
            {
                // property 写成 "<layerId>.<属性名>"
                const std::string property = propIt.key();
                const size_t dot           = property.find_last_of('.');
                if (dot == std::string::npos)
                {
                    error = "states." + stateName + "." + property
                        + " 要写成 \"<层 id>.<属性名>\"，例如 \"bg.fill\"";
                    return nullptr;
                }

                const std::string layerId = property.substr(0, dot);
                const std::string propName = property.substr(dot + 1);
                const int layerIndex       = appearance->indexOfLayer(layerId);
                if (layerIndex < 0)
                {
                    error = "states." + stateName + "." + property + " 引用了不存在的层 \"" + layerId + "\"";
                    return nullptr;
                }

                AppearanceProp prop = AppearanceProp::Fill;
                if (!appearancePropFromName(propName, prop))
                {
                    error = "states." + stateName + "." + property + " 不认识的属性 \"" + propName + "\"";
                    return nullptr;
                }

                AppearanceTransition transition;
                transition.layerId = layerId;
                transition.prop    = prop;
                transition.durationMs = stateNode[property].value("duration", 0.0f);
                transition.easing     = stateNode[property].value("easing", "linear");

                const Json& target = stateNode[property];
                if (!target.is_object() || !target.contains("to"))
                {
                    error = "states." + stateName + "." + property + " 必须写成 {\"to\": ..., \"duration\": 毫秒}";
                    return nullptr;
                }

                if (appearancePropIsColor(prop))
                {
                    ImU32 color = 0;
                    if (!resolveColor(target["to"], theme, color, error, "states." + stateName + "." + property + ".to"))
                        return nullptr;
                    splitColor(color, transition.target);
                }
                else if (!resolveNumber(target["to"], theme, transition.target[0], error,
                             "states." + stateName + "." + property + ".to"))
                {
                    return nullptr;
                }
                else if (prop != AppearanceProp::Opacity)
                {
                    // 几何量（圆角/描边宽/字号/位移/内边距）的目标值也跟随 720p 缩放；
                    // 颜色与 opacity 不缩放
                    transition.target[0] = scalePixels(transition.target[0]);
                }

                transitions.push_back(transition);
            }

            appearance->stateNames.push_back(stateName);
            appearance->states.emplace_back(stateName, std::move(transitions));
        }
    }

    return appearance;
}

std::shared_ptr<const Appearance> getWidgetAppearance(const std::string& widget)
{
    const std::string key = getTheme().getName() + "/" + widget;

    auto& cache = appearanceCache();
    const auto cached = cache.find(key);
    if (cached != cache.end())
        return cached->second;

    const std::string* text = getTheme().widgetAppearanceJson(widget);
    if (text == nullptr)
    {
        cache[key] = nullptr;
        return nullptr;
    }

    std::string error;
    std::shared_ptr<const Appearance> appearance = Appearance::compile(*text, getTheme(), error);
    if (appearance == nullptr)
        BKLV_LOG_ERROR("控件外观编译失败（%s）：%s", key.c_str(), error.c_str());

    cache[key] = appearance;
    return appearance;
}

namespace
{

JustifyContent justifyFromName(const std::string& name)
{
    if (name == "center")
        return JustifyContent::CENTER;
    if (name == "flex_end" || name == "end")
        return JustifyContent::FLEX_END;
    if (name == "space_between")
        return JustifyContent::SPACE_BETWEEN;
    if (name == "space_around")
        return JustifyContent::SPACE_AROUND;
    if (name == "space_evenly")
        return JustifyContent::SPACE_EVENLY;

    return JustifyContent::FLEX_START;
}

AlignItems alignFromName(const std::string& name)
{
    if (name == "center")
        return AlignItems::CENTER;
    if (name == "flex_end" || name == "end")
        return AlignItems::FLEX_END;
    if (name == "stretch")
        return AlignItems::STRETCH;
    if (name == "baseline")
        return AlignItems::BASELINE;

    return AlignItems::FLEX_START;
}

} // namespace

void applyAppearanceBox(Box* view, const AppearanceBox& box)
{
    if (view == nullptr)
        return;

    if (box.hasWidth)
    {
        if (box.widthAuto)
            view->setWidth(AUTO);
        else if (box.widthPercent)
            view->setWidthPercentage(box.width);
        else
            view->setWidth(box.width);
    }
    if (box.hasHeight)
    {
        if (box.heightAuto)
            view->setHeight(AUTO);
        else if (box.heightPercent)
            view->setHeightPercentage(box.height);
        else
            view->setHeight(box.height);
    }

    if (box.hasMinWidth)
        view->setMinWidth(box.minWidth);
    if (box.hasMinHeight)
        view->setMinHeight(box.minHeight);
    if (box.hasPadding)
        view->setPadding(box.padding[0], box.padding[1], box.padding[2], box.padding[3]);
    if (box.hasMargin)
        view->setMargins(box.margin[0], box.margin[1], box.margin[2], box.margin[3]);
    if (box.hasGap)
        view->setGap(box.gap);
    if (box.hasGrow)
        view->setGrow(box.grow);
    if (box.hasShrink)
        view->setShrink(box.shrink);
    if (box.hasJustify)
        view->setJustifyContent(justifyFromName(box.justify));
    if (box.hasAlign)
        view->setAlignItems(alignFromName(box.align));
    if (box.hasBackground)
        view->setBackgroundColor(box.background);
    if (box.hasCornerRadius)
        view->setCornerRadius(box.cornerRadius);
    if (box.hasBorderThickness)
        view->setBorderThickness(box.borderThickness);
    if (box.hasBorderColor)
        view->setBorderColor(box.borderColor);
}

bool applyWidgetBox(Box* view, const std::string& name, const std::string& variant)
{
    const std::string full = variant.empty() ? name : name + "." + variant;
    const std::shared_ptr<const Appearance> appearance = getWidgetAppearance(full);
    if (appearance == nullptr)
        return false;

    applyAppearanceBox(view, appearance->getBox());
    return true;
}

void clearAppearanceCache()
{
    appearanceCache().clear();
}

bool setTheme(const std::string& name)
{
    if (!loadThemeIntoGlobal(name))
        return false;

    // 外观是按（主题名, 控件名）缓存的，换主题必须清掉，否则控件还在用旧外观
    clearAppearanceCache();
    return true;
}

// ---------------------------------------------------------------------------
// AppearanceInstance
// ---------------------------------------------------------------------------

struct AppearanceInstance::Impl
{
    struct Value
    {
        float channel[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    };

    struct ActiveTween
    {
        int layer   = 0;
        int prop    = 0;
        int channel = 0;
        tweeny::tween<float> tween;
    };

    std::shared_ptr<const Appearance> appearance;
    std::vector<std::vector<Value>> values; // [layer][prop]
    std::vector<std::string> activeStates;
    std::vector<ActiveTween> tweens;
};

AppearanceInstance::AppearanceInstance()
    : impl(new Impl())
{
}

AppearanceInstance::~AppearanceInstance() = default;
AppearanceInstance::AppearanceInstance(AppearanceInstance&&) noexcept = default;
AppearanceInstance& AppearanceInstance::operator=(AppearanceInstance&&) noexcept = default;

const Appearance& AppearanceInstance::getAppearance() const
{
    static const Appearance empty;
    return this->impl->appearance != nullptr ? *this->impl->appearance : empty;
}

void AppearanceInstance::setAppearance(std::shared_ptr<const Appearance> appearance)
{
    if (this->impl->appearance == appearance)
        return;

    this->impl->appearance = std::move(appearance);
    this->impl->tweens.clear();
    this->impl->activeStates.clear();
    this->impl->values.clear();

    if (this->impl->appearance == nullptr)
        return;

    // 抄初值
    for (const AppearanceLayer& layer : this->impl->appearance->getLayers())
    {
        std::vector<Impl::Value> perLayer(PROP_COUNT);
        splitColor(layer.fill, perLayer[propIndex(AppearanceProp::Fill)].channel);
        splitColor(layer.stroke, perLayer[propIndex(AppearanceProp::Stroke)].channel);
        splitColor(layer.textColor, perLayer[propIndex(AppearanceProp::TextColor)].channel);
        perLayer[propIndex(AppearanceProp::StrokeWidth)].channel[0] = layer.strokeWidth;
        perLayer[propIndex(AppearanceProp::Radius)].channel[0]      = layer.radius;
        perLayer[propIndex(AppearanceProp::Opacity)].channel[0]     = layer.opacity;
        perLayer[propIndex(AppearanceProp::OffsetX)].channel[0]     = layer.offset[0];
        perLayer[propIndex(AppearanceProp::OffsetY)].channel[0]     = layer.offset[1];
        perLayer[propIndex(AppearanceProp::InsetTop)].channel[0]    = layer.inset[0];
        perLayer[propIndex(AppearanceProp::InsetRight)].channel[0]  = layer.inset[1];
        perLayer[propIndex(AppearanceProp::InsetBottom)].channel[0] = layer.inset[2];
        perLayer[propIndex(AppearanceProp::InsetLeft)].channel[0]   = layer.inset[3];
        perLayer[propIndex(AppearanceProp::FontSize)].channel[0]    = layer.fontSize;
        this->impl->values.push_back(std::move(perLayer));
    }
}

bool AppearanceInstance::isStateActive(const std::string& state) const
{
    return std::find(this->impl->activeStates.begin(), this->impl->activeStates.end(), state)
        != this->impl->activeStates.end();
}

void AppearanceInstance::clearStates()
{
    this->impl->activeStates.clear();
    this->impl->tweens.clear();
}

void AppearanceInstance::setState(const std::string& state, bool active)
{
    const bool wasActive = this->isStateActive(state);
    if (wasActive == active)
        return;

    if (active)
        this->impl->activeStates.push_back(state);
    else
        this->impl->activeStates.erase(
            std::remove(this->impl->activeStates.begin(), this->impl->activeStates.end(), state),
            this->impl->activeStates.end());

    if (this->impl->appearance == nullptr)
        return;

    // 重算每个属性的目标值：优先级最高的活跃状态说了算；没有状态管它的话回到层的初值。
    // 目标变了的属性，从**当前值**补到目标值（所以连点两下不会跳变）。
    const Appearance& appearance = *this->impl->appearance;
    const auto& layers           = appearance.getLayers();

    // 取某属性的初值（颜色走 4 通道，标量走 [0]）
    auto initialOf = [&](const AppearanceLayer& layer, AppearanceProp prop, float out[4]) {
        out[0] = out[1] = out[2] = out[3] = 0.0f;
        if (appearancePropIsColor(prop))
        {
            ImU32 color = 0;
            switch (prop)
            {
                case AppearanceProp::Fill: color = layer.fill; break;
                case AppearanceProp::Stroke: color = layer.stroke; break;
                default: color = layer.textColor; break;
            }
            splitColor(color, out);
            return;
        }

        switch (prop)
        {
            case AppearanceProp::StrokeWidth: out[0] = layer.strokeWidth; break;
            case AppearanceProp::Radius: out[0] = layer.radius; break;
            case AppearanceProp::Opacity: out[0] = layer.opacity; break;
            case AppearanceProp::OffsetX: out[0] = layer.offset[0]; break;
            case AppearanceProp::OffsetY: out[0] = layer.offset[1]; break;
            case AppearanceProp::InsetTop: out[0] = layer.inset[0]; break;
            case AppearanceProp::InsetRight: out[0] = layer.inset[1]; break;
            case AppearanceProp::InsetBottom: out[0] = layer.inset[2]; break;
            case AppearanceProp::InsetLeft: out[0] = layer.inset[3]; break;
            case AppearanceProp::FontSize: out[0] = layer.fontSize; break;
            default: break;
        }
    };

    for (size_t layerIndex = 0; layerIndex < layers.size(); layerIndex++)
    {
        const AppearanceLayer& layer = layers[layerIndex];

        for (AppearanceProp prop : ALL_PROPS)
        {
            // 1) 目标：优先级最高的活跃状态
            int bestPriority   = -1;
            float target[4]    = { 0.0f, 0.0f, 0.0f, 0.0f };
            float duration     = 0.0f;
            std::string easing = "linear";

            for (const std::string& stateName : this->impl->activeStates)
            {
                const int priority = priorityOf(stateName);
                for (const AppearanceTransition& transition : appearance.getTransitions(stateName))
                {
                    if (transition.prop != prop || transition.layerId != layer.id)
                        continue;
                    if (priority <= bestPriority)
                        continue;

                    bestPriority = priority;
                    duration     = transition.durationMs;
                    easing       = transition.easing;
                    for (int c = 0; c < 4; c++)
                        target[c] = transition.target[c];
                }
            }

            // 2) 没有活跃状态管它：回初值。时长沿用该属性在任意状态里的值，手感一致。
            if (bestPriority < 0)
            {
                initialOf(layer, prop, target);

                for (const auto& stateEntry : appearance.getStateNames())
                {
                    for (const AppearanceTransition& transition : appearance.getTransitions(stateEntry))
                    {
                        if (transition.prop == prop && transition.layerId == layer.id)
                        {
                            duration = transition.durationMs;
                            easing   = transition.easing;
                        }
                    }
                }
            }

            // 3) 对有变化的通道启动补间
            const int channels    = appearancePropIsColor(prop) ? 4 : 1;
            const uint32_t frames = (uint32_t)std::max(1.0f, duration);

            for (int c = 0; c < channels; c++)
            {
                const float current = this->impl->values[layerIndex][propIndex(prop)].channel[c];

                // **先撤掉这个通道上正在跑的补间**，再看要不要建新的。
                // 不能等到"目标与当前值不同"才撤：刚打开又马上关掉时，
                // 当前值还等于初值，旧的补间就会被漏掉，继续往旧目标跑
                // （弹窗面板整块补到透明就是这么来的）。
                this->impl->tweens.erase(
                    std::remove_if(this->impl->tweens.begin(), this->impl->tweens.end(),
                        [&](const Impl::ActiveTween& other) {
                            return other.layer == (int)layerIndex && other.prop == propIndex(prop)
                                && other.channel == c;
                        }),
                    this->impl->tweens.end());

                // 目标与当前值一致就不用补了（保持当前值）
                if (std::fabs(current - target[c]) < 0.01f)
                    continue;

                this->impl->tweens.push_back(Impl::ActiveTween { (int)layerIndex, propIndex(prop), c,
                    makeTween(current, target[c], frames, easing) });
            }
        }
    }
}

void AppearanceInstance::update(float dtSeconds)
{
    if (this->impl->tweens.empty())
        return;

    const uint32_t stepMs = (uint32_t)std::max(1.0f, dtSeconds * 1000.0f);

    for (Impl::ActiveTween& active : this->impl->tweens)
    {
        active.tween.step((int32_t)stepMs);
        this->impl->values[active.layer][active.prop].channel[active.channel] = active.tween.peek();
    }

    this->impl->tweens.erase(
        std::remove_if(this->impl->tweens.begin(), this->impl->tweens.end(),
            [](const Impl::ActiveTween& active) { return active.tween.progress() >= 1.0f; }),
        this->impl->tweens.end());
}

bool AppearanceInstance::isAnimating() const
{
    return !this->impl->tweens.empty();
}

ImU32 AppearanceInstance::getColor(int layer, AppearanceProp prop) const
{
    if (layer < 0 || (size_t)layer >= this->impl->values.size())
        return 0;
    return joinColor(this->impl->values[layer][propIndex(prop)].channel);
}

float AppearanceInstance::getNumber(int layer, AppearanceProp prop) const
{
    if (layer < 0 || (size_t)layer >= this->impl->values.size())
        return 0.0f;
    return this->impl->values[layer][propIndex(prop)].channel[0];
}

} // namespace bklv
