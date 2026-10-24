#include "bklv/ui/theme/appearance_box.hpp"

#include "bklv/ui/theme/attributes.hpp" // enumMatches / warnUnknownEnumValue
#include "bklv/foundation/json/json_policy.hpp"

namespace bklv
{
namespace compile
{

bool parseBoxNode(const Json& boxNode, const ThemeData& theme, AppearanceBox& box,
    std::string& error, const std::string& where)
{
    if (!boxNode.is_object())
        return true; // 没有 box 段 = 什么都不覆盖（与内联时一致）

    {

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
            const FieldResult result = applyNumberField(node, theme, out, error, where);
            if (result == FieldResult::FAILED)
                return false;
            if (result == FieldResult::SKIPPED)
                return true; // 跳过：out 不动，auto/percent 也不设
            out = scalePixels(out);
            return true;
        };

        auto readEdges = [&](const Json& node, float* edges, const std::string& where) -> bool {
            // 单值（数字或 "@theme/metric.x" 引用）：四边同值
            if (node.is_number() || node.is_string())
            {
                float all = 0.0f;
                const FieldResult result = applyNumberField(node, theme, all, error, where);
                if (result == FieldResult::FAILED)
                    return false;
                if (result == FieldResult::SKIPPED)
                    return true; // 跳过：四边都不写，保留默认
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
                const FieldResult result = applyNumberField(node[i], theme, edges[i], error,
                    where + "[" + std::to_string(i) + "]");
                if (result == FieldResult::FAILED)
                    return false;
                if (result == FieldResult::SKIPPED)
                    continue;
                edges[i] = scalePixels(edges[i]);
            }
            return true;
        };

        auto readNumber = [&](const char* key, float& out, bool& has, const std::string& where) {
            if (!boxNode.contains(key))
                return true;
            const FieldResult result = applyNumberField(boxNode[key], theme, out, error,
                where + "." + key);
            if (result == FieldResult::FAILED)
                return false;
            if (result == FieldResult::SKIPPED)
                return true; // 跳过：has 保持 false，等于这个字段没写
            out = scalePixels(out);
            has = true;
            return true;
        };

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
            const FieldResult result = applyColorField(boxNode["backgroundColor"], theme,
                box.background, error, where + ".backgroundColor");
            if (result == FieldResult::FAILED)
                return false;
            if (result == FieldResult::SKIPPED)
                return true; // 跳过：整段 box 剩下的字段也不再处理（保持默认）
            box.hasBackground = true;
        }
        if (boxNode.contains("borderColor"))
        {
            const FieldResult result = applyColorField(boxNode["borderColor"], theme,
                box.borderColor, error, where + ".borderColor");
            if (result == FieldResult::FAILED)
                return false;
            if (result == FieldResult::SKIPPED)
                return true;
            box.hasBorderColor = true;
        }
    } // 收掉"有 box 段"这一层（切片时它被当成函数收尾吃掉了，补回来）

    return true;
}

} // namespace compile
} // namespace bklv
