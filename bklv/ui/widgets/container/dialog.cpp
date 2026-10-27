#include "bklv/ui/widgets/container/dialog.hpp"

#include <utility>

#include "bklv/ui/app/app.hpp"
#include "bklv/ui/theme/theme_data.hpp"
#include "bklv/ui/input/gesture.hpp"
#include "bklv/ui/input/input.hpp"
#include "bklv/ui/widgets/basic/label.hpp"
#include "bklv/ui/widgets/container/scroll_box.hpp"
#include "bklv/ui/widgets/controls/button.hpp"

using namespace bklv;

Dialog::Dialog()
{
    this->setPositionType(PositionType::ABSOLUTE);
    this->setPositionTop(0.0f);
    this->setPositionRight(0.0f);
    this->setPositionBottom(0.0f);
    this->setPositionLeft(0.0f);
    this->setVisibility(Visibility::GONE); // open() 之前不出现，也不参与布局
    // 面板居中：全屏容器默认会把唯一子节点压在左上角、并沿交叉轴拉伸。
    this->setAxis(Axis::ROW);
    this->setJustifyContent(JustifyContent::CENTER);
    this->setAlignItems(AlignItems::CENTER);

    this->setupStructure();
    this->applyTheme();

    // 点面板之外的地方（也就是遮罩）关闭。
    // 用点击手势而不是 onPointerAction：手势在"按下就被吃掉"这一层生效，
    // 不会顺带执行 A 键动作，也不会把焦点抢走。
    this->addGesture(std::unique_ptr<GestureRecognizer>(new TapGestureRecognizer(
        [this]()
        {
            if (this->closeOnBackdropClick)
                this->close();
        })));
}

Dialog::~Dialog() = default;

void Dialog::setupStructure()
{
    // ---- 遮罩 ----
    this->backdrop = new AppearanceView("base/box/dialog_backdrop", false);
    this->backdrop->setPositionType(PositionType::ABSOLUTE);
    this->backdrop->setPositionTop(0.0f);
    this->backdrop->setPositionRight(0.0f);
    this->backdrop->setPositionBottom(0.0f);
    this->backdrop->setPositionLeft(0.0f);
    this->addView(this->backdrop);

    // ---- 面板 ----
    this->panel = new AppearanceView("base/box/dialog", false);
    // 面板本身要能接住点击：它不可聚焦（焦点在里面的按钮上），但必须挡住
    // "点面板空白处"的点击，否则会冒泡到 Dialog 的轻触手势 → 误关。
    this->panel->setPointerTarget(true);
    this->panel->addGesture(std::unique_ptr<GestureRecognizer>(
        new TapGestureRecognizer([] {})));
    this->addView(this->panel);

    this->body = new Box(Axis::COLUMN);
    this->body->setWidthPercentage(100.0f);
    this->body->setHeightPercentage(100.0f);
    this->panel->addView(this->body);

    // 表头：图标 + 标题
    this->header = new Box(Axis::ROW);
    this->header->setAlignItems(AlignItems::CENTER);
    this->header->setHeight(AUTO);
    this->body->addView(this->header);

    this->iconLabel = new Label();
    this->iconLabel->setIconFontEnabled(true);
    this->iconLabel->setSingleLine(true);
    this->header->addView(this->iconLabel);

    this->titleLabel = new Label();
    this->titleLabel->setSingleLine(true);
    this->header->addView(this->titleLabel);

    // 可滚动内容区：吃掉剩余高度
    this->scroller = new ScrollBox(Axis::COLUMN);
    this->scroller->setGrow(1.0f);
    this->scroller->setFlexBasis(0.0f);
    this->scroller->setShrink(1.0f);
    this->scroller->setShowScrollingIndicator(true);
    this->body->addView(this->scroller);
    this->content = this->scroller->getContentBox();

    // 按钮行
    this->buttonRow = new Box(Axis::ROW);
    this->buttonRow->setWidthPercentage(100.0f);
    this->buttonRow->setHeight(AUTO);
    this->buttonRow->setJustifyContent(JustifyContent::FLEX_END);
    this->buttonRow->setAlignItems(AlignItems::CENTER);
    this->body->addView(this->buttonRow);

    this->updateHeaderVisibility();
}

void Dialog::applyTheme()
{
    Box::applyTheme();

    const ThemeData& theme = getTheme();

    // 面板尺寸：给个默认值，调用方可以再覆盖（setWidth / setMaxHeightPercentage）。
    if (this->panel != nullptr)
    {
        this->panel->setWidth(theme.number("metric.dialog.width", 520.0f));
        this->panel->setMaxHeightPercentage(theme.number("metric.dialog.maxHeightPercent", 85.0f));
    }

    if (this->buttonRow != nullptr)
        this->buttonRow->setGap(theme.number("metric.dialog.buttonGap", 12.0f));

    if (this->content != nullptr)
        this->content->setGap(theme.number("metric.dialog.gap", 16.0f));

    this->refreshHeaderMetrics();
}

void Dialog::refreshHeaderMetrics()
{
    const ThemeData& theme = getTheme();

    if (this->header != nullptr)
        this->header->setGap(theme.number("metric.dialog.iconGap", 10.0f));

    if (this->iconLabel != nullptr)
    {
        this->iconLabel->setFontSize(theme.number("metric.dialog.iconSize", 24.0f));
        this->iconLabel->setTextColor(theme.color("color.dialog.title", IM_COL32(31, 35, 40, 255)));
    }

    if (this->titleLabel != nullptr)
    {
        this->titleLabel->setFontSize(theme.number("metric.dialog.titleSize", 24.0f));
        this->titleLabel->setTextColor(theme.color("color.dialog.title", IM_COL32(31, 35, 40, 255)));
    }
}

void Dialog::updateHeaderVisibility()
{
    if (this->header == nullptr)
        return;

    const bool visible = !this->icon.empty() || !this->title.empty();
    this->header->setVisibility(visible ? Visibility::VISIBLE : Visibility::GONE);

    if (this->iconLabel != nullptr)
        this->iconLabel->setVisibility(this->icon.empty() ? Visibility::GONE : Visibility::VISIBLE);
    if (this->titleLabel != nullptr)
        this->titleLabel->setVisibility(this->title.empty() ? Visibility::GONE : Visibility::VISIBLE);
}

Box* Dialog::getContentBox() const { return this->content; }
Box* Dialog::getButtonRow() const { return this->buttonRow; }
Box* Dialog::getHeaderRow() const { return this->header; }

void Dialog::setIcon(const std::string& glyph)
{
    this->icon = glyph;
    if (this->iconLabel != nullptr)
        this->iconLabel->setText(glyph);
    this->updateHeaderVisibility();
    this->invalidate();
}

void Dialog::setTitle(const std::string& text)
{
    this->title = text;
    if (this->titleLabel != nullptr)
        this->titleLabel->setText(text);
    this->updateHeaderVisibility();
    this->invalidate();
}

TextButton* Dialog::addButton(const std::string& label, std::function<void()> onPress)
{
    auto* button = new TextButton(label, "dialog");

    // 对话框的按钮点完就关窗：先跑调用方的动作（可能没有），再把自己收掉。
    // 这样「取消」这种不传回调的按钮天然就是"关掉"，调用方不用自己接 close()。
    button->setOnClick([this, action = std::move(onPress)] {
        if (action)
            action();
        this->close();
    });
    // 按钮在底部**均匀分配**宽度：每个都 grow=1，两个就各占一半，
    // 只有一个时自然铺满整行（不用调用方自己算宽度）。
    button->setGrow(1.0f);
    this->buttonRow->addView(button);
    this->invalidate();
    return button;
}

void Dialog::clearButtons()
{
    if (this->buttonRow != nullptr)
        this->buttonRow->clearViews();
}

bool Dialog::open()
{
    if (this->opened)
        return true;

    App* app = App::getCurrent();
    if (app == nullptr)
        return false;

    // 先变可见再挂弹层：pushOverlay 里 pushModal 会给模态里的默认焦点
    // （canFocus 要求 isVisibleInTree，GONE 的树一个都选不中——踩过，焦点留在外面）。
    this->opened = true;
    this->setVisibility(Visibility::VISIBLE);
    this->invalidate();

    if (!app->pushOverlay(this, true))
    {
        this->opened = false;
        this->setVisibility(Visibility::GONE);
        return false;
    }

    return true;
}

void Dialog::close(bool freeSelf)
{
    if (!this->opened)
    {
        if (freeSelf)
            delete this;
        return;
    }

    this->opened = false;
    this->setVisibility(Visibility::GONE);

    if (App* app = App::getCurrent())
        app->popOverlay(this, false);

    // 先回调再决定要不要自毁：回调里可能还要读这个对话框（比如拿内容做校验）。
    if (this->onClose)
        this->onClose();

    if (freeSelf)
        delete this;
}

void Dialog::setPanelAppearance(const std::string& appearanceName, const std::string& variant)
{
    if (this->panel == nullptr)
        return;
    this->panel->setAppearanceName(appearanceName);
    this->panel->setAppearanceVariant(variant);
    this->panel->applyTheme();
    this->invalidate();
}

void Dialog::setBackdropAppearance(const std::string& appearanceName, const std::string& variant)
{
    if (this->backdrop == nullptr)
        return;
    this->backdrop->setAppearanceName(appearanceName);
    this->backdrop->setAppearanceVariant(variant);
    this->backdrop->applyTheme();
}
