#include "bklv/compile/appearance_layer_meta.hpp"

#include "bklv/compile/layer_registry.hpp"
#include "bklv/core/attributes.hpp"
#include "bklv/core/logger.hpp"

namespace bklv
{
namespace compile
{

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

bool parseLayerMeta(const Json& node, size_t index, const ThemeData& theme, AppearanceLayer& out,
    const LayerType*& outType, std::string& error)
{
    const std::string where = "layer"; // 原来用循环里的 where，抽出来后自己提供
    outType                 = nullptr;

    if (!node.is_object())
    {
        error = where + " 必须是对象";
        return false;
    }

    // 没写 id 时用 op + 序号当 id（状态里引用它）
    const std::string opName = node.value("op", "rect");
    bool opOk               = false;
    out.op                = parseOp(opName, opOk);

    // 注册表里的类型：默认 id 前缀、参数模式、绘制上下文都要用它，所以查一次带出去
    outType = compile::findLayer(opName);

    if (out.op == AppearanceLayer::Op::EXTENSION)
    {
        out.extensionType = opName;

        // 扩展 op 自己的**结构**参数（数组/对象）交给注册表里的 parse 钩子。
        // 标量参数不用它：引擎按参数模式自动解析（见 parseDeclaredParams）。
        if (outType != nullptr && outType->parse)
        {
            if (!outType->parse(node, theme, out, error,
                    "layers[" + std::to_string(index) + "] (" + opName + ")"))
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
    const std::string idPrefix = outType != nullptr ? outType->idPrefix : opName;
    out.id     = node.value("id", idPrefix + std::to_string(index));
    // data_id 是正式键；source 是旧键，兼容一版并提示迁移
    if (node.contains("data_id"))
        out.dataId = node.value("data_id", "");
    else if (node.contains("source"))
    {
        out.dataId = node.value("source", "");
        BKLV_LOG_WARNING("图层 \"%s\" 用的是旧键 \"source\"，请改成 \"data_id\"",
            out.id.c_str());
    }
    out.font   = node.value("font", "");
    out.align  = node.value("align", "left");
    out.valign = node.value("valign", "center");
    out.scaling = node.value("scaling", "fit");

    // 这三项拼错时在**编译期**警告一次（绘制期每帧都比一遍，在那儿报会刷屏）
    {
        const std::string alignWhere = where + ".align";
        if (!(enumMatches(out.align, "left") || enumMatches(out.align, "center")
                || enumMatches(out.align, "right")))
            warnUnknownEnumValue(alignWhere.c_str(), out.align, "left|center|right");

        const std::string valignWhere = where + ".valign";
        if (!(enumMatches(out.valign, "center") || enumMatches(out.valign, "top")
                || enumMatches(out.valign, "bottom")))
            warnUnknownEnumValue(valignWhere.c_str(), out.valign, "center|top|bottom");

        const std::string scalingWhere = where + ".scaling";
        if (!enumMatches(out.scaling, "fit"))
            warnUnknownEnumValue(scalingWhere.c_str(), out.scaling,
                "fit（其它缩放由控件自己在 custom 层处理）");
    }

    // 通用字段（颜色/数值字段表 + cutout/inset/offset）：见 compile/appearance_layer_fields.cpp

    return true;
}

} // namespace compile
} // namespace bklv
