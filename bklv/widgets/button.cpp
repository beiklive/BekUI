#include "bklv/widgets/button.hpp"

#include <utility>

#include "bklv/core/attributes.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/audio.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/time.hpp"

namespace bklv
{

namespace
{

void setButtonTextAttr(View* v, const ResolvedValue& r)
{
    static_cast<Button*>(v)->setText(r.text);
}

void setButtonVariantAttr(View* v, const ResolvedValue& r)
{
    static_cast<Button*>(v)->setAppearanceVariant(r.text);
}

void setButtonEnabledAttr(View* v, const ResolvedValue& r)
{
    static_cast<Button*>(v)->setEnabled(r.boolean);
}

} // namespace

Button::Button(const std::string& text, const std::string& variant)
    : AppearanceView(variant.empty() ? "button" : "button." + variant, true)
{
    this->setText(text);

    // 点一下就执行（不用先聚焦再按回车），键盘/手柄走 A 键
    this->onPointerAction = true;
    this->registerAction(ControllerButton::A, getStr("hints/ok"), [this](View*) { this->trigger(); });

    this->enableContentMeasurement();
    this->applyTheme();
}

void Button::setEnabled(bool value)
{
    if (this->enabled == value)
        return;

    this->enabled = value;
    this->setAppearanceState("disabled", !value);
}

void Button::trigger()
{
    if (!this->enabled || !this->onClick)
    {
        if (!this->enabled)
            return;
    }

    // 按下反馈：打开 pressed 状态，过一小段时间由 update() 关掉。
    // 外观 JSON 里可以给 pressed 定义颜色/位移等动效。
    this->setAppearanceState("pressed", true);
    const double now = ImGui::GetCurrentContext() != nullptr ? ImGui::GetTime() : 0.0;
    this->pressedUntil = now + (double)getTheme().number("metric.anim.fast", 90.0f) / 1000.0;

    if (this->onClick)
        this->onClick();
}

void Button::update()
{
    AppearanceView::update();

    static double clock = 0.0; // 没有 ImGui 上下文时自己累计（测试用）
    clock += getFrameDeltaSeconds();

    const double now = ImGui::GetCurrentContext() != nullptr ? ImGui::GetTime() : clock;
    if (this->isAppearanceStateActive("pressed") && now >= this->pressedUntil)
        this->setAppearanceState("pressed", false);
}

const AttributeTable& Button::attributes() const
{
    static const AttributeTable table = {
        { "text", AttributeKind::Text, setButtonTextAttr, nullptr },
        { "variant", AttributeKind::String, setButtonVariantAttr, nullptr },
        { "enabled", AttributeKind::Bool, setButtonEnabledAttr, nullptr },
    };
    return table;
}

} // namespace bklv
