#include "bklv/widgets/bottom_bar.hpp"

#include <string>

#include "bklv/core/attributes.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/theme_data.hpp"

namespace bklv
{

namespace
{

void setBottomBarTextAttr(View* v, const ResolvedValue& r)
{
    static_cast<BottomBar*>(v)->setText(r.text);
}

void setBottomBarVariantAttr(View* v, const ResolvedValue& r)
{
    static_cast<BottomBar*>(v)->setAppearanceVariant(r.text);
}

} // namespace

BottomBar::BottomBar(const std::string& text, const std::string& variant)
    : AppearanceView(variant.empty() ? "bottom_bar" : "bottom_bar." + variant, false)
{
    this->setText(text);
    // 宽高/不压缩来自外观 JSON 的 box 段（widgets/bottom_bar.json）
    this->enableContentMeasurement();

    this->applyTheme();
}

void BottomBar::showHintsFromFocus()
{
    // 复用输入层的提示数据：把当前焦点注册的动作拼成一行
    std::string line;
    for (const HintItem& hint : getInputManager().getHints())
    {
        if (hint.hintText.empty())
            continue;
        if (!line.empty())
            line += "    ";

        line += getControllerButtonName(hint.button);
        line += " ";
        line += hint.hintText;
    }

    this->setText(line);
}

const AttributeTable& BottomBar::attributes() const
{
    static const AttributeTable table = {
        { "text", AttributeKind::Text, setBottomBarTextAttr, nullptr },
        { "variant", AttributeKind::String, setBottomBarVariantAttr, nullptr },
    };
    return table;
}

} // namespace bklv
