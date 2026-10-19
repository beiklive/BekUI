#include "bklv/compile/layer_registry.hpp"

#include <algorithm>
#include <array>

#include "bklv/core/globals.hpp"
#include "bklv/core/logger.hpp"

namespace bklv
{
namespace compile
{

namespace
{

std::vector<LayerType>& registry()
{
    static std::vector<LayerType> layers;
    return layers;
}

// 内置层是否已经注册过。刻意放**命名空间作用域**而不是函数内 static：
// clearLayersForTest() / resetGlobals() 要把这个标志一起翻回来 —— 只清 vector 不翻标志的话，
// 重置之后内置的 rect / text / glass 会静默消失（外表全报"未知 op"，比崩更难查）。
bool g_builtinsRegistered = false;

// 内置层惰性注册一次：调用方不用关心初始化顺序，
// 第三方仍然可以在自己的初始化里提前 registerLayer（重复会报错，不会互相覆盖）
void ensureBuiltins()
{
    if (g_builtinsRegistered)
        return;
    g_builtinsRegistered = true;
    registerBuiltinLayers();

    // 注册表的状态是自己的静态表（搬不进全局表），按 core/globals.hpp 的约定登记重置钩子
    globals::getGlobalStore().addResetHook("layers", &clearLayersForTest);
}

} // namespace

const std::vector<std::string>& LayerType::reservedKeys()
{
    static const std::vector<std::string> keys = {
        "op", "id", "width", "height", "inset", "align", "valign", "offset", "opacity",
    };
    return keys;
}

// ---- LayerParam 的便捷构造（写法见 layer_registry.hpp 的注释）----

LayerParam LayerParam::number(std::string name, float defaultValue, std::string label, std::string help)
{
    LayerParam param;
    param.name          = std::move(name);
    param.type          = LayerParamType::NUMBER;
    param.numberDefault = defaultValue;
    param.label         = std::move(label);
    param.help          = std::move(help);
    return param;
}

LayerParam LayerParam::pixels(std::string name, float defaultValue, std::string label, std::string help)
{
    LayerParam param = number(std::move(name), defaultValue, std::move(label), std::move(help));
    param.type       = LayerParamType::PIXELS;
    return param;
}

LayerParam LayerParam::color(std::string name, ImU32 defaultValue, std::string label, std::string help)
{
    LayerParam param;
    param.name         = std::move(name);
    param.type         = LayerParamType::COLOR;
    param.colorDefault = defaultValue;
    param.label        = std::move(label);
    param.help         = std::move(help);
    return param;
}

LayerParam LayerParam::boolean(std::string name, bool defaultValue, std::string label, std::string help)
{
    LayerParam param;
    param.name        = std::move(name);
    param.type        = LayerParamType::BOOL;
    param.boolDefault = defaultValue;
    param.label       = std::move(label);
    param.help        = std::move(help);
    return param;
}

LayerParam LayerParam::enumeration(std::string name, std::vector<std::string> values, int defaultIndex,
    std::string label, std::string help)
{
    LayerParam param;
    param.name          = std::move(name);
    param.type          = LayerParamType::ENUM;
    param.enumValues    = std::move(values);
    param.numberDefault = (float)defaultIndex;
    param.label         = std::move(label);
    param.help          = std::move(help);
    return param;
}

const std::vector<std::string>& engineLayerKeys()
{
    // 与 compile/appearance_layer_meta.cpp（元信息）和 compile/appearance_layer_fields.cpp
    // （通用字段表）保持一致：那边加一个键，这里补一条，设计器与注册校验就自动跟上。
    static const std::vector<std::string> keys = {
        // 元信息
        "op", "id", "data_id", "source", "align", "valign", "scaling",
        // 通用字段：颜色
        "fill", "stroke", "color", "textColor",
        // 通用字段：数值
        "strokeWidth", "radius", "opacity", "fontSize", "width", "height", "feather", "spread",
        // 通用字段：结构
        "cutout", "inset", "offset", "gradient",
    };
    return keys;
}

bool registerLayer(LayerType type, std::string& error)
{
    if (type.name.empty())
    {
        error = "图层类型的 name 不能为空";
        return false;
    }
    if (type.idPrefix.empty())
    {
        error = "图层类型 \"" + type.name + "\" 的 idPrefix 不能为空（没写 id 时要用它生成默认 id）";
        return false;
    }
    if (!type.allowReservedKeys)
    {
        const std::vector<std::string>& reserved = LayerType::reservedKeys();
        for (const std::string& key : type.keys)
        {
            if (std::find(reserved.begin(), reserved.end(), key) != reserved.end())
            {
                error = "图层类型 \"" + type.name + "\" 的键 \"" + key
                    + "\" 是引擎保留键（层几何/通用属性），扩展层请起独立名字"
                      "（如 lineWidth / gap）";
                return false;
            }
        }
    }

    // 参数模式的三条校验。放在注册期报清楚，避免"JSON 里写了却没生效"这种要读代码才懂的坑。
    if (type.params.size() > kMaxLayerParams)
    {
        error = "图层类型 \"" + type.name + "\" 声明了 " + std::to_string(type.params.size())
            + " 个参数，上限是 " + std::to_string(kMaxLayerParams)
            + "（参数槽是定长存放的，见 core/appearance.hpp 的 kMaxLayerParams）";
        return false;
    }
    for (size_t i = 0; i < type.params.size(); i++)
    {
        const LayerParam& param = type.params[i];
        if (param.name.empty())
        {
            error = "图层类型 \"" + type.name + "\" 的第 " + std::to_string(i) + " 个参数没有名字";
            return false;
        }
        for (size_t j = i + 1; j < type.params.size(); j++)
        {
            if (type.params[j].name == param.name)
            {
                error = "图层类型 \"" + type.name + "\" 的参数 \"" + param.name + "\" 重复声明";
                return false;
            }
        }
        if (param.type == LayerParamType::ENUM && param.enumValues.empty())
        {
            error = "图层类型 \"" + type.name + "\" 的参数 \"" + param.name
                + "\" 是 ENUM 但没有给 enumValues（拼错时就没法提示可选值）";
            return false;
        }
        // 参数名不能占用引擎保留键：那些键引擎会先拿去算矩形/透明度，压根到不了参数解析
        if (!type.allowReservedKeys)
        {
            const std::vector<std::string>& reserved = LayerType::reservedKeys();
            if (std::find(reserved.begin(), reserved.end(), param.name) != reserved.end())
            {
                error = "图层类型 \"" + type.name + "\" 的参数 \"" + param.name
                    + "\" 是引擎保留键（引擎会先拿它算几何，参数永远收不到值）";
                return false;
            }
        }

        // 撞上引擎自己解析的键（fill / radius / angle 这类）不算错，但那一层会有**两份值**：
        // 引擎一份（`ctx.layer.fill`）、参数槽一份。警告一次，让人决定是改名还是直接用引擎字段。
        const std::vector<std::string>& engineKeys = engineLayerKeys();
        if (std::find(engineKeys.begin(), engineKeys.end(), param.name) != engineKeys.end())
            BKLV_LOG_WARNING("图层类型 \"%s\" 的参数 \"%s\" 和引擎自己解析的键重名："
                             "引擎会把值放进 layer.%s，参数槽里是另一份；建议改名，"
                             "或者绘制时直接用 ctx.layer.%s",
                type.name.c_str(), param.name.c_str(), param.name.c_str(), param.name.c_str());
    }

    if (findLayer(type.name) != nullptr)
    {
        error = "图层类型 \"" + type.name + "\" 重复注册";
        return false;
    }

    registry().push_back(std::move(type));
    return true;
}

const LayerType* builtinLayer(AppearanceLayer::Op op)
{
    ensureBuiltins();

    // 下标 = 枚举值。数组懒建一次：内置 6 种注册完之后就不再变，所以缓存是安全的
    // （注册表本身是进程级生命周期，指针不会失效）。
    static const std::array<const LayerType*, 6> table = [] {
        std::array<const LayerType*, 6> built{};
        const AppearanceLayer::Op ops[6] = { AppearanceLayer::Op::RECT, AppearanceLayer::Op::TEXT,
            AppearanceLayer::Op::IMAGE, AppearanceLayer::Op::SHADOW, AppearanceLayer::Op::GLASS,
            AppearanceLayer::Op::CUSTOM };
        for (size_t i = 0; i < 6; i++)
            built[i] = findLayer(layerOpName(ops[i]));
        return built;
    }();

    const int index = (int)op;
    if (index < 0 || index >= (int)table.size())
        return nullptr; // EXTENSION 或非法值：调用方走 findLayer(extensionType)
    return table[(size_t)index];
}

const LayerType* findLayer(const std::string& name)
{
    ensureBuiltins();
    const std::vector<LayerType>& layers = registry();
    auto it = std::find_if(layers.begin(), layers.end(),
        [&name](const LayerType& type) { return type.name == name; });
    return it == layers.end() ? nullptr : &(*it);
}

bool setLayerDraw(const std::string& name, LayerDrawFn draw)
{
    ensureBuiltins();
    for (LayerType& type : registry())
    {
        if (type.name == name)
        {
            if (!draw)
                return false;
            type.draw = std::move(draw);
            return true;
        }
    }
    return false;
}

// ---- 绘制钩子按名字取参数 ----
//
// 参数个数 ≤ 8，逐个比名字可以忽略（只有扩展层会调）。类型不对时返回 fallback 而不是
// 断言：参数模式是注册方自己声明的，写错了应该表现为"取到默认值"，而不是绘制期崩掉。

namespace
{

// 找到参数下标；没找到返回 -1
int findParamIndex(const LayerDrawContext& ctx, const char* name)
{
    if (ctx.type == nullptr || name == nullptr)
        return -1;
    for (size_t i = 0; i < ctx.type->params.size(); i++)
        if (ctx.type->params[i].name == name)
            return (int)i;
    return -1;
}

} // namespace

float numberParam(const LayerDrawContext& ctx, const char* name, float fallback)
{
    const int index = findParamIndex(ctx, name);
    if (index < 0 || ctx.type == nullptr || (size_t)index >= kMaxLayerParams)
        return fallback;

    const LayerParam& param = ctx.type->params[(size_t)index];
    if (param.type != LayerParamType::NUMBER && param.type != LayerParamType::PIXELS
        && param.type != LayerParamType::ENUM)
        return fallback;
    return ctx.layer.params[(size_t)index].number;
}

ImU32 colorParam(const LayerDrawContext& ctx, const char* name, ImU32 fallback)
{
    const int index = findParamIndex(ctx, name);
    if (index < 0 || ctx.type == nullptr || (size_t)index >= kMaxLayerParams)
        return fallback;
    if (ctx.type->params[(size_t)index].type != LayerParamType::COLOR)
        return fallback;
    return ctx.layer.params[(size_t)index].color;
}

bool boolParam(const LayerDrawContext& ctx, const char* name, bool fallback)
{
    const int index = findParamIndex(ctx, name);
    if (index < 0 || ctx.type == nullptr || (size_t)index >= kMaxLayerParams)
        return fallback;
    if (ctx.type->params[(size_t)index].type != LayerParamType::BOOL)
        return fallback;
    return ctx.layer.params[(size_t)index].boolean;
}

const char* enumParam(const LayerDrawContext& ctx, const char* name)
{
    const int index = findParamIndex(ctx, name);
    if (index < 0 || ctx.type == nullptr || (size_t)index >= kMaxLayerParams)
        return "";

    const LayerParam& param = ctx.type->params[(size_t)index];
    if (param.type != LayerParamType::ENUM)
        return "";

    const int value = (int)ctx.layer.params[(size_t)index].number;
    if (value < 0 || value >= (int)param.enumValues.size())
        return "";
    return param.enumValues[(size_t)value].c_str();
}

const char* layerOpName(AppearanceLayer::Op op)
{
    switch (op)
    {
        case AppearanceLayer::Op::RECT: return "rect";
        case AppearanceLayer::Op::TEXT: return "text";
        case AppearanceLayer::Op::IMAGE: return "image";
        case AppearanceLayer::Op::SHADOW: return "shadow";
        case AppearanceLayer::Op::GLASS: return "glass";
        case AppearanceLayer::Op::CUSTOM: return "custom";
        case AppearanceLayer::Op::EXTENSION: return "extension"; // 名字在 layer.extensionType
        default: return "rect";
    }
}

const std::vector<LayerType>& registeredLayers()
{
    ensureBuiltins();
    return registry();
}

void clearLayersForTest()
{
    registry().clear();
    // 一并翻回"没注册过"：下一次用到时会重新注册内置层（否则内置 op 会静默失效）
    g_builtinsRegistered = false;
}

int layerSortRank(LayerOrder order)
{
    // 阴影和玻璃有硬性要求（阴影里的半透明整块不能压在玻璃上、聚焦描边要压在内容上），
    // 所以这两档固定；其余档位只用来把**扩展层**排到内置层之后。
    //
    // 内置层之间保持 JSON 书写顺序：实测 168 份发布外观里 121 份把 rect（hlGlow/hlBorder）
    // 写在 text 之后，按"rect 就是 BACKGROUND"重排会让这些描边掉到文字下面。
    switch (order)
    {
        case LayerOrder::SHADOW: return 0;
        case LayerOrder::GLASS: return 1;
        case LayerOrder::DECORATION: return 3;
        case LayerOrder::OVERLAY: return 4;
        // BACKGROUND / HIGHLIGHT_BG / CONTENT 都落在"其余"这一档：保持 JSON 顺序
        default: return 2;
    }
}

void registerBuiltinLayers()
{
    std::string error;
    auto add = [&error](LayerType type) {
        if (!registerLayer(std::move(type), error))
            BKLV_LOG_ERROR("内置图层注册失败：%s", error.c_str());
    };

    // 用工厂而不是聚合初始化：钩子字段保持默认空值，也不会踩 -Wmissing-field-initializers
    auto make = [](const char* name, const char* label, const char* idPrefix, LayerOrder order,
                    const char* help, std::vector<std::string> keys) {
        LayerType type;
        type.name     = name;
        type.label    = label;
        type.idPrefix = idPrefix;
        type.order    = order;
        type.help     = help;
        type.keys     = std::move(keys);
        type.allowReservedKeys = true; // 内置层本来就用这些键
        return type; // parse / draw 留空：①c-3 才接上
    };

    add(make("rect", "矩形", "rect", LayerOrder::BACKGROUND,
        "矩形：填充/描边/圆角/内缩，可带 gradient（渐变）",
        { "fill", "stroke", "strokeWidth", "radius", "opacity", "inset", "width", "height",
            "align", "valign", "gradient" }));
    add(make("text", "文字", "text", LayerOrder::CONTENT,
        "文字：从内容槽取字（source: text/icon/info），可指定字体、颜色、对齐、内缩",
        { "data_id", "color", "fontSize", "align", "valign", "inset", "width", "height", "opacity" }));
    add(make("image", "图片", "image", LayerOrder::CONTENT,
        "图片：source: image + scaling(fit/fill/crop...)",
        { "data_id", "scaling", "tint", "align", "valign", "inset", "width", "height", "radius",
            "opacity" }));
    add(make("shadow", "阴影", "shadow", LayerOrder::SHADOW,
        "柔化投影：fill 浓度 / feather 羽化 / spread 外扩 / offset 位移 / cutout 挖空 / inner 内阴影",
        { "fill", "feather", "spread", "offset", "radius", "cutout", "inner", "inset", "width",
            "height", "opacity" }));
    add(make("glass", "玻璃", "glass", LayerOrder::GLASS,
        "玻璃：fill 是染色（也是后端不可用时的降级底），stroke/strokeWidth 是内缘高光",
        { "fill", "stroke", "strokeWidth", "radius", "opacity", "inset", "width", "height" }));
    add(make("custom", "自定义", "custom", LayerOrder::CONTENT,
        "自定义：语义绘制在 C++ 里（控件的 drawCustomLayer），外观只给几何与底色",
        { "fill", "inset", "width", "height", "opacity" }));
}

} // namespace compile
} // namespace bklv
