#include "designer/core/layer_fields.hpp"

#include "bklv/compile/layer_registry.hpp"

namespace designer
{

const std::vector<FieldDesc>& fixedLayerFields()
{
    // 与 compile/appearance_layer_meta.cpp 的通用字段表保持一致（那边加一个键，这里补一条）。
    // 用函数构造而不是聚合初始化：FieldDesc 有默认值，漏写尾字段会触发
    // -Wmissing-field-initializers（项目零警告）。
    static const std::vector<FieldDesc> fields = [] {
        std::vector<FieldDesc> out;
        auto add = [&out](const char* key, FieldKind kind, const char* label, const char* help = "",
                       std::vector<std::string> enumValues = {}) {
            FieldDesc field;
            field.key        = key;
            field.kind       = kind;
            field.label      = label;
            field.help       = help;
            field.enumValues = std::move(enumValues);
            out.push_back(std::move(field));
        };
        add("fill", FieldKind::COLOR, "填充", "底色 / 文字色（rect/text/shadow/glass 用）");
        add("data_id", FieldKind::TEXT, "内容槽",
            "这一层从哪个内容槽取内容（text / icon / info / image，或自定义 id）");
        add("stroke", FieldKind::COLOR, "描边色");
        add("strokeWidth", FieldKind::PIXELS, "描边宽");
        add("radius", FieldKind::PIXELS, "圆角");
        add("inset", FieldKind::INSET, "内缩", "[上,右,下,左]；装饰件靠它定位");
        add("offset", FieldKind::OFFSET, "位移", "[x,y]；阴影/装饰用");
        add("align", FieldKind::ENUM, "水平对齐", "", { "left", "center", "right" });
        add("valign", FieldKind::ENUM, "垂直对齐", "", { "center", "top", "bottom" });
        add("width", FieldKind::PIXELS, "宽", "只给装饰件用；不写=铺满");
        add("height", FieldKind::PIXELS, "高", "只给装饰件用；不写=铺满");
        add("opacity", FieldKind::NUMBER, "不透明度", "0~1 系数");
        add("text", FieldKind::TEXT, "文字", "text 层的内容（也可写 @i18n/）");
        add("fontSize", FieldKind::PIXELS, "字号");
        add("cutout", FieldKind::BOOL, "挖空", "阴影只画外圈");
        add("inner", FieldKind::BOOL, "内阴影", "朝内画（层序保持书写顺序）");
        return out;
    }();
    return fields;
}

namespace
{

FieldKind fromParamType(bklv::LayerParamType type)
{
    using bklv::LayerParamType;
    switch (type)
    {
        case LayerParamType::NUMBER: return FieldKind::NUMBER;
        case LayerParamType::PIXELS: return FieldKind::PIXELS;
        case LayerParamType::COLOR: return FieldKind::COLOR;
        case LayerParamType::BOOL: return FieldKind::BOOL;
        case LayerParamType::ENUM: return FieldKind::ENUM;
    }
    return FieldKind::NUMBER;
}

} // namespace

std::vector<FieldDesc> layerFields(const std::string& op)
{
    std::vector<FieldDesc> fields;
    const bklv::compile::LayerType* type = bklv::compile::findLayer(op);
    if (type == nullptr)
        return fields;

    for (const bklv::compile::LayerParam& param : type->params)
    {
        FieldDesc field;
        field.key                = param.name;
        field.kind               = fromParamType(param.type);
        field.label              = param.label;
        field.help               = param.help;
        field.enumValues         = param.enumValues;
        field.numberDefault      = param.numberDefault;
        field.boolDefault        = param.boolDefault;
        field.colorDefault       = param.colorDefault;
        field.declaredByRegistry = true;
        fields.push_back(std::move(field));
    }

    // 固定字段：跳过参数表里已经出现的同名键（注册表优先）
    for (const FieldDesc& fixed : fixedLayerFields())
    {
        bool exists = false;
        for (const FieldDesc& field : fields)
            exists = exists || field.key == fixed.key;
        if (!exists)
            fields.push_back(fixed);
    }
    return fields;
}

} // namespace designer
