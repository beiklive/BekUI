#include "bklv/ui/widgets/controls/button.hpp"

#include <utility>

#include "bklv/foundation/anim/time.hpp"
#include "bklv/foundation/core/i18n.hpp"
#include "bklv/foundation/core/logger.hpp"
#include "bklv/system/media/audio.hpp"
#include "bklv/ui/input/input.hpp"
#include "bklv/ui/theme/attributes.hpp"
#include "bklv/ui/theme/theme_data.hpp"
#include "bklv/ui/view/json_actions.hpp"

namespace bklv
{

namespace
{

// ---- 属性表的 setter（函数指针，静态表里只存地址，不随实例分配）----

void setButtonTextAttr(View* v, const ResolvedValue& r)
{
    static_cast<Button*>(v)->setText(r.text);
}

void setButtonIconAttr(View* v, const ResolvedValue& r)
{
    static_cast<Button*>(v)->setIcon(r.text);
}

void setButtonVariantAttr(View* v, const ResolvedValue& r)
{
    static_cast<Button*>(v)->setAppearanceVariant(r.text);
}

void setButtonEnabledAttr(View* v, const ResolvedValue& r)
{
    static_cast<Button*>(v)->setEnabled(r.boolean);
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

void setButtonOnClickAttr(View* v, const ResolvedValue& r)
{
    Button* button            = static_cast<Button*>(v);
    const std::string action  = r.text;
    button->setOnClick([button, action] {
        std::string error;
        if (!invokeJsonAction(action, *button, error))
            BKLV_LOG_ERROR("%s", error.c_str());
    });
}

} // namespace

// ---- 基类：只做"按钮都有什么" ----

Button::Button(std::string appearanceName, std::string variant)
    : AppearanceView(std::move(appearanceName), true)
{
    // 点一下就执行（不用先聚焦再按回车），键盘/手柄走 A 键
    this->onPointerAction = true;
    this->registerAction(ControllerButton::A, translate("hints/ok"), [this](View*) { this->trigger(); });

    // 具体形态的默认外观就是"基名 + variant"，由基类统一拼（子类不各写一份路径）
    if (!variant.empty())
        this->setAppearanceVariant(variant);

    this->enableContentMeasurement();
    this->applyTheme();
}

void Button::setText(const std::string& text)
{
    AppearanceView::setText(content::TEXT, text);
}

void Button::setIcon(const std::string& glyph)
{
    AppearanceView::setIcon(content::ICON, glyph);
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
    if (!this->enabled)
        return;

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

// ---- 三个具体形态：只差"用哪份外观 + 填哪几个内容位" ----

TextButton::TextButton(const std::string& text, const std::string& variant)
    : Button("base/button/button", variant)
{
    this->setText(text);
}

const AttributeTable& TextButton::attributes() const
{
    static const AttributeTable table = {
        { "text", AttributeKind::TEXT, setButtonTextAttr, nullptr },
        { "variant", AttributeKind::STRING, setButtonVariantAttr, nullptr },
        { "enabled", AttributeKind::BOOL, setButtonEnabledAttr, nullptr },
        { "onClick", AttributeKind::STRING, setButtonOnClickAttr, nullptr },
    };
    return table;
}

IconButton::IconButton(const std::string& glyph, const std::string& variant)
    : Button("base/button/button", variant)
{
    this->setIcon(glyph);
}

const AttributeTable& IconButton::attributes() const
{
    static const AttributeTable table = {
        { "icon", AttributeKind::TEXT, setButtonIconAttr, nullptr },
        { "iconFont", AttributeKind::ENUM, setButtonIconFontAttr, "material|switch" },
        { "variant", AttributeKind::STRING, setButtonVariantAttr, nullptr },
        { "enabled", AttributeKind::BOOL, setButtonEnabledAttr, nullptr },
        { "onClick", AttributeKind::STRING, setButtonOnClickAttr, nullptr },
    };
    return table;
}

IconTextButton::IconTextButton(const std::string& text, const std::string& glyph, const std::string& variant)
    : Button("base/button/button", variant)
{
    this->setText(text);
    this->setIcon(glyph);
}

const AttributeTable& IconTextButton::attributes() const
{
    static const AttributeTable table = {
        { "text", AttributeKind::TEXT, setButtonTextAttr, nullptr },
        { "icon", AttributeKind::TEXT, setButtonIconAttr, nullptr },
        { "iconFont", AttributeKind::ENUM, setButtonIconFontAttr, "material|switch" },
        { "variant", AttributeKind::STRING, setButtonVariantAttr, nullptr },
        { "enabled", AttributeKind::BOOL, setButtonEnabledAttr, nullptr },
        { "onClick", AttributeKind::STRING, setButtonOnClickAttr, nullptr },
    };
    return table;
}

} // namespace bklv
