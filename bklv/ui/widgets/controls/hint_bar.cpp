#include "bklv/ui/widgets/controls/hint_bar.hpp"

#include <cstdint>
#include <string>
#include <utility>

#include "bklv/foundation/core/i18n.hpp"
#include "bklv/system/media/font.hpp"
#include "bklv/ui/input/gesture.hpp"
#include "bklv/ui/view/box.hpp"
#include "bklv/ui/view/view.hpp"

namespace bklv
{

// ---------------------------------------------------------------------------
// Hint（一条提示）
// ---------------------------------------------------------------------------

Hint::Hint()
    : AppearanceView("base/hintitem", false)
{
    // 高度 / 内边距来自 widgets 的 box 段；内容尺寸按文字量。
    this->enableContentMeasurement();

    // 按键字形在 Switch 那套图标字体里（NintendoExt 的私有区码位，和 borealis 一致）。
    // 不指定的话基类默认用 Material 那套，同码位会画出灯泡/书本之类的东西。
    this->setLayerIconFontKind("icon", IconFontKind::SWITCH);

    this->applyTheme();

    // 点击：手势而不是"焦点 + A 动作" —— 后者要求可聚焦，会抢走焦点。
    // 手势在 END 时回调，位移/时长超阈值就不算轻触（见 TapGestureRecognizer）。
    this->addGesture(std::unique_ptr<GestureRecognizer>(
        new TapGestureRecognizer([this] { this->invoke(); })));
}

void Hint::setHint(ControllerButton button, const std::string& text,
    const ActionListener& actionListener, View* actionOwner)
{
    this->button   = button;
    this->label    = text;
    this->listener = actionListener;
    this->owner    = actionOwner;
    this->refreshContent();
}

void Hint::refreshContent()
{
    // 有图标字体就用字形；没有就退回键名 + 空格，免得画出方块。
    if (hasIconFont() && this->button != ControllerButton::NONE)
        this->setText("icon", getControllerButtonIcon(this->button));
    else
        this->clearSlot("icon");

    const std::string text = hasIconFont()
        ? this->label
        : std::string(getControllerButtonName(this->button)) + " " + this->label;
    this->setText("label", text);

    this->markMeasureDirty();
    this->invalidate();
}

void Hint::invoke()
{
    if (this->listener == nullptr)
        return;

    // listener 的签名是 void(View*)：优先传注册动作的那个视图，没登记就传自己。
    this->listener(this->owner != nullptr ? this->owner : this);
}

// ---------------------------------------------------------------------------
// HintBar
// ---------------------------------------------------------------------------

HintBar::HintBar()
    : AppearanceView("base/hintbar", false)
{
    this->applyTheme();
}

void HintBar::refresh()
{
    const std::vector<HintItem> hints = getInputManager().getActiveHints();

    // 焦点没变（内容一样）就不重建：重建会白白触发一次 Yoga 重排。
    std::string signature;
    for (const HintItem& hint : hints)
    {
        if (hint.hintText.empty())
            continue;
        // 签名里必须带**动作主人**的地址：换页 / 换主题后新按钮的提示文字与按键
        // 往往一模一样，只比文字会命中缓存，于是 Hint 留着指向已销毁按钮的 listener，
        // 点一下就是野指针（踩过：hint 触摸随机崩在 listener 调用里）。
        signature += std::to_string((int)hint.button) + ":" + hint.hintText + "@"
            + std::to_string((unsigned long long)(uintptr_t)hint.owner) + "|";
    }

    if (signature == this->lastSignature)
        return;

    this->lastSignature = signature;

    for (Hint* item : this->items)
        this->removeView(item, true);
    this->items.clear();

    for (const HintItem& hint : hints)
    {
        if (hint.hintText.empty())
            continue;

        Hint* item = new Hint();
        item->setHint(hint.button, hint.hintText, hint.listener, hint.owner);
        this->addView(item);
        this->items.push_back(item);
    }

    if (this->hideWhenEmpty)
        this->setVisibility(this->items.empty() ? Visibility::GONE : Visibility::VISIBLE);
}

void HintBar::update()
{
    AppearanceView::update();
    this->refresh();
}

// ---------------------------------------------------------------------------
// BottomBar
// ---------------------------------------------------------------------------

BottomBar::BottomBar()
    : AppearanceView("base/bottombar", false)
{
    this->applyTheme();

    // 提示条是子视图（不是层）：它要按内容量宽度、还要能被点，
    // 层只是绘制回调，建不了子视图。位置靠 bottombar.json 的
    // justifyContent: flexEnd + alignItems: center。
    this->hintBar = new HintBar();
    this->addView(this->hintBar);
}

} // namespace bklv
