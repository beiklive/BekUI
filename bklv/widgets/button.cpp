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
    : AppearanceView(variant.empty() ? "base/button" : "base/button." + variant, true)
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
    //
    // 时间基统一用 getCPUTimeUsec()（steady_clock，自进程启动）：
    // 以前"设置"走 ImGui::GetTime()、"清除"在没有上下文时走一个函数级 static 累加器，
    // 两条时间基不同源（一个 0 起点、一个进程累计），无窗口下 pressed 会被立刻清掉；
    // 而且那个 static 是所有 Button 实例共享的。
    this->setAppearanceState("pressed", true);
    this->pressedUntil = getSecondsNow() + (double)getTheme().number("metric.anim.fast", 90.0f) / 1000.0;

    if (this->onClick)
        this->onClick();
}

void Button::update()
{
    AppearanceView::update();

    if (this->isAppearanceStateActive("pressed") && getSecondsNow() >= this->pressedUntil)
        this->setAppearanceState("pressed", false);
}

void Button::setSwitched(bool on)
{
    if (this->switched == on)
        return;

    this->switched = on;

    // 开关的视觉是外观状态（on / off），过渡动画由主题里的 duration/easing 描述。
    // 两个状态互斥地打开，AppearanceInstance 会按外观补间过去（不是瞬间切换）。
    this->setAppearanceState("off", !on);
    this->setAppearanceState("on", on);
    this->invalidate();
}

namespace
{

void setButtonIconAttr(View* v, const ResolvedValue& r)
{
    static_cast<Button*>(v)->setIcon(r.text);
}

void setButtonIconFontAttr(View* v, const ResolvedValue& r)
{
    Button* button = static_cast<Button*>(v);
    if (enumMatches(r.text, "material"))
        button->setIconFontKind(IconFontKind::MATERIAL);
    else if (enumMatches(r.text, "switch"))
        button->setIconFontKind(IconFontKind::SWITCH);
    else
        warnUnknownEnumValue("iconFont", r.text, "material|switch");
}

} // namespace

const AttributeTable& Button::attributes() const
{
    // Button 自己的 text 语义与基类不同（还要更新子 Label），所以 text 留在这儿；
    // icon / iconFont 与基类同义，照抄表项（AttributeTable 只收 initializer_list，不能拼接）。
    static const AttributeTable table = {
        { "text", AttributeKind::TEXT, setButtonTextAttr, nullptr },
        { "icon", AttributeKind::TEXT, setButtonIconAttr, nullptr },
        { "iconFont", AttributeKind::ENUM, setButtonIconFontAttr, "material|switch" },
        { "variant", AttributeKind::STRING, setButtonVariantAttr, nullptr },
        { "enabled", AttributeKind::BOOL, setButtonEnabledAttr, nullptr },
    };
    return table;
}

} // namespace bklv
