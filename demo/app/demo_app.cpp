#include "app/demo_app.hpp"

#include <cstdio>

#include "app/card_button.hpp"
#include "app/pages.hpp"
#include "bklv/core/icon_font.hpp"
#include "bklv/views/preset_backdrop.hpp"

using namespace bklv;
using namespace bklv::i18n; // "key"_i18n 在这个子命名空间里

namespace demo
{

static ToastManager* g_demoToasts = nullptr;

ToastManager* toastManager()
{
    return g_demoToasts;
}

DemoApp::DemoApp()
{
    this->shell = new Shell();
    // this->shell->setGlassEnabled(true);
    // this->shell->setGlassQuality(bklv::glass::GlassQuality::MINIMAL); // 演示默认用快速档
    // 卡片自己有"聚焦抬起 + 强调色描边"，再叠流光焦点框会太吵
    // this->shell->setFocusRingEnabled(false);

    this->shell->setFpsOverlayEnabled(true);

    // 预设背景（挂外壳的背景层，铺满视口）
    this->setBackdropPreset(0); // 实验：浅色背景时不要深色预设

    // Toast：挂在 HUD 层（内容/页头之上），右上角滑入
    this->toasts = new ToastManager();
    g_demoToasts = this->toasts;
    this->toasts->setPositionTop(64.0f); // 避开右上角的帧率覆盖
    this->shell->getHudLayer()->addView(this->toasts);

    this->header = new Header();
    // this->header->setAppearanceVariant("applet");
    this->applyHeaderFor(std::string());
    this->shell->setHeaderView(this->header);
    this->shell->getOverlayLayer()->addView(new Padding());
    const float headerH = getTheme().number("metric.appletFrame.headerHeight", 88.0f);
    this->shell->setHeaderInset(headerH);
    // 内容避让是显式的：不给内容层留白，页面会从 y=0 开始、被页头压住
    this->shell->getContentLayer()->setPaddingTop(headerH);

    this->contentRoot = new Box(Axis::COLUMN);
    this->contentRoot->setWidthPercentage(100.0f);
    this->contentRoot->setHeightPercentage(100.0f);
    this->contentRoot->setAlignItems(AlignItems::STRETCH);

    this->shell->pushContentView(this->contentRoot);

    // B / 返回：内容层还有下钻时退回主页
    this->shell->setOnBackRequested([this] {
        if (this->currentDemo == 0)
            return false;
        this->goBack();
        return true;
    });

    BKLV_LOG_INFO("demo 已就绪：主页 + 3 个演示页");
}

void DemoApp::applyHeaderFor(const std::string& title)
{
    if (this->header == nullptr)
        return;
    this->header->setTitle(title.empty() ? "app/title"_i18n : title);
    this->header->setSubtitle(std::string());
}

void DemoApp::update()
{
    if (!this->built)
    {
        this->built = true;
        this->showHome();
        if (this->pendingOpen > 0)
            this->openDemo(this->pendingOpen);
    }
}

void DemoApp::showHome()
{
    if (this->contentRoot->getChildrenCount() > 0)
        return; // 主页只建一次，返回时复用

    Box* home = new Box(Axis::COLUMN);
    home->setWidthPercentage(100.0f);
    home->setGrow(1.0f);
    home->setGap(getTheme().number("metric.spacing.md", 12.0f));
    home->setAlignItems(AlignItems::CENTER);
    home->setJustifyContent(JustifyContent::CENTER);

    // 三张卡片横排；每张自带浮动动效（绘制期变换，不影响布局）
    home->setGap(getTheme().number("metric.spacing.lg", 24.0f));
    home->setAlignItems(AlignItems::CENTER);

    Box* cards = new Box(Axis::ROW);
    cards->setWidthPercentage(100.0f);
    cards->setGap(getTheme().number("metric.spacing.lg", 24.0f));
    cards->setJustifyContent(JustifyContent::CENTER);
    cards->setAlignItems(AlignItems::CENTER);

    struct Entry
    {
        const char* title;
        const char* subtitle;
        const char* icon;
        int         index;
    };
    const Entry entries[] = {
        { "图标展示", "两套图标字体的全部字形", "star", 1 },
        { "水平 TabFrame", "栏在顶部的分页容器", "menu", 2 },
        { "竖直 TabFrame", "栏在左侧的分页容器", "list", 3 },
        { "按钮展示", "button 外观一览（骨架）", "add", 4 },
    };

    for (const Entry& entry : entries)
    {
        CardButton* card = new CardButton(entry.title, entry.subtitle,
            bklv::icon::glyphByName(entry.icon));
        card->setOnClick([this, index = entry.index] { this->openDemo(index); });
        cards->addView(card);
    }

    home->addView(cards);

    this->contentRoot->addView(home);

    // 默认聚焦第一张卡片（会抬起来，能看到动效）
    if (cards->getChildrenCount() > 0)
        getInputManager().giveFocus(cards->getChildren()[0]);
}

void DemoApp::setBackdropPreset(int preset)
{
    BackdropPreset value = BackdropPreset::GRID;
    switch (preset)
    {
        case 0: value = BackdropPreset::NONE; break;
        case 2: value = BackdropPreset::FLOATING; break;
        case 3: value = BackdropPreset::GRADIENT; break;
        default: value = BackdropPreset::GRID; break;
    }

    if (this->backdrop == nullptr)
    {
        if (value == BackdropPreset::NONE)
            return;
        this->backdrop = new PresetBackdrop(value);
        this->shell->getBackdropLayer()->addView(this->backdrop);
        return;
    }

    if (value == BackdropPreset::NONE)
    {
        this->shell->getBackdropLayer()->removeView(this->backdrop);
        this->backdrop = nullptr;
        return;
    }
    this->backdrop->setPreset(value);
}

void DemoApp::openDemo(int index)
{
    if (index == 1)
        this->openPage("图标展示", [] { return buildIconsPage(); }, 1);
    else if (index == 2)
        this->openPage("水平 TabFrame", [] { return buildHorizontalTabPage(); }, 2);
    else if (index == 3)
        this->openPage("竖直 TabFrame", [] { return buildVerticalTabPage(); }, 3);
    else if (index == 4)
        this->openPage("按钮展示", [] { return buildButtonShowcasePage(); }, 4);
}

Box* DemoApp::wrapPage(Box* content)
{
    Box* frame = new Box(Axis::COLUMN);
    frame->setWidthPercentage(100.0f);
    frame->setHeightPercentage(100.0f);
    frame->setAlignItems(AlignItems::STRETCH);
    frame->setPadding(getTheme().number("metric.appletFrame.contentPaddingTopBottom", 28.0f));
    frame->setPaddingLeft(getTheme().number("metric.appletFrame.contentPaddingSides", 28.0f));
    frame->setPaddingRight(getTheme().number("metric.appletFrame.contentPaddingSides", 28.0f));
    frame->setGap(getTheme().number("metric.spacing.md", 12.0f));

    // 顶部「返回」行
    Box* bar = new Box(Axis::ROW);
    bar->setWidthPercentage(100.0f);
    bar->setMarginTop(10.f);
    Button* back = new Button();
    back->setAppearanceName("base/button/button");
    back->setText("返回"); // 不用 ‹：默认字体里没有这个字形（会显示成问号）
    back->setWidth(180.0f);
    back->setHeight(46.0f);
    back->setOnClick([this] { this->goBack(); });

    bar->addView(new Padding());
    bar->addView(back);

    // 内容（可滚动）
    ScrollingFrame* scroller = new ScrollingFrame();
    scroller->setWidthPercentage(100.0f);
    scroller->setGrow(1.0f);
    scroller->setShowScrollingIndicator(true);
    scroller->setContentView(content); // 必须用 setContentView：addView 只是普通子节点，滚动器不认
    frame->addView(scroller);
    frame->addView(bar);

    return frame;
}

void DemoApp::openPage(const std::string& title, std::function<Box*()> builder, int index)
{
    Box* content = builder();
    if (content == nullptr)
        return;

    this->currentDemo = index;
    this->applyHeaderFor(title);
    this->shell->pushContentView(this->wrapPage(content));
}

void DemoApp::goBack()
{
    if (this->currentDemo == 0)
        return;

    this->currentDemo = 0;
    this->applyHeaderFor(std::string());
    this->shell->popContentView();
}

} // namespace demo
