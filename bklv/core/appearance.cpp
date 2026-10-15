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
#include "bklv/compile/json_util.hpp"
#include "bklv/compile/layer_registry.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/ui_scale.hpp"

namespace bklv
{

// 解析助手集中在 compile/json_util（取值/报错只写一遍）
using namespace bklv::compile;

namespace
{

// 外观 JSON 里的像素量（圆角/描边/显式宽高/字号/内边距/位移）要跟随 720p 缩放；
// opacity 是系数、color 是颜色、duration 是毫秒，都不缩放。

// 主题引用里的 key 归一：`color/a.b` 与 `color.a.b` 都当成 `color.a.b`。
//
// 分组名（color / metric）后面应该是点，写成斜杠的人不少（我自己就反复写错），
// 所以这里容错，但会警告一句——**真正不存在的 key 仍然硬失败**，
// 所以容错不会退回"静默变洋红"。


using Json = nlohmann::json;

// 属性名 -> 枚举。JSON 里用小驼峰，和 View 的属性表命名保持一致。
struct PropName
{
    const char* name;
    AppearanceProp prop;
};

// 全部可插值属性，每个枚举值一条（没有别名，遍历时不用去重）
const AppearanceProp ALL_PROPS[] = {
    AppearanceProp::FILL,
    AppearanceProp::STROKE,
    AppearanceProp::TEXT_COLOR,
    AppearanceProp::STROKE_WIDTH,
    AppearanceProp::RADIUS,
    AppearanceProp::OPACITY,
    AppearanceProp::OFFSET_X,
    AppearanceProp::OFFSET_Y,
    AppearanceProp::FEATHER,
    AppearanceProp::SPREAD,
    AppearanceProp::INSET_TOP,
    AppearanceProp::INSET_RIGHT,
    AppearanceProp::INSET_BOTTOM,
    AppearanceProp::INSET_LEFT,
    AppearanceProp::FONT_SIZE,
};

// 属性总数**从表长推导**，不要写死：以前写死 13，加了两个属性后
// perLayer(PROP_COUNT) 就比枚举少两格，越界写把堆写坏了（SIGTRAP 在 malloc 里）。
constexpr int PROP_COUNT = (int)(sizeof(ALL_PROPS) / sizeof(ALL_PROPS[0]));
static_assert(PROP_COUNT == (int)AppearanceProp::FONT_SIZE + 1,
    "加了 AppearanceProp 就要往 ALL_PROPS 里补一条");

int propIndex(AppearanceProp prop)
{
    // 一次性的顺序自检：ALL_PROPS 的顺序必须与枚举一致（两边都要同步加）
    static const bool ordered = [] {
        for (int i = 0; i < PROP_COUNT; i++)
            if ((int)ALL_PROPS[i] != i)
            {
                BKLV_LOG_ERROR("外观属性表顺序与 AppearanceProp 枚举不一致（第 %d 条）", i);
                return false;
            }
        return true;
    }();
    (void)ordered;
    return (int)prop;
}


const PropName PROP_NAMES[] = {
    { "fill", AppearanceProp::FILL },
    { "stroke", AppearanceProp::STROKE },
    { "color", AppearanceProp::TEXT_COLOR },
    { "textColor", AppearanceProp::TEXT_COLOR },
    { "strokeWidth", AppearanceProp::STROKE_WIDTH },
    { "radius", AppearanceProp::RADIUS },
    { "opacity", AppearanceProp::OPACITY },
    { "offsetX", AppearanceProp::OFFSET_X },
    { "offsetY", AppearanceProp::OFFSET_Y },
    { "feather", AppearanceProp::FEATHER },
    { "spread", AppearanceProp::SPREAD },
    { "insetTop", AppearanceProp::INSET_TOP },
    { "insetRight", AppearanceProp::INSET_RIGHT },
    { "insetBottom", AppearanceProp::INSET_BOTTOM },
    { "insetLeft", AppearanceProp::INSET_LEFT },
    { "fontSize", AppearanceProp::FONT_SIZE },
};




// 颜色项：`"#RRGGBB"` 或 `"@theme/color.xxx"`

// 数值项：数字 或 `"@theme/metric.x.y"`

AppearanceLayer::Op parseOp(const std::string& text, bool& ok)
{
    ok = true;
    if (enumMatches(text, "rect"))
        return AppearanceLayer::Op::RECT;
    if (enumMatches(text, "text"))
        return AppearanceLayer::Op::TEXT;
    if (enumMatches(text, "image"))
        return AppearanceLayer::Op::IMAGE;
    if (enumMatches(text, "shadow"))
        return AppearanceLayer::Op::SHADOW;
    if (enumMatches(text, "glass"))
        return AppearanceLayer::Op::GLASS;
    if (enumMatches(text, "custom"))
        return AppearanceLayer::Op::CUSTOM;

    // 未命中内置：看注册表里有没有同名的**扩展 op**（第三方在自己初始化里注册的）。
    // 有就按扩展处理（名字由调用方记进 layer.extensionType），没有才报错。
    if (findLayer(text) != nullptr)
        return AppearanceLayer::Op::EXTENSION;

    ok = false;
    return AppearanceLayer::Op::RECT;
}

// `op: "glass"` 表达的是"这个控件的底是玻璃"，所以**永远排在最前**（早于 bg/阴影/内容）。
// 解析期排一次序，这样主题里可以用 `import` 基外观 + 追加一层 glass 的写法做玻璃变体，
// 不用关心书写位置（否则玻璃会盖住内容 —— 这是最容易犯的错）。
void hoistGlassLayers(std::vector<AppearanceLayer>& layers)
{
    // 顺序固定成：阴影（0）-> 玻璃（1）-> 其余（2）。
    // 玻璃不能真的排到"第一个"：阴影层是半透明的一整块（不是 cutout 时连面板底下也铺），
    // 排到玻璃后面会把玻璃压暗。
    auto rank = [](AppearanceLayer::Op op) {
        if (op == AppearanceLayer::Op::SHADOW)
            return 0;
        if (op == AppearanceLayer::Op::GLASS)
            return 1;
        return 2;
    };
    std::stable_sort(layers.begin(), layers.end(),
        [&rank](const AppearanceLayer& a, const AppearanceLayer& b) {
            return rank(a.op) < rank(b.op);
        });
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

    if (enumMatches(easing, "linear"))
        return tweeny::from(from).to(to).via(linear).during(frames).build();
    if (enumMatches(easing, "quadraticIn"))
        return tweeny::from(from).to(to).via(quadraticIn).during(frames).build();
    if (enumMatches(easing, "quadraticOut"))
        return tweeny::from(from).to(to).via(quadraticOut).during(frames).build();
    if (enumMatches(easing, "quadraticInOut"))
        return tweeny::from(from).to(to).via(quadraticInOut).during(frames).build();
    if (enumMatches(easing, "cubicIn"))
        return tweeny::from(from).to(to).via(cubicIn).during(frames).build();
    if (enumMatches(easing, "cubicOut"))
        return tweeny::from(from).to(to).via(cubicOut).during(frames).build();
    if (enumMatches(easing, "cubicInOut"))
        return tweeny::from(from).to(to).via(cubicInOut).during(frames).build();
    if (enumMatches(easing, "quarticOut"))
        return tweeny::from(from).to(to).via(quarticOut).during(frames).build();
    if (enumMatches(easing, "quinticOut"))
        return tweeny::from(from).to(to).via(quinticOut).during(frames).build();
    if (enumMatches(easing, "sinusoidalIn"))
        return tweeny::from(from).to(to).via(sinusoidalIn).during(frames).build();
    if (enumMatches(easing, "sinusoidalOut"))
        return tweeny::from(from).to(to).via(sinusoidalOut).during(frames).build();
    if (enumMatches(easing, "sinusoidalInOut"))
        return tweeny::from(from).to(to).via(sinusoidalInOut).during(frames).build();
    if (enumMatches(easing, "exponentialOut"))
        return tweeny::from(from).to(to).via(exponentialOut).during(frames).build();
    if (enumMatches(easing, "circularOut"))
        return tweeny::from(from).to(to).via(circularOut).during(frames).build();
    if (enumMatches(easing, "backOut"))
        return tweeny::from(from).to(to).via(backOut).during(frames).build();
    if (enumMatches(easing, "backInOut"))
        return tweeny::from(from).to(to).via(backInOut).during(frames).build();
    if (enumMatches(easing, "bounceOut"))
        return tweeny::from(from).to(to).via(bounceOut).during(frames).build();
    if (enumMatches(easing, "elasticOut"))
        return tweeny::from(from).to(to).via(elasticOut).during(frames).build();
    if (enumMatches(easing, "stepped"))
        return tweeny::from(from).to(to).via(stepped).during(frames).build();

    warnUnknownEnumValue("easing", easing,
        "linear|quadraticIn/Out/InOut|cubicIn/Out/InOut|quarticOut|quinticOut|"
        "sinusoidalIn/Out/InOut|exponentialOut|circularOut|backOut/backInOut|bounceOut|"
        "elasticOut|stepped");
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
    return prop == AppearanceProp::FILL || prop == AppearanceProp::STROKE || prop == AppearanceProp::TEXT_COLOR;
}

const std::vector<AppearanceTransition>& Appearance::getTransitions(const std::string& state) const
{
    static const std::vector<AppearanceTransition> empty;
    for (const auto& entry : this->states)
        if (entry.first == state)
            return entry.second;
    return empty;
}

const char* transitionEventName(TransitionEvent event)
{
    switch (event)
    {
        case TransitionEvent::ENTER: return "enter";
        case TransitionEvent::EXIT: return "exit";
        case TransitionEvent::SHOW: return "show";
        case TransitionEvent::HIDE: return "hide";
        default: return "enter";
    }
}

bool transitionEventFromName(const std::string& name, TransitionEvent& out)
{
    if (enumMatches(name, "enter")) { out = TransitionEvent::ENTER; return true; }
    if (enumMatches(name, "exit")) { out = TransitionEvent::EXIT; return true; }
    if (enumMatches(name, "show")) { out = TransitionEvent::SHOW; return true; }
    if (enumMatches(name, "hide")) { out = TransitionEvent::HIDE; return true; }
    return false;
}

const char* viewChannelName(ViewChannel channel)
{
    switch (channel)
    {
        case ViewChannel::TRANSLATE_X: return "translateX";
        case ViewChannel::TRANSLATE_Y: return "translateY";
        case ViewChannel::SCALE_X: return "scaleX";
        case ViewChannel::SCALE_Y: return "scaleY";
        case ViewChannel::ALPHA: return "alpha";
        default: return "alpha";
    }
}

bool viewChannelFromName(const std::string& name, ViewChannel& out)
{
    for (int i = 0; i < (int)ViewChannel::COUNT; i++)
    {
        const ViewChannel channel = (ViewChannel)i;
        if (enumMatches(name, viewChannelName(channel)))
        {
            out = channel;
            return true;
        }
    }
    return false;
}

bool viewChannelIsScaled(ViewChannel channel)
{
    // 位移是像素量（跟随 720p 缩放）；缩放倍数与透明度是系数，不缩放
    return channel == ViewChannel::TRANSLATE_X || channel == ViewChannel::TRANSLATE_Y;
}

bool Appearance::hasTransitions() const
{
    for (const TransitionEventSpec& spec : this->transitions)
        if (spec.defined)
            return true;
    return false;
}

const TransitionEventSpec& Appearance::getTransition(TransitionEvent event) const
{
    return this->transitions[(size_t)event];
}

TransitionEventSpec& Appearance::transitionsFor(TransitionEvent event)
{
    return this->transitions[(size_t)event];
}

int Appearance::indexOfLayer(const std::string& layerId) const
{
    for (size_t i = 0; i < this->layers.size(); i++)
        if (this->layers[i].id == layerId)
            return (int)i;
    return -1;
}

namespace
{

// import：一个外观可以引用别的外观，**合并规则逐字段生效**（不是整段替换）：
//   · 对象（box / states / 单个 state …）递归合并，子文件的同名字段赢
//   · layers 数组按 id 合并：同 id 的层递归合并（子赢），新 id 追加在后面
//   · 其它数组整体替换（子赢）
//   · 标量：子赢
// 所以"base/box.component 改了内边距，所有 import 它的按钮一起变"，
// 而按钮自己写的字段仍然优先（用户要的"当前控件设置覆盖导入的"）。
Json mergeJson(const Json& base, const Json& over)
{
    if (!base.is_object() || !over.is_object())
        return over;

    Json result = base;
    for (auto it = over.begin(); it != over.end(); ++it)
    {
        const std::string& key = it.key();
        if (key == "layers" && it.value().is_array() && result.contains("layers")
            && result["layers"].is_array())
        {
            // 按 id 合并层：相同 id 的合并字段，新 id 追加
            Json merged = Json::array();
            Json consumed = Json::array();

            for (const Json& baseLayer : result["layers"])
            {
                Json layer = baseLayer;
                if (baseLayer.is_object() && baseLayer.contains("id"))
                {
                    for (const Json& overLayer : it.value())
                    {
                        if (overLayer.is_object() && overLayer.contains("id")
                            && overLayer["id"] == baseLayer["id"])
                        {
                            layer = mergeJson(baseLayer, overLayer);
                            consumed.push_back(overLayer["id"]);
                        }
                    }
                }
                merged.push_back(layer);
            }

            for (const Json& overLayer : it.value())
            {
                bool already = false;
                if (overLayer.is_object() && overLayer.contains("id"))
                    for (const Json& id : consumed)
                        if (id == overLayer["id"])
                            already = true;
                if (!already)
                    merged.push_back(overLayer);
            }

            result["layers"] = merged;
            continue;
        }

        if (result.contains(key) && result[key].is_object() && it.value().is_object())
            result[key] = mergeJson(result[key], it.value());
        else
            result[key] = it.value();
    }
    return result;
}

// 把 import 链解析成一份扁平文档；visited 用于循环检测
bool resolveImports(const Json& doc, const ThemeData& theme, std::vector<std::string>& visited,
    Json& out, std::string& error)
{
    if (!doc.contains("import"))
    {
        out = doc;
        return true;
    }

    std::vector<std::string> names;
    if (doc["import"].is_string())
        names.push_back(doc["import"].get<std::string>());
    else if (doc["import"].is_array())
    {
        for (const Json& entry : doc["import"])
            if (entry.is_string())
                names.push_back(entry.get<std::string>());
    }
    else
    {
        error = "import 必须是字符串或字符串数组";
        return false;
    }

    if (visited.size() >= 8)
    {
        error = "import 链太深（超过 8 层）：" + names.front();
        return false;
    }

    Json base = Json::object();
    for (const std::string& name : names)
    {
        if (std::find(visited.begin(), visited.end(), name) != visited.end())
        {
            error = "import 出现循环引用：" + name;
            return false;
        }

        const std::string* importedJson = theme.widgetAppearanceJson(name);
        if (importedJson == nullptr)
        {
            error = "import 的外观不存在：\"" + name + "\"（检查名字与 theme.json 的 files[]）";
            return false;
        }

        Json importedDoc;
        try
        {
            importedDoc = Json::parse(*importedJson);
        }
        catch (const Json::parse_error& e)
        {
            error = "import 的 " + name + " 语法错误：" + e.what();
            return false;
        }

        visited.push_back(name);
        Json resolved;
        const bool ok = resolveImports(importedDoc, theme, visited, resolved, error);
        visited.pop_back();
        if (!ok)
        {
            error = name + " -> " + error;
            return false;
        }

        base = mergeJson(base, resolved);
    }

    Json self = doc;
    self.erase("import");
    out = mergeJson(base, self);
    return true;
}

} // namespace

// layers 段：每一层是一个"绘制声明"（op + 该 op 自己的字段）。
// 抽成函数是为下一步做准备：**每种 op 的字段解析会各自独立成文件**（compile/layers/<op>.cpp），
// 那时这里只剩按 op 的调度。
bool parseLayers(const Json& doc, const ThemeData& theme, AppearanceBox& box,
    std::vector<AppearanceLayer>& layers, std::string& error)
{
        for (size_t i = 0; i < doc["layers"].size(); i++)
        {
            const Json& node = doc["layers"][i];
            const std::string where = "layers[" + std::to_string(i) + "]";

            if (!node.is_object())
            {
                error = where + " 必须是对象";
                return false;
            }

            AppearanceLayer layer;
            // 没写 id 时用 op + 序号当 id（状态里引用它）
            const std::string opName = node.value("op", "rect");
            bool opOk               = false;
            layer.op                = parseOp(opName, opOk);
            if (layer.op == AppearanceLayer::Op::EXTENSION)
            {
                layer.extensionType = opName;

                // 扩展 op 自己的键交给**注册表里的 parse 钩子**（第三方注册时提供）。
                // 之前只记了类型名、没调钩子 —— 样例的 angle/lineWidth 其实一直没生效
                // （条纹是靠默认值碰巧画对的）。失败按硬错误处理，和内置 op 一致。
                const compile::LayerType* extType = compile::findLayer(opName);
                if (extType != nullptr && extType->parse)
                {
                    if (!extType->parse(node, theme, layer, error,
                            "layers[" + std::to_string(i) + "] (" + opName + ")"))
                        return false; // parseLayers 的失败约定是 false（compile 才转成 nullptr）
                }
            }
            if (!opOk)
            {
                error = where + ".op 不认识：" + opName + "（可用 rect / text / image / shadow / custom）";
                return false;
            }

            // 默认 id 的前缀从**注册表**取（单一事实来源）。
            // 以前这里是一张三元的映射表，只认 rect/text/image —— shadow/glass 会落到 custom 分支，
            // 被叫成 custom0 / custom1（tests/layer_parse_test.cpp 里把这个 quirk 钉成了基线，
            // 现在按注册表修掉）。
            // （opName 是上面已经取好的 op 字符串）
            const compile::LayerType* layerType = compile::findLayer(opName);
            const std::string idPrefix = layerType != nullptr ? layerType->idPrefix : opName;
            layer.id     = node.value("id", idPrefix + std::to_string(i));
            layer.source = node.value("source", "");
            layer.align  = node.value("align", "left");
            layer.valign = node.value("valign", "center");
            layer.scaling = node.value("scaling", "fit");

            // 这三项拼错时在**编译期**警告一次（绘制期每帧都比一遍，在那儿报会刷屏）
            {
                const std::string alignWhere = where + ".align";
                if (!(enumMatches(layer.align, "left") || enumMatches(layer.align, "center")
                        || enumMatches(layer.align, "right")))
                    warnUnknownEnumValue(alignWhere.c_str(), layer.align, "left|center|right");

                const std::string valignWhere = where + ".valign";
                if (!(enumMatches(layer.valign, "center") || enumMatches(layer.valign, "top")
                        || enumMatches(layer.valign, "bottom")))
                    warnUnknownEnumValue(valignWhere.c_str(), layer.valign, "center|top|bottom");

                const std::string scalingWhere = where + ".scaling";
                if (!enumMatches(layer.scaling, "fit"))
                    warnUnknownEnumValue(scalingWhere.c_str(), layer.scaling,
                        "fit（其它缩放由控件自己在 custom 层处理）");
            }

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
                    return false;
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
                { "feather", &layer.feather, true },
                { "spread", &layer.spread, true },
            };
            if (node.contains("cutout") && node["cutout"].is_boolean())
                layer.cutout = node["cutout"].get<bool>();

            for (const NumberField& field : numberFields)
            {
                if (!node.contains(field.key))
                    continue;
                if (!resolveNumber(node[field.key], theme, *field.target, error, where + "." + field.key))
                    return false;
                if (field.pixels)
                    *field.target = scalePixels(*field.target);
            }

            if (node.contains("inset") && node["inset"].is_array() && node["inset"].size() == 4)
                for (size_t k = 0; k < 4; k++)
                {
                    if (!resolveNumber(node["inset"][k], theme, layer.inset[k], error,
                            where + ".inset[" + std::to_string(k) + "]"))
                        return false;
                    layer.inset[k] = scalePixels(layer.inset[k]);
                }

            if (node.contains("offset") && node["offset"].is_array() && node["offset"].size() == 2)
                for (size_t k = 0; k < 2; k++)
                {
                    if (!resolveNumber(node["offset"][k], theme, layer.offset[k], error,
                            where + ".offset[" + std::to_string(k) + "]"))
                        return false;
                    layer.offset[k] = scalePixels(layer.offset[k]);
                }

            // RECT 层的渐变（可选）。校验从严：停靠点数量 / 递增 / 越界都**报错**，不静默降级。
            if (node.contains("gradient"))
            {
                const Json& gradientNode = node["gradient"];
                const std::string gradientWhere = where + ".gradient";
                if (!gradientNode.is_object())
                {
                    error = gradientWhere + " 要写成对象：{ type, angle, center, stops }";
                    return false;
                }

                Gradient parsed;
                const std::string typeName
                    = gradientNode.value("type", std::string("linear"));
                if (!gradientTypeFromName(typeName, parsed.type))
                {
                    error = gradientWhere + ".type 只支持 linear / radial（当前：" + typeName + "）";
                    return false;
                }

                // angle / center / at 都是**无量纲**，不跟随 720p 缩放
                if (gradientNode.contains("angle")
                    && !resolveNumber(gradientNode["angle"], theme, parsed.angle, error,
                        gradientWhere + ".angle"))
                    return false;

                if (gradientNode.contains("center"))
                {
                    const Json& centerNode = gradientNode["center"];
                    if (!centerNode.is_array() || centerNode.size() != 2)
                    {
                        error = gradientWhere + ".center 要写成 [x, y]（0..1 相对层矩形）";
                        return false;
                    }
                    for (size_t k = 0; k < 2; k++)
                    {
                        float value = 0.0f;
                        if (!resolveNumber(centerNode[k], theme, value, error,
                                gradientWhere + ".center[" + std::to_string(k) + "]"))
                            return false;
                        if (value < 0.0f || value > 1.0f)
                        {
                            error = gradientWhere + ".center[" + std::to_string(k)
                                + "] 要在 0..1 之内（当前：" + std::to_string(value) + "）";
                            return false;
                        }
                        parsed.center[k] = value;
                    }
                }

                if (!gradientNode.contains("stops") || !gradientNode["stops"].is_array())
                {
                    error = gradientWhere + ".stops 要写成数组：[{ \"at\": 0, \"color\": \"#RRGGBBAA\" }, ...]";
                    return false;
                }

                const Json& stopsNode = gradientNode["stops"];
                if (stopsNode.size() < 2 || (int)stopsNode.size() > kMaxGradientStops)
                {
                    error = gradientWhere + ".stops 要 2~" + std::to_string(kMaxGradientStops)
                        + " 个（当前 " + std::to_string(stopsNode.size()) + " 个）";
                    return false;
                }

                float previousAt = -1.0f;
                for (size_t i = 0; i < stopsNode.size(); i++)
                {
                    const Json& stopNode  = stopsNode[i];
                    const std::string stopWhere
                        = gradientWhere + ".stops[" + std::to_string(i) + "]";
                    if (!stopNode.is_object())
                    {
                        error = stopWhere + " 要写成对象 { at, color }";
                        return false;
                    }

                    GradientStop stop;
                    if (stopNode.contains("at")
                        && !resolveNumber(stopNode["at"], theme, stop.at, error, stopWhere + ".at"))
                        return false;
                    if (stop.at < 0.0f || stop.at > 1.0f)
                    {
                        error = stopWhere + ".at 要在 0..1 之内（当前：" + std::to_string(stop.at) + "）";
                        return false;
                    }
                    if (stop.at <= previousAt)
                    {
                        error = stopWhere + ".at 必须比上一个停靠点大（停在 "
                            + std::to_string(previousAt) + "，这个是 " + std::to_string(stop.at) + "）";
                        return false;
                    }
                    previousAt = stop.at;

                    if (!stopNode.contains("color"))
                    {
                        error = stopWhere + " 缺少 color";
                        return false;
                    }
                    if (!resolveColor(stopNode["color"], theme, stop.color, error, stopWhere + ".color"))
                        return false;

                    parsed.stops[i] = stop;
                }
                parsed.stopCount = (int)stopsNode.size();
                layer.gradient   = parsed;
            }

            layers.push_back(layer);
        }

        // 玻璃层一律提到最前（见 hoistGlassLayers 的注释）
        hoistGlassLayers(layers);

        // ---- 盒模型/布局（顶层 "box" 段）----
        if (doc.contains("box") && doc["box"].is_object())
        {
            const Json& boxNode = doc["box"];

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
                return false;
            box.hasWidth = boxNode.contains("width");

            if (boxNode.contains("height") && !readLength(boxNode["height"], box.height, box.heightPercent,
                                               box.heightAuto, where + ".height"))
                return false;
            box.hasHeight = boxNode.contains("height");

            if (!readNumber("minWidth", box.minWidth, box.hasMinWidth, where))
                return false;
            if (!readNumber("minHeight", box.minHeight, box.hasMinHeight, where))
                return false;
            if (!readNumber("gap", box.gap, box.hasGap, where))
                return false;
            if (!readNumber("grow", box.grow, box.hasGrow, where))
                return false;
            if (!readNumber("shrink", box.shrink, box.hasShrink, where))
                return false;
            if (!readNumber("cornerRadius", box.cornerRadius, box.hasCornerRadius, where))
                return false;
            if (!readNumber("borderThickness", box.borderThickness, box.hasBorderThickness, where))
                return false;

            if (boxNode.contains("padding"))
            {
                if (!readEdges(boxNode["padding"], box.padding, where + ".padding"))
                    return false;
                box.hasPadding = true;
            }
            if (boxNode.contains("margin"))
            {
                if (!readEdges(boxNode["margin"], box.margin, where + ".margin"))
                    return false;
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
            if (boxNode.contains("axis") && boxNode["axis"].is_string())
            {
                box.axis    = boxNode["axis"].get<std::string>();
                box.hasAxis = true;
            }
            if (boxNode.contains("measureContent") && boxNode["measureContent"].is_string())
            {
                const std::string mode = boxNode["measureContent"].get<std::string>();
                if (enumMatches(mode, "sum"))
                    box.measureSum = true;
                else if (enumMatches(mode, "max"))
                    box.measureSum = false;
                else
                    warnUnknownEnumValue("measureContent", mode, "max|sum");
            }
            if (boxNode.contains("backgroundColor"))
            {
                if (!resolveColor(boxNode["backgroundColor"], theme, box.background, error,
                        where + ".backgroundColor"))
                    return false;
                box.hasBackground = true;
            }
            if (boxNode.contains("borderColor"))
            {
                if (!resolveColor(boxNode["borderColor"], theme, box.borderColor, error,
                        where + ".borderColor"))
                    return false;
                box.hasBorderColor = true;
            }
        }
    return true;
}

// states 段：`states.<状态>.<层id>.<属性>` -> 每个状态一组通道补间。
// 抽成函数只为可读（原来和 box / layers / transitions 挤在 compile 里一个 501 行的函数体）。
bool parseStates(const Json& doc, const ThemeData& theme, Appearance& target,
    std::vector<std::string>& stateNames,
    std::vector<std::pair<std::string, std::vector<AppearanceTransition>>>& states,
    std::string& error)
{
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
                    return false;
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
                        return false;
                    }

                    const std::string layerId = property.substr(0, dot);
                    const std::string propName = property.substr(dot + 1);
                    const int layerIndex       = target.indexOfLayer(layerId);
                    if (layerIndex < 0)
                    {
                        error = "states." + stateName + "." + property + " 引用了不存在的层 \"" + layerId + "\"";
                        return false;
                    }

                    AppearanceProp prop = AppearanceProp::FILL;
                    if (!appearancePropFromName(propName, prop))
                    {
                        error = "states." + stateName + "." + property + " 不认识的属性 \"" + propName + "\"";
                        return false;
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
                        return false;
                    }

                    if (appearancePropIsColor(prop))
                    {
                        ImU32 color = 0;
                        if (!resolveColor(target["to"], theme, color, error, "states." + stateName + "." + property + ".to"))
                            return false;
                        splitColor(color, transition.target);
                    }
                    else if (!resolveNumber(target["to"], theme, transition.target[0], error,
                                 "states." + stateName + "." + property + ".to"))
                    {
                        return false;
                    }
                    else if (prop != AppearanceProp::OPACITY)
                    {
                        // 几何量（圆角/描边宽/字号/位移/内边距）的目标值也跟随 720p 缩放；
                        // 颜色与 opacity 不缩放
                        transition.target[0] = scalePixels(transition.target[0]);
                    }

                    transitions.push_back(transition);
                }

                stateNames.push_back(stateName);
                states.emplace_back(stateName, std::move(transitions));
            }
        }
    return true;
}

// 视图过渡（transitions）解析。写错一律报错（不静默降级）：
//   · transitions 不是对象 / 事件名不认识（只支持 enter/exit/show/hide）
//   · 不认识的视图级通道（顺带把 "type": "slideFade" 这类"预设写法"堵掉，并提示直接写通道）
//   · 层 id 不存在 / 属性名不认识 / 颜色或数字解析失败 / 缺 to
//   · when 不是 with/after（写 before 会明确说明为什么不行）
//   · 时间与位移出现负数
// 一条**警告**：出场/隐藏里写 when:after（那时已经看不见了），按 with 处理。
//
// 时间在解析期就算好：视图级总时长 → 给 when:after 的层内条目补 delay → 取全局最大
// 得到 spec.totalMs（调用方拿它决定"这段动画播完"的时机）。
bool parseTransitions(const Json& root, const Appearance& appearance, const ThemeData& theme,
    Appearance& target, std::string& error)
{
    if (!root.contains("transitions"))
        return true;
    if (!root["transitions"].is_object())
    {
        error = "transitions 必须是对象";
        return false;
    }

    const float defaultDuration = theme.number("metric.anim.normal", 150.0f);

    for (auto eventIt = root["transitions"].begin(); eventIt != root["transitions"].end(); ++eventIt)
    {
        const std::string eventName = eventIt.key();
        TransitionEvent event      = TransitionEvent::ENTER;
        if (!transitionEventFromName(eventName, event))
        {
            error = "transitions 不认识的键 \"" + eventName
                + "\"（只支持 enter / exit / show / hide）";
            return false;
        }
        if (!eventIt.value().is_object())
        {
            error = "transitions." + eventName + " 必须是对象";
            return false;
        }

        const Json& node        = eventIt.value();
        const std::string where = "transitions." + eventName;
        // 出场 / 隐藏没有"动画之后"这一段（那时视图已经看不见了）
        const bool goneEvent    = (event == TransitionEvent::EXIT || event == TransitionEvent::HIDE);

        TransitionEventSpec spec;
        spec.defined    = true;
        spec.durationMs = defaultDuration;
        spec.easing     = "quadraticOut";

        if (node.contains("duration"))
        {
            if (!node["duration"].is_number())
            {
                error = where + ".duration 必须是数字（毫秒）";
                return false;
            }
            spec.durationMs = node["duration"].get<float>();
        }
        if (node.contains("delay"))
        {
            if (!node["delay"].is_number())
            {
                error = where + ".delay 必须是数字（毫秒）";
                return false;
            }
            spec.delayMs = node["delay"].get<float>();
        }
        if (node.contains("easing"))
        {
            if (!node["easing"].is_string())
            {
                error = where + ".easing 必须是字符串";
                return false;
            }
            spec.easing = node["easing"].get<std::string>();
        }
        if (spec.durationMs < 0.0f || spec.delayMs < 0.0f)
        {
            error = where + " 的 duration / delay 不能是负数（duration: 0 表示瞬变）";
            return false;
        }

        // ---- 第一趟：视图级通道（整树一起动）----
        for (auto keyIt = node.begin(); keyIt != node.end(); ++keyIt)
        {
            const std::string key = keyIt.key();
            if (key == "duration" || key == "delay" || key == "easing" || key == "layers")
                continue;

            ViewChannel channel = ViewChannel::ALPHA;
            if (!viewChannelFromName(key, channel))
            {
                error = where + " 不认识的键 \"" + key
                    + "\"：视图级通道只支持 translateX / translateY / scaleX / scaleY / alpha"
                      "（过渡不做预设，效果直接写通道名；层内动画写在 layers 里）";
                return false;
            }
            if (!keyIt.value().is_object() || !keyIt.value().contains("to"))
            {
                error = where + "." + key
                    + " 必须写成 { \"from\": 起点, \"to\": 终点, \"duration\": 毫秒 }";
                return false;
            }

            const Json& valueNode = keyIt.value();
            for (auto valueIt = valueNode.begin(); valueIt != valueNode.end(); ++valueIt)
            {
                const std::string& valueKey = valueIt.key();
                if (valueKey != "from" && valueKey != "to" && valueKey != "duration"
                    && valueKey != "easing" && valueKey != "delay")
                {
                    error = where + "." + key + " 里不认识的键 \"" + valueKey
                        + "\"（通道对象只支持 from / to / duration / easing / delay）";
                    return false;
                }
            }

            ViewChannelClip clip;
            clip.channel    = channel;
            clip.durationMs = valueNode.value("duration", spec.durationMs);
            clip.delayMs    = valueNode.value("delay", spec.delayMs);
            clip.easing     = valueNode.value("easing", spec.easing);
            if (clip.durationMs < 0.0f || clip.delayMs < 0.0f)
            {
                error = where + "." + key + " 的 duration / delay 不能是负数";
                return false;
            }
            if (!resolveNumber(valueNode["to"], theme, clip.target, error, where + "." + key + ".to"))
                return false;
            if (viewChannelIsScaled(channel))
                clip.target = scalePixels(clip.target);
            if (valueNode.contains("from"))
            {
                if (!resolveNumber(valueNode["from"], theme, clip.source, error,
                        where + "." + key + ".from"))
                    return false;
                if (viewChannelIsScaled(channel))
                    clip.source = scalePixels(clip.source);
                clip.hasSource = true;
            }
            spec.viewClips.push_back(clip);
        }

        for (const ViewChannelClip& clip : spec.viewClips)
            spec.viewTotalMs = std::max(spec.viewTotalMs, clip.delayMs + clip.durationMs);

        // ---- 第二趟：层内动画（这时 viewTotalMs 已知，when:after 才能补 delay）----
        if (node.contains("layers"))
        {
            if (!node["layers"].is_object())
            {
                error = where + ".layers 必须是对象";
                return false;
            }

            for (auto layerIt = node["layers"].begin(); layerIt != node["layers"].end(); ++layerIt)
            {
                const std::string layerId = layerIt.key();
                const std::string layerWhere = where + ".layers." + layerId;

                if (appearance.indexOfLayer(layerId) < 0)
                {
                    error = layerWhere + " 引用了不存在的层 \"" + layerId + "\"";
                    return false;
                }
                if (!layerIt.value().is_object())
                {
                    error = layerWhere + " 必须是对象";
                    return false;
                }

                const Json& layerNode = layerIt.value();

                std::string when = "with";
                if (layerNode.contains("when"))
                {
                    if (!layerNode["when"].is_string())
                    {
                        error = layerWhere + ".when 必须是字符串";
                        return false;
                    }
                    when = layerNode["when"].get<std::string>();
                    if (enumMatches(when, "before"))
                    {
                        error = layerWhere
                            + ".when: \"before\" 不支持 —— 出现之前 / 消失之后视图都看不见，"
                              "那段时间做动画没有意义；想让内容先动、整页后动，用 delay 表达提前量";
                        return false;
                    }
                    if (!enumMatches(when, "with") && !enumMatches(when, "after"))
                    {
                        error = layerWhere + ".when 只支持 \"with\" / \"after\"";
                        return false;
                    }
                    if (goneEvent && enumMatches(when, "after"))
                    {
                        BKLV_LOG_WARNING("%s.when: 出场/隐藏没有 \"after\" 段（那时已经看不见了），按 with 处理",
                            layerWhere.c_str());
                        when = "with";
                    }
                }

                float entryDelay    = layerNode.value("delay", 0.0f);
                float entryDuration = layerNode.value("duration", spec.durationMs);
                std::string entryEasing = layerNode.value("easing", spec.easing);
                if (entryDelay < 0.0f || entryDuration < 0.0f)
                {
                    error = layerWhere + " 的 delay / duration 不能是负数";
                    return false;
                }
                if (enumMatches(when, "after"))
                    entryDelay += spec.viewTotalMs;

                for (auto propIt = layerNode.begin(); propIt != layerNode.end(); ++propIt)
                {
                    const std::string propName = propIt.key();
                    if (propName == "when" || propName == "delay" || propName == "duration"
                        || propName == "easing")
                        continue;

                    AppearanceProp prop = AppearanceProp::FILL;
                    if (!appearancePropFromName(propName, prop))
                    {
                        error = layerWhere + " 不认识的属性 \"" + propName
                            + "\"（和 states 一样写层的属性名）";
                        return false;
                    }
                    if (!propIt.value().is_object() || !propIt.value().contains("to"))
                    {
                        error = layerWhere + "." + propName
                            + " 必须写成 { \"from\": 起点, \"to\": 终点, \"duration\": 毫秒 }";
                        return false;
                    }

                    const Json& valueNode = propIt.value();
                    for (auto valueIt = valueNode.begin(); valueIt != valueNode.end(); ++valueIt)
                    {
                        const std::string& valueKey = valueIt.key();
                        if (valueKey != "from" && valueKey != "to" && valueKey != "duration"
                            && valueKey != "easing" && valueKey != "delay")
                        {
                            error = layerWhere + "." + propName + " 里不认识的键 \"" + valueKey
                                + "\"（通道对象只支持 from / to / duration / easing / delay）";
                            return false;
                        }
                    }

                    AppearanceTransition clip;
                    clip.layerId    = layerId;
                    clip.prop       = prop;
                    // 通道对象里的写法优先于层条目级（层条目级又优先于事件默认）
                    clip.durationMs = valueNode.value("duration", entryDuration);
                    clip.easing     = valueNode.value("easing", entryEasing);
                    clip.delayMs    = entryDelay + valueNode.value("delay", 0.0f);
                    if (clip.durationMs < 0.0f || clip.delayMs < 0.0f)
                    {
                        error = layerWhere + "." + propName + " 的 duration / delay 不能是负数";
                        return false;
                    }

                    if (appearancePropIsColor(prop))
                    {
                        ImU32 toColor = 0;
                        if (!resolveColor(valueNode["to"], theme, toColor, error,
                                layerWhere + "." + propName + ".to"))
                            return false;
                        splitColor(toColor, clip.target);

                        if (valueNode.contains("from"))
                        {
                            ImU32 fromColor = 0;
                            if (!resolveColor(valueNode["from"], theme, fromColor, error,
                                    layerWhere + "." + propName + ".from"))
                                return false;
                            splitColor(fromColor, clip.source);
                            clip.hasSource = true;
                        }
                    }
                    else
                    {
                        if (!resolveNumber(valueNode["to"], theme, clip.target[0], error,
                                layerWhere + "." + propName + ".to"))
                            return false;
                        if (prop != AppearanceProp::OPACITY)
                            clip.target[0] = scalePixels(clip.target[0]);

                        if (valueNode.contains("from"))
                        {
                            if (!resolveNumber(valueNode["from"], theme, clip.source[0], error,
                                    layerWhere + "." + propName + ".from"))
                                return false;
                            if (prop != AppearanceProp::OPACITY)
                                clip.source[0] = scalePixels(clip.source[0]);
                            clip.hasSource = true;
                        }
                    }

                    spec.layerClips.push_back(clip);
                }
            }
        }

        // ---- 事件总时长 = max(事件时长, 每条条目 delay + duration) ----
        spec.totalMs = spec.durationMs;
        for (const ViewChannelClip& clip : spec.viewClips)
            spec.totalMs = std::max(spec.totalMs, clip.delayMs + clip.durationMs);
        for (const AppearanceTransition& clip : spec.layerClips)
            spec.totalMs = std::max(spec.totalMs, clip.delayMs + clip.durationMs);

        target.transitionsFor(event) = std::move(spec);
    }

    return true;
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

    if (doc.is_object() && doc.contains("import"))
    {
        std::vector<std::string> visited;
        Json resolved;
        if (!resolveImports(doc, theme, visited, resolved, error))
            return nullptr;
        doc = std::move(resolved);
    }

    if (!doc.is_object() || !doc.contains("layers") || !doc["layers"].is_array())
    {
        error = "外观 JSON 缺少 layers 数组";
        return nullptr;
    }

    auto appearance = std::shared_ptr<Appearance>(new Appearance());

    // 图层
    if (!parseLayers(doc, theme, appearance->box, appearance->layers, error))
        return nullptr;


    // 状态
    if (!parseStates(doc, theme, *appearance, appearance->stateNames, appearance->states, error))
        return nullptr;

    // 视图过渡（enter / exit / show / hide）
    if (!parseTransitions(doc, *appearance, theme, *appearance, error))
        return nullptr;

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

// 外观 box 的枚举也走宽容匹配：flex_start / flexStart / FLEX_START 都认
// （视图描述 JSON 传统写 camelCase、外观 JSON 传统写 snake_case，统一后两边随便写）
// 返回 false = 这个名字不认识（已打警告），调用方**不要动**当前值
bool justifyFromName(const std::string& name, JustifyContent& out)
{
    if (enumMatches(name, "flexStart"))
        out = JustifyContent::FLEX_START;
    else if (enumMatches(name, "center"))
        out = JustifyContent::CENTER;
    else if (enumMatches(name, "flexEnd"))
        out = JustifyContent::FLEX_END;
    else if (enumMatches(name, "spaceBetween"))
        out = JustifyContent::SPACE_BETWEEN;
    else if (enumMatches(name, "spaceAround"))
        out = JustifyContent::SPACE_AROUND;
    else if (enumMatches(name, "spaceEvenly"))
        out = JustifyContent::SPACE_EVENLY;
    else
        return warnUnknownEnumValue("justifyContent", name,
            "flexStart|center|flexEnd|spaceBetween|spaceAround|spaceEvenly");
    return true;
}

bool alignFromName(const std::string& name, AlignItems& out)
{
    if (enumMatches(name, "flexStart"))
        out = AlignItems::FLEX_START;
    else if (enumMatches(name, "center"))
        out = AlignItems::CENTER;
    else if (enumMatches(name, "flexEnd"))
        out = AlignItems::FLEX_END;
    else if (enumMatches(name, "stretch"))
        out = AlignItems::STRETCH;
    else if (enumMatches(name, "baseline"))
        out = AlignItems::BASELINE;
    else
        return warnUnknownEnumValue("alignItems", name,
            "flexStart|center|flexEnd|stretch|baseline");
    return true;
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
    if (box.hasAxis)
    {
        if (enumMatches(box.axis, "row"))
            view->setAxis(Axis::ROW);
        else if (enumMatches(box.axis, "column"))
            view->setAxis(Axis::COLUMN);
        else
            warnUnknownEnumValue("axis", box.axis, "row|column");
    }
    if (box.hasJustify)
    {
        JustifyContent justify = JustifyContent::FLEX_START;
        if (justifyFromName(box.justify, justify))
            view->setJustifyContent(justify);
    }
    if (box.hasAlign)
    {
        AlignItems alignment = AlignItems::STRETCH;
        if (alignFromName(box.align, alignment))
            view->setAlignItems(alignment);
    }
    if (box.hasBackground)
        view->setBackgroundColor(box.background);
    if (box.hasCornerRadius)
        view->setCornerRadius(box.cornerRadius, false); // 主题值，不算"显式"，代码可以覆盖
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

const std::vector<std::string>& appearancePropNames()
{
    static const std::vector<std::string> names = [] {
        std::vector<std::string> list;
        for (int i = 0; i < PROP_COUNT; i++)
            list.push_back(PROP_NAMES[i].name);
        return list;
    }();
    return names;
}

bool validateThemeAppearances(std::string& error)
{
    const ThemeData& theme = getTheme();
    for (const std::string& widget : theme.getWidgetNames())
    {
        const std::string* text = theme.widgetAppearanceJson(widget);
        if (text == nullptr)
            continue;

        std::string localError;
        if (Appearance::compile(*text, theme, localError) == nullptr)
        {
            error = widget + "：" + localError;
            return false;
        }
    }
    return true;
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
        splitColor(layer.fill, perLayer[propIndex(AppearanceProp::FILL)].channel);
        splitColor(layer.stroke, perLayer[propIndex(AppearanceProp::STROKE)].channel);
        splitColor(layer.textColor, perLayer[propIndex(AppearanceProp::TEXT_COLOR)].channel);
        perLayer[propIndex(AppearanceProp::STROKE_WIDTH)].channel[0] = layer.strokeWidth;
        perLayer[propIndex(AppearanceProp::RADIUS)].channel[0]      = layer.radius;
        perLayer[propIndex(AppearanceProp::OPACITY)].channel[0]     = layer.opacity;
        perLayer[propIndex(AppearanceProp::OFFSET_X)].channel[0]     = layer.offset[0];
        perLayer[propIndex(AppearanceProp::OFFSET_Y)].channel[0]     = layer.offset[1];
        perLayer[propIndex(AppearanceProp::FEATHER)].channel[0]      = layer.feather;
        perLayer[propIndex(AppearanceProp::SPREAD)].channel[0]       = layer.spread;
        perLayer[propIndex(AppearanceProp::INSET_TOP)].channel[0]    = layer.inset[0];
        perLayer[propIndex(AppearanceProp::INSET_RIGHT)].channel[0]  = layer.inset[1];
        perLayer[propIndex(AppearanceProp::INSET_BOTTOM)].channel[0] = layer.inset[2];
        perLayer[propIndex(AppearanceProp::INSET_LEFT)].channel[0]   = layer.inset[3];
        perLayer[propIndex(AppearanceProp::FONT_SIZE)].channel[0]    = layer.fontSize;
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
                case AppearanceProp::FILL: color = layer.fill; break;
                case AppearanceProp::STROKE: color = layer.stroke; break;
                default: color = layer.textColor; break;
            }
            splitColor(color, out);
            return;
        }

        switch (prop)
        {
            case AppearanceProp::STROKE_WIDTH: out[0] = layer.strokeWidth; break;
            case AppearanceProp::RADIUS: out[0] = layer.radius; break;
            case AppearanceProp::OPACITY: out[0] = layer.opacity; break;
            case AppearanceProp::OFFSET_X: out[0] = layer.offset[0]; break;
            case AppearanceProp::OFFSET_Y: out[0] = layer.offset[1]; break;
            case AppearanceProp::FEATHER: out[0] = layer.feather; break;
            case AppearanceProp::SPREAD: out[0] = layer.spread; break;
            case AppearanceProp::INSET_TOP: out[0] = layer.inset[0]; break;
            case AppearanceProp::INSET_RIGHT: out[0] = layer.inset[1]; break;
            case AppearanceProp::INSET_BOTTOM: out[0] = layer.inset[2]; break;
            case AppearanceProp::INSET_LEFT: out[0] = layer.inset[3]; break;
            case AppearanceProp::FONT_SIZE: out[0] = layer.fontSize; break;
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

void AppearanceInstance::cancelTween(int layer, AppearanceProp prop)
{
    auto& tweens = this->impl->tweens;
    tweens.erase(std::remove_if(tweens.begin(), tweens.end(),
                     [layer, prop](const Impl::ActiveTween& tween) {
                         return tween.layer == layer && tween.prop == (int)prop;
                     }),
        tweens.end());
}

void AppearanceInstance::setNumber(int layer, AppearanceProp prop, float value)
{
    if (layer < 0)
        return;
    if ((size_t)layer >= this->impl->values.size())
        return;
    if ((size_t)prop >= this->impl->values[(size_t)layer].size())
        return;

    this->cancelTween(layer, prop);
    this->impl->values[(size_t)layer][(size_t)prop].channel[0] = value;
}

void AppearanceInstance::setColor(int layer, AppearanceProp prop, const float rgba[4])
{
    if (layer < 0 || rgba == nullptr)
        return;
    if ((size_t)layer >= this->impl->values.size())
        return;
    if ((size_t)prop >= this->impl->values[(size_t)layer].size())
        return;

    this->cancelTween(layer, prop);
    for (int i = 0; i < 4; i++)
        this->impl->values[(size_t)layer][(size_t)prop].channel[i] = rgba[i];
}

float AppearanceInstance::getNumber(int layer, AppearanceProp prop) const
{
    if (layer < 0 || (size_t)layer >= this->impl->values.size())
        return 0.0f;
    return this->impl->values[layer][propIndex(prop)].channel[0];
}

} // namespace bklv
