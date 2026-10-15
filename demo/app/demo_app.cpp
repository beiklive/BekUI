#include "app/demo_app.hpp"

#include "app/pages.hpp"

#include <algorithm>

#include "app/ui.hpp"
#include "bklv/bklv.hpp"
#include "bklv/core/icon_font.hpp"

using namespace bklv;

namespace demo
{

DemoApp::DemoApp()
{
    // ---- 外壳：Shell 四层 ----
    this->shell = new Shell();
    // 玻璃是 opt-in：这里打开**总开关**，但只有外观里写了 `op: "glass"` 的控件
    // 才吃得到效果 —— 其它控件一个像素都不受影响（基础控件页每个控件都有一行玻璃版本）。
    this->shell->setGlassEnabled(true);

    this->header = new Header();
    // 应用外壳的页头比独立页头高（88，borealis 的 applet_frame/header_height）：
    // 走 header 的 "applet" 变体（另一份 JSON），而不是在代码里改高度
    this->header->setAppearanceVariant("applet");
    this->header->setTitle(getStr("app/title"));
    this->header->setSubtitle(getStr("app/subtitle"));

    this->footer = new BottomBar();
    // 桌面没有电池/无线硬件，状态区默认只显示时钟。这里注入一份"演示数据"，
    // 让底栏右侧的电量/网络图标在桌面上也看得见（Switch 上取的是 psm/nifm 真数据）。
    this->footer->getStatus()->setProvider([] {
        StatusSnapshot snapshot;
        snapshot.batteryPercent = 78;
        snapshot.wifiLevel      = 3;
        return snapshot;
    });

    // 覆盖层是铺满的绝对层：页头在上、底栏在下，中间用 Padding 顶开。
    // 页头/底栏是**纯显示控件**，永不参与焦点（方案 §3.2 的焦点契约）。
    this->shell->setHeaderView(this->header);
    this->shell->getOverlayLayer()->addView(new Padding());
    this->shell->setFooterView(this->footer);

    // ---- 日志窗口：挂在**调试层**上（在 HudLayer 之上，永远最后绘制）----
    // 它读的是 Logger 的环形缓冲（显示全部日志），不是 ImGui 窗口，也不吃输入。
    this->logView = new DebugLogView();
    this->shell->getDebugLayer()->addView(this->logView);

    // ---- 内容：左 侧栏 + 右 子页面 ----
    this->contentRoot = new Box(Axis::COLUMN);
    this->contentRoot->setWidthPercentage(100.0f);
    this->contentRoot->setHeightPercentage(100.0f);
    this->contentRoot->setAlignItems(AlignItems::STRETCH);

    Box* body = makeRow(getTheme().number("metric.spacing.xl", 24.0f), AlignItems::STRETCH);
    body->setHeight(AUTO);
    body->setGrow(1.0f);
    body->setWidth(AUTO); // 有左右 margin 时不能写 100%：会多出边距那一段
    body->setAlignItems(AlignItems::STRETCH);

    this->sidebar = new Sidebar(getStr("app/pages"));
    body->addView(this->sidebar);

    Box* right = makeColumn(getTheme().number("metric.spacing.md", 12.0f));
    right->setGrow(1.0f);
    right->setShrink(1.0f);
    right->setHeight(AUTO); // 交叉轴 stretch 会让它填满 body


    // 滚动槽：Pager 的页面都挂在这里（页面工厂负责现建现删）
    this->scroller = new ScrollingFrame();
    this->scroller->setWidthPercentage(100.0f);
    this->scroller->setGrow(1.0f);
    this->scroller->setShowScrollingIndicator(true);
    right->addView(this->scroller);

    body->addView(right);
    // 内容区左右留白：和页头/底栏的 margin 用同一个主题值，四边看起来才是一套
    const float sideMargin = getTheme().number("metric.appletFrame.paddingSides", 30.0f);
    body->setMargins(0.0f, sideMargin, 0.0f, sideMargin);

    this->contentRoot->addView(body);
    // 基础内容进下钻栈的栈底；详情页往上叠（Shell::pushContentView）
    // 玻璃的背景壁纸：挂在**内容层**（不滚动的父层），并且插在内容之前 ——
    // 页面在滚动器里，壁纸放进页面就会跟着滚（踩过）。内容层的背景是不透明的，
    // 所以铺在 backdrop 层看不见；挂在内容层才既在页面之下、又不随页面滚动。
    this->shell->getContentLayer()->addView(makeGlassWallpaper());
    this->shell->pushContentView(this->contentRoot);

    // 内容避让是**显式**的：内容层按页头/底栏高度留白
    this->applyContentInsets();

    // ---- Pager：括号里的页面懒加载 + 同级换页 ----
    this->pager = new Pager();
    this->pager->setSlot(this->scroller);

    // 页面构建时要能操作外壳（设置页切页头/底栏；图鉴下钻）
    PageDeps deps;
    deps.shell                  = this->shell;
    deps.setHeaderVisible       = [this](bool visible) { this->setHeaderVisible(visible); };
    deps.setFooterVisible       = [this](bool visible) { this->setFooterVisible(visible); };
    deps.setBarsAnimated        = [this](bool animated) { this->setBarsAnimated(animated); };
    deps.setBarAnimationDuration = [this](float seconds) { this->setBarAnimationDuration(seconds); };
    deps.setLogWindowVisible     = [this](bool visible) { this->setLogWindowEnabled(visible); };
    deps.getLogWindowVisible     = [this] { return this->isLogWindowEnabled(); };
    setPageDeps(deps);

    // B 键路由的第 3 级：内容层还有下钻的层 → 退回上一层
    this->shell->setOnBackRequested([this] { return this->handleBackRequest(); });

    // ---- 页面（顺序就是侧栏顺序）----
    this->addPage("theme", getStr("tab/theme"), getStr("page/theme_sub"), false, "palette");
    this->addPage("text", getStr("tab/text"), getStr("page/text_sub"), false, "format_align_left");
    this->addPage("layout", getStr("tab/layout"), getStr("page/layout_sub"), false, "grid_on");
    this->sidebar->addSeparator(); // 分组：示例 / 组件 / 工具
    this->addPage("list", getStr("tab/list"), getStr("page/list_sub"), true, "list");
    this->addPage("pokedex", getStr("tab/pokedex"), getStr("page/pokedex_sub"), true, "favorite");
    this->addPage("json", getStr("tab/json"), getStr("page/json_sub"), false, "description");
    this->addPage("nav", getStr("tab/nav"), getStr("page/nav_sub"), false, "menu");
    this->addPage("overlay", getStr("tab/overlay"), getStr("page/overlay_sub"), false, "layers");
    this->sidebar->addSeparator();
    this->addPage("scroll", getStr("tab/scroll"), getStr("page/scroll_sub"), false, "drag_indicator");
    this->addPage("transform", getStr("tab/transform"), getStr("page/transform_sub"), false, "fullscreen");
    this->addPage("settings", getStr("tab/settings"), getStr("page/settings_sub"), false, "settings");
    this->addPage("shadow", getStr("tab/shadow"), getStr("page/shadow_sub"), false, "brush");
    this->addPage("icons", getStr("tab/icons"), getStr("page/icons_sub"), false, "star");
    this->addPage("base", getStr("tab/base"), getStr("page/base_sub"), false, "home");
    this->addPage("shell", getStr("tab/shell"), getStr("page/shell_sub"), false, "info");

    // 起始页**推迟到第一帧**再选（见 update()）：视图树是在 App::run() 之前建的，
    // 而字体（含两套图标字体）要等 App::run() 建好图集才装载。页面若在这之前构建，
    // 取到的是"字体还没有"的空表（图标总览页会整页空白），所以统一等第一帧再选页。
    this->pendingStartPage = 0;

    // F1 开关日志窗口：面板在**所有层之上**，视觉上会盖住右上角那排按钮
    // （但调试层不吃输入，按钮照样点得到），所以给一个随时能按的快捷键。
    getInputManager().setSystemAction(ImGuiKey_F1, [this] {
        this->setLogWindowEnabled(!this->isLogWindowEnabled());
    });

    BKLV_LOG_INFO("界面已就绪：%d 个页面（Shell + Pager，左侧栏导航）", (int)this->pages.size());
    BKLV_LOG_INFO("图形后端：%s", getGraphicsBackendName());
    BKLV_LOG_INFO("UI 缩放：%.3f（主题尺寸按 1280x720 写）", (double)getUiScale());
}

void DemoApp::setLogWindowEnabled(bool enabled)
{
    if (this->logView == nullptr)
        return;

    this->logView->setEnabled(enabled);
}

bool DemoApp::isLogWindowEnabled() const
{
    return this->logView != nullptr && this->logView->isEnabled();
}

DemoApp::~DemoApp()
{
    // Pager 拥有"当前页"，必须先于视图树析构（页面是 scroller 的孩子）
    delete this->pager;
    this->pager = nullptr;
}

void DemoApp::addPage(const std::string& id, const std::string& name, const std::string& subtitle,
    bool selfScrolling, const char* iconName)
{
    const size_t index = this->pages.size();

    Page page;
    page.id            = id;
    page.name          = name;
    page.subtitle      = subtitle;
    page.selfScrolling = selfScrolling;
    this->pages.push_back(page);

    // 捕获 this + index：DemoApp 活到程序结束，安全；捕获局部下标引用会悬空
    // 聚焦 = 切页（不带内容焦点）；A = 切页并把焦点交给子页面的默认控件
    // 侧栏条目 = 图标 + 文字（图标字形是字形表里的名字，建树期就查得到）
    this->sidebar->addItem(name, icon::glyphByName(iconName),
        [this, index] { this->selectPage(index, false); },
        [this, index] { this->selectPage(index, true); });

    // 工厂：切到这一页时现建（旧页由 Pager 删）
    this->pager->setPageFactory(id, [this, index](PageContext&) -> View* {
        Box* content = this->buildPage(index);
        if (content == nullptr)
            return nullptr;
        return this->wrapPage(content, this->pages[index].selfScrolling);
    });
}

Box* DemoApp::buildPage(size_t index)
{
    switch (index)
    {
        case 0:  return buildThemePage();
        case 1:  return buildTextPage();
        case 2:  return buildLayoutPage();
        case 3:
        {
            ListPageResult listPage = buildListPage();
            this->mainList          = listPage.list;
            return listPage.view;
        }
        case 4:  return buildPokedexPage();
        case 5:  return buildJsonPage();
        case 6:  return buildNavPage();
        case 7:  return buildOverlayPage();
        case 8:  return buildScrollPage();
        case 9: return buildTransformPage();
        case 10: return buildSettingsPage();
        case 11: return buildShadowPage();
        case 12: return buildIconsPage();
        case 13: return buildBaseWidgetsPage();
        case 14: return buildShellPage();
        default: return nullptr;
    }
}

// 内容内边距：borealis 的 tab_frame/content_padding_*（左右 60、上下 42）。
// 以前这层是 AppletFrame 内容区里的 pageSlot，现在由 Pager 的页面外层框承担。
View* DemoApp::wrapPage(Box* page, bool selfScrolling)
{
    Box* frame = new Box(Axis::COLUMN);
    frame->setWidthPercentage(100.0f);
    frame->setAlignItems(AlignItems::STRETCH);
    frame->setPadding(getTheme().number("metric.appletFrame.contentPaddingTopBottom", 42.0f));
    frame->setPaddingLeft(getTheme().number("metric.appletFrame.contentPaddingSides", 60.0f));
    frame->setPaddingRight(getTheme().number("metric.appletFrame.contentPaddingSides", 60.0f));

    if (selfScrolling)
    {
        // 页面自己滚动（内部有列表）：外层框和滚动器等高，列表才会在框里滚。
        // 实际高度由 update() 按滚动器实测高度补上（百分比在两段链上不一定解析出来）。
        frame->setHeight(AUTO);
    }
    else
    {
        // 普通长页：框高 auto，内容多高它就多高 —— 超出的部分由滚动器滚
        frame->setHeight(AUTO);
    }

    frame->addView(page);
    return frame;
}

void DemoApp::selectPage(size_t index, bool focusContent)
{
    if (index >= this->pages.size())
        return;

    if (index == this->currentPage && !focusContent)
        return;

    this->currentPage = index;
    this->sidebar->setSelectedIndex(index);

    // 页头显示当前页
    this->header->setTitle(this->pages[index].name);
    this->header->setSubtitle(this->pages[index].subtitle);

    // 同级换页：Pager 负责 建新页 → 挂上槽 → 交接焦点 → 删旧页
    this->pager->show(this->pages[index].id);

    // 按 A 确认时把焦点交给子页面的默认控件（borealis 的语义）。
    // 侧栏聚焦过来（focusContent=false）时不动焦点 —— 否则侧栏就没法上下移动了，
    // 这条由 Pager 自己判（焦点原本不在页面里就不抢）。
    if (focusContent)
    {
        if (View* first = this->pager->getCurrentView() != nullptr
                ? this->pager->getCurrentView()->getDefaultFocus()
                : nullptr)
            getInputManager().giveFocus(first);
    }
}

// 内容避让：insets 用**目标值**（方案 5.4：动画期间返回目标值，内容只重排一次），
// 真正的"高度平滑变化"发生在页头/底栏自己的折叠补间里。
void DemoApp::applyContentInsets()
{
    if (this->shell == nullptr)
        return;

    const float headerFull = getTheme().number("metric.appletFrame.headerHeight", 88.0f);
    const float footerFull = getTheme().number("metric.appletFrame.footerHeight", 73.0f);

    this->shell->setHeaderInset(this->headerVisible ? headerFull : 0.0f);
    this->shell->setFooterInset(this->footerVisible ? footerFull : 0.0f);

    // 留白加在**内容层**上：这样下钻出来的详情页（同层的兄弟）也自动避让
    Box* content = this->shell->getContentLayer();
    content->setPaddingTop(this->shell->getHeaderInset());
    content->setPaddingBottom(this->shell->getFooterInset());
}

void DemoApp::setHeaderVisible(bool visible)
{
    if (this->headerVisible == visible)
        return;

    this->headerVisible = visible;

    if (this->barsAnimated)
    {
        this->startBarAnimation();
        return;
    }

    this->header->setVisibility(visible ? Visibility::VISIBLE : Visibility::GONE);
    this->applyContentInsets();
    App::requestLayout();
}

void DemoApp::setFooterVisible(bool visible)
{
    if (this->footerVisible == visible)
        return;

    this->footerVisible = visible;

    if (this->barsAnimated)
    {
        this->startBarAnimation();
        return;
    }

    this->footer->setVisibility(visible ? Visibility::VISIBLE : Visibility::GONE);
    this->applyContentInsets();
    App::requestLayout();
}

// 折叠补间：把栏高从整高补到 0（或反过来）。
// 动画期间每帧写一次高度并请求重排——布局是"一帧最多一次"的，所以不会白算。
void DemoApp::startBarAnimation()
{
    const float headerFull = getTheme().number("metric.appletFrame.headerHeight", 88.0f);
    const float footerFull = getTheme().number("metric.appletFrame.footerHeight", 73.0f);

    this->headerAnim.reset(this->headerVisible ? 0.0f : 1.0f);
    this->headerAnim.addStep(this->headerVisible ? 1.0f : 0.0f, this->barDuration);

    this->footerAnim.reset(this->footerVisible ? 0.0f : 1.0f);
    this->footerAnim.addStep(this->footerVisible ? 1.0f : 0.0f, this->barDuration);

    // 动画期间必须是可见的（不然 GONE 了就没有高度可补）
    this->header->setVisibility(Visibility::VISIBLE);
    this->footer->setVisibility(Visibility::VISIBLE);
    this->header->setHeight(std::max(1.0f, headerFull * this->headerAnim.getValue()));
    this->footer->setHeight(std::max(1.0f, footerFull * this->footerAnim.getValue()));

    this->headerAnim.setEndCallback([this](bool) { this->finishBarAnimation(); });
    this->footerAnim.setEndCallback([this](bool) { this->finishBarAnimation(); });

    this->animating = true;
    this->headerAnim.start();
    this->footerAnim.start();

    // 内容避让立刻用目标值（不跟着逐帧改 padding）
    this->applyContentInsets();
    App::requestLayout();
}

void DemoApp::updateBars()
{
    if (!this->animating)
        return;

    const float headerFull = getTheme().number("metric.appletFrame.headerHeight", 88.0f);
    const float footerFull = getTheme().number("metric.appletFrame.footerHeight", 73.0f);

    this->header->setHeight(std::max(1.0f, headerFull * (float)this->headerAnim));
    this->footer->setHeight(std::max(1.0f, footerFull * (float)this->footerAnim));

    App::requestLayout(); // 补间期间每帧都要重排（布局是延迟的，一帧最多一次）
}

// 两段补间都跑完：把该藏的栏真正 GONE 掉
void DemoApp::finishBarAnimation()
{
    if (!this->headerAnim.isStopped() || !this->footerAnim.isStopped())
        return;

    this->animating = false;

    const float headerFull = getTheme().number("metric.appletFrame.headerHeight", 88.0f);
    const float footerFull = getTheme().number("metric.appletFrame.footerHeight", 73.0f);

    this->header->setHeight(headerFull);
    this->footer->setHeight(footerFull);
    this->header->setVisibility(this->headerVisible ? Visibility::VISIBLE : Visibility::GONE);
    this->footer->setVisibility(this->footerVisible ? Visibility::VISIBLE : Visibility::GONE);

    this->applyContentInsets();
    App::requestLayout();
}

bool DemoApp::handleBackRequest()
{
    // 内容层里还有下钻出来的层 → 退回上一层（详情页是临时页，退回时释放）
    if (this->shell != nullptr && this->shell->getContentStackSize() > 1)
    {
        this->shell->popContentView();
        return true;
    }

    return false;
}

void DemoApp::update()
{
    // 首帧才选起始页（原因见构造函数里的注释）
    if (this->pendingStartPage >= 0)
    {
        const size_t page = (size_t)this->pendingStartPage;
        this->pendingStartPage = -1;

        // 焦点落到侧栏对应条目上：侧栏"聚焦即切页"，这一步就把页面切过去了
        if (View* item = this->sidebar->getItemView(page))
            getInputManager().giveFocus(item);
        this->selectPage(page, false);
    }

    // 自管滚动的页（内部有列表）：外层框要和滚动器等高，列表才会在框里滚
    if (this->currentPage < this->pages.size() && this->pages[this->currentPage].selfScrolling)
    {
        const float height = this->scroller->getHeight();
        if (height > 0.0f && this->pager->getCurrentView() != nullptr)
            this->pager->getCurrentView()->setHeight(height);
    }

    // 页面记忆里的滚动位置要等布局算完才落得下去
    this->pager->update();

    this->updateBars();
}

} // namespace demo
