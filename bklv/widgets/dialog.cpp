#include "bklv/widgets/dialog.hpp"

#include <utility>

#include "bklv/core/app.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/theme_data.hpp"

namespace bklv
{

DialogBackdrop::DialogBackdrop()
    : AppearanceView("base/dialog_backdrop", false)
{
    this->setPositionType(PositionType::ABSOLUTE);
    this->setPositionTop(0.0f);
    this->setPositionLeft(0.0f);
    this->setWidthPercentage(100.0f);
    this->setHeightPercentage(100.0f);
    this->applyTheme();
}

DialogPanel::DialogPanel()
    : AppearanceView("widget/dialog", false)
{
    this->setWidth(getTheme().number("metric.dialog.width", 520.0f));
    this->setHeight(getTheme().number("metric.dialog.height", 220.0f));
    this->setPadding(getTheme().number("metric.spacing.lg", 16.0f));
    this->setJustifyContent(JustifyContent::FLEX_END);

    // 底部按钮行：右对齐
    this->buttonRow = new Box(Axis::ROW);
    this->buttonRow->setWidthPercentage(100.0f);
    this->buttonRow->setHeight(AUTO);
    this->buttonRow->setGap(getTheme().number("metric.spacing.sm", 8.0f));
    this->buttonRow->setJustifyContent(JustifyContent::FLEX_END);
    this->addView(this->buttonRow);

    this->applyTheme();
}

void DialogPanel::setTitle(const std::string& value)
{
    this->title = value;
    this->markMeasureDirty();
}

void DialogPanel::setText(const std::string& value)
{
    this->text = value;
    this->markMeasureDirty();
}

std::string DialogPanel::getLayerText(const std::string& source)
{
    if (source == "title")
        return this->title;
    if (source == "text")
        return this->text;

    return AppearanceView::getLayerText(source);
}

Dialog::Dialog()
    : Box(Axis::COLUMN)
{
    this->setJustifyContent(JustifyContent::CENTER);
    this->setAlignItems(AlignItems::CENTER);

    this->backdrop = new DialogBackdrop();
    this->addView(this->backdrop);

    this->panel = new DialogPanel();
    // 面板用绝对定位居中：不受容器 padding 影响，也不会把遮罩挤开
    this->panel->setPositionType(PositionType::ABSOLUTE);
    this->panel->setAlignSelf(AlignSelf::CENTER);
    this->addView(this->panel);

    // 默认是关闭外观：透明 + 下移
    this->backdrop->setAppearanceState("closed", true);
    this->panel->setAppearanceState("closed", true);
}

void Dialog::setTitle(const std::string& value)
{
    this->panel->setTitle(value);
}

void Dialog::setText(const std::string& value)
{
    this->panel->setText(value);
}

Button* Dialog::addButton(const std::string& label, std::function<void()> onPress)
{
    Button* button = new Button(label);
    if (onPress)
        button->setOnClick(std::move(onPress));

    this->panel->getButtonRow()->addView(button);
    return button;
}

void Dialog::open()
{
    if (this->opened)
        return;

    App* app = App::getCurrent();
    if (app == nullptr)
    {
        BKLV_LOG_ERROR("Dialog::open 失败：找不到 App 实例");
        return;
    }

    if (!app->pushOverlay(this))
        return;

    this->opened  = true;
    this->closing = false;

    // 从 closed 补到 open：颜色/位移的时长与缓动都在主题 JSON 里
    this->backdrop->setAppearanceState("closed", false);
    this->backdrop->setAppearanceState("open", true);
    this->panel->setAppearanceState("closed", false);
    this->panel->setAppearanceState("open", true);

    // B 键关闭（输入层的模态也会兜底）
    this->registerAction(ControllerButton::B, "关闭", [this](View*) { this->close(); });
}

void Dialog::close()
{
    if (!this->opened || this->closing)
        return;

    this->closing = true;

    // 只切状态，**先不摘弹层**：摘掉之后就脱离视图树了，update() 不再跑，
    // "关掉"的动效根本播不出来。等补间结束再由 update() 摘。
    this->backdrop->setAppearanceState("open", false);
    this->backdrop->setAppearanceState("closed", true);
    this->panel->setAppearanceState("open", false);
    this->panel->setAppearanceState("closed", true);
}

void Dialog::update()
{
    Box::update();

    if (!this->closing)
        return;

    // 关闭补间播完了：现在才把弹层摘掉（焦点也在这时候还给下面的界面），
    // 然后回调调用方。对象本身不销毁，由调用方决定要不要复用/delete。
    if (this->backdrop->getAppearanceInstanceForTest().isAnimating()
        || this->panel->getAppearanceInstanceForTest().isAnimating())
        return;

    this->closing = false;
    this->opened  = false;

    if (App* app = App::getCurrent())
        app->popOverlay(this, false);

    if (this->onClose)
        this->onClose();
}

} // namespace bklv
