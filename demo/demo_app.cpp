#include "demo_app.hpp"

#include <string>
#include <utility>

#include "bklv/graphics/render/glass/glass_service.hpp"
#include "bklv/system/media/icon_font.hpp"
#include "bklv/ui/widgets/controls/button.hpp"
#include "bklv/ui/input/input.hpp"
#include "bklv/ui/shell/shell.hpp"
#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/basic/label.hpp"
#include "bklv/ui/widgets/container/tab_view.hpp"
#include "bklv/ui/widgets/controls/hint_bar.hpp"
#include "bklv/ui/widgets/controls/tab_bar.hpp"
#include "style.hpp"
#include "bklv/ui/widgets/container/dialog.hpp"
#include "bklv/ui/widgets/debug/debug_log_view.hpp"
#include "bklv/ui/widgets/debug/fps_overlay.hpp"
#include "bklv/ui/widgets/debug/preset_backdrop.hpp"
#include "bklv/ui/widgets/list/recycler.hpp"
#include "tabs/buttons_tab.hpp"
#include "tabs/container_tab.hpp"
#include "tabs/debug_tab.hpp"
#include "tabs/layout_tab.hpp"
#include "tabs/list_tab.hpp"

namespace demo
{

namespace
{

// 主题轮换顺序；themes.json 的 available 里是 default / glass。
const char* const kThemes[] = { "default", "glass" };
constexpr int kThemeCount    = static_cast<int>(sizeof(kThemes) / sizeof(kThemes[0]));

// 五个标签页的标题（顶栏显示的就是它）
const char* const kTabTitles[] = { "按钮", "布局", "列表", "容器", "调试" };
constexpr int kTabCount        = static_cast<int>(sizeof(kTabTitles) / sizeof(kTabTitles[0]));

// 一个"有 count 条"的最简数据源：列表只关心条数，数据在 cell 里按 index 现算。
class CountSource : public bklv::RecyclerDataSource
{
  public:
    CountSource(size_t count, std::string identifier, float itemHeight)
        : count(count), identifier(std::move(identifier)), itemHeight(itemHeight)
    { }

    size_t getItemCount() override { return this->count; }

    // 每种一眼对应一种 cell 类型：RecyclerFrame 用它去 registerCell 里查工厂
    std::string getItemIdentifier(size_t) override { return this->identifier; }

    // 行高由数据源给（不覆盖的话默认 0，2000 行会塌成零高）
    float getItemHeight(size_t) override { return this->itemHeight; }

  private:
    size_t count = 0;
    std::string identifier;
    float itemHeight = 0.0f;
};

} // namespace

DemoApp::DemoApp()
{
    // 玻璃（高斯模糊）默认关：库不主动改变宿主观感，宿主自己开。
    // 对话框的遮罩就是一块玻璃，所以 demo 里开着，换主题时也能看到模糊。
    bklv::glass::getGlassService().setEnabled(true);

    this->buildShell();
    this->application.setRootView(this->root);
}

DemoApp::~DemoApp() = default;

void DemoApp::buildShell()
{
    // 外壳自带五层：backdrop（页底色）/ content（顶栏 + 标签区）/ overlay（底栏）/
    // hud（焦点框 + 通知）/ debug。demo 只用前四层。
    this->root = new bklv::Shell();

    // ---- 顶栏：只有标题（不再有"返回"按钮，主体是标签页）----
    auto* header = new bklv::Box(bklv::Axis::ROW);
    header->setAlignItems(bklv::AlignItems::CENTER);
    header->setPadding(20.0f, 20.0f, 10.0f, 20.0f);

    this->titleLabel = new bklv::Label();
    this->titleLabel->setGrow(1.0f);
    this->titleLabel->setFontSize(24.0f);
    this->titleLabel->setVerticalAlign(bklv::VerticalAlign::CENTER);
    header->addView(this->titleLabel);

    // ---- 标签区：标签条在上/左，标签页内容占剩余空间 ----
    this->tabLayout = new bklv::Box(bklv::Axis::COLUMN);
    this->tabLayout->setGrow(1.0f);

    this->tabBar = new bklv::TabBar(bklv::TabBar::Orientation::HORIZONTAL);
    this->tabBar->addItem(kTabTitles[0], "\uE001", "buttons");
    this->tabBar->addItem(kTabTitles[1], "\uE002", "layout");
    this->tabBar->addItem(kTabTitles[2], "\uE003", "list");
    this->tabBar->addItem(kTabTitles[3], "\uE004", "container");
    this->tabBar->addItem(kTabTitles[4], "\uE005", "debug");

    this->tabs = new bklv::TabView();
    this->tabs->addTab("buttons", buildButtonsTab(*this));
    this->tabs->addTab("layout", buildLayoutTab(*this));
    this->tabs->addTab("list", buildListTab(*this));
    this->tabs->addTab("container", buildContainerTab(*this));
    this->tabs->addTab("debug", buildDebugTab(*this));

    // 双向同步：点/按确认切标签 → TabView 生效；TabView 被代码切 → 条与标题跟着变
    this->tabBar->setOnTabSelected([this](int index) { this->tabs->setActiveTab(index); });
    this->tabs->setOnTabChanged([this](int index, bklv::View*) {
        this->tabBar->setActiveIndex(index, false);
        this->titleLabel->setText(index >= 0 && index < kTabCount ? kTabTitles[index] : "");
    });

    const int start = this->initialTab >= 0 && this->initialTab < kTabCount ? this->initialTab : 0;
    this->tabs->setActiveTab(start);
    this->tabBar->setActiveIndex(start, false);
    this->titleLabel->setText(kTabTitles[start]);

    this->tabLayout->addView(this->tabBar);
    this->tabLayout->addView(this->tabs);

    this->root->getContentLayer()->addView(header);
    this->root->getContentLayer()->addView(this->tabLayout);

    // ---- 底栏：提示条挂覆盖层、贴底；内容层下边留出同样高度 ----
    this->bottomBar = new bklv::BottomBar();
    this->bottomBar->setPositionType(bklv::PositionType::ABSOLUTE);
    this->bottomBar->setPositionBottom(0.0f);
    this->root->getOverlayLayer()->addView(this->bottomBar);

    // 内容层给底栏让出高度（数值跟 base/hint/bottombar.json 的 box.height 一致）
    this->root->getContentLayer()->setPadding(0.0f, 0.0f, 100.0f, 0.0f);

    // 焦点框归 Shell（挂在浮层，永远画在内容/覆盖层之上）
    this->root->setFocusRingEnabled(true);

    // 内容层下钻（ContentStack）的转场：两边都滑入淡出
    this->root->setEnterTransition(bklv::ContentTransition::SLIDE_FADE);
    this->root->setExitTransition(bklv::ContentTransition::SLIDE_FADE);
    this->root->setTransitionDuration(0.22f);

    // 根视图兜一个 BACK：先关内容层的下钻层（demo 现在没用，留着给以后）
    this->root->setOnBackRequested([this]() -> bool {
        if (this->root != nullptr && this->root->getContentStackSize() > 0)
        {
            this->root->popContentView();
            return true;
        }
        return false;
    });

    if (this->initialVertical)
        this->setTabOrientation(true);

    this->buildDebugWidgets();
    this->applyThemeColors();
}

void DemoApp::applyThemeColors()
{
    // 底色铺在背景层上（盖住 App 的深色清屏色），文字用主题里配套的深色。
    this->root->getBackdropLayer()->setBackgroundColor(pageBackgroundColor());
    this->titleLabel->setTextColor(textColor());
}

void DemoApp::setTabOrientation(bool vertical)
{
    if (this->tabBar == nullptr || this->tabLayout == nullptr)
        return;

    this->tabVertical = vertical;
    this->tabBar->setOrientation(vertical ? bklv::TabBar::Orientation::VERTICAL
                                          : bklv::TabBar::Orientation::HORIZONTAL);
    this->tabLayout->setAxis(vertical ? bklv::Axis::ROW : bklv::Axis::COLUMN);

    // 竖直排列时标签条要跟内容一样高（ROW 主轴下它属于交叉轴，靠 AlignSelf 撑开；
    // 光靠 JSON 里的 height: 100% 会在"父高由子决定"的那一趟里算成内容高度）。
    this->tabBar->setAlignSelf(vertical ? bklv::AlignSelf::STRETCH : bklv::AlignSelf::AUTO);
    this->tabLayout->setAlignItems(vertical ? bklv::AlignItems::STRETCH : bklv::AlignItems::FLEX_START);

    // 已知不足：竖直排列时标签条只长到"内容高度"（tabular 的高度来自 TabView，
    // 而 TabView 在 ROW 交叉轴上还没有确定高度），所以它下方的白色面板不会铺到底。
    // 要修得让 TabView 在 ROW 里也拿一个确定高度，属控件侧的事，demo 先这样。

    this->tabLayout->invalidate();
}

void DemoApp::setInitialTab(int index, bool vertical)
{
    this->initialTab      = index;
    this->initialVertical = vertical;

    // 通常是在构造之后、run() 之前调用：外壳已经搭好，就地生效，
    // 否则窗口起来的第一帧还会停在默认标签页/默认排列上。
    if (this->tabs != nullptr && this->tabBar != nullptr)
    {
        const int start = index >= 0 && index < kTabCount ? index : 0;
        this->tabs->setActiveTab(start);
        this->tabBar->setActiveIndex(start, false);
        if (this->titleLabel != nullptr)
            this->titleLabel->setText(kTabTitles[start]);

        // 焦点落在"当前选中的那个标签"上：不然首帧焦点会落到第一个可聚焦控件
        // （标签条的第一项），出现"选中在布局、焦点框在按钮"的错位。
        if (bklv::TabItem* item = this->tabBar->getItem(start))
            bklv::getInputManager().giveFocus(item);
    }

    if (this->tabBar != nullptr)
        this->setTabOrientation(vertical);
}

bool DemoApp::setTheme(const std::string& name)
{
    if (!this->application.setTheme(name))
        return false;

    this->themeName = name;

    if (this->root != nullptr)
        this->applyThemeColors();

    return true;
}

void DemoApp::cycleTheme()
{
    int index = 0;
    for (int i = 0; i < kThemeCount; ++i)
    {
        if (this->themeName == kThemes[i])
        {
            index = i;
            break;
        }
    }

    const char* next = kThemes[(index + 1) % kThemeCount];
    if (!this->setTheme(next))
    {
        this->application.notify(std::string("主题打不开：") + next);
        return;
    }

    this->application.notify(std::string("主题：") + next);
}

// ---- 通知的两种形态 + 连发 -------------------------------------------------

void DemoApp::notifyImage(const std::string& text)
{
    this->application.notifyImage(text, "resources/testcard.png");
}

void DemoApp::notifyBurst(int count)
{
    // 时长错开（4~8 秒），方便看清"到期的先收回、下面的往上挪"
    for (int i = 0; i < count; ++i)
    {
        const float duration = 4.0f + (float)i;
        if (i % 2 == 0)
            this->application.notifyImage("图片通知 #" + std::to_string(i + 1), "resources/testcard.png", duration);
        else
            this->application.notifyIcon("图标通知 #" + std::to_string(i + 1), bklv::icon::glyph(bklv::icon::Id::INFO),
                duration);
    }
}

// ---- 列表页的数据源与下钻 -------------------------------------------------

bklv::RecyclerDataSource* DemoApp::getLongListSource()
{
    if (this->longListSource == nullptr)
        this->longListSource = std::make_unique<CountSource>(2000, "row", 72.0f);
    return this->longListSource.get();
}

bklv::RecyclerDataSource* DemoApp::getGridSource()
{
    if (this->gridSource == nullptr)
        this->gridSource = std::make_unique<CountSource>(400, "tile", 96.0f);
    return this->gridSource.get();
}

void DemoApp::openListDetail(size_t index)
{
    auto* page = new bklv::Box(bklv::Axis::COLUMN);
    page->setGap(16.0f);
    page->setPadding(24.0f);
    page->setBackgroundColor(pageBackgroundColor());

    page->addView(makeTitle("第 " + std::to_string(index + 1) + " 条 · 详情页"));
    page->addView(makeParagraph("这一页是往内容层压进来的（Shell::pushContentView），"
                                "不是标签页里的一个分支。"));
    page->addView(makeNote("入场是滑动淡入；返回时上一层会恢复它自己的滚动位置和焦点 —— "
                           "因为 ContentStack 记了每一层的滚动值与焦点路径。"));

    auto* back = new bklv::TextButton("返回（或按 B）");
    back->setOnClick([this] {
        if (this->root != nullptr)
            this->root->popContentView();
    });
    page->addView(back);

    this->root->fillLayer(page);
    this->root->pushContentView(page, "list-detail");
}

void DemoApp::openGridDetail(size_t index)
{
    auto* page = new bklv::Box(bklv::Axis::COLUMN);
    page->setGap(16.0f);
    page->setPadding(24.0f);
    page->setBackgroundColor(pageBackgroundColor());

    page->addView(makeTitle("格 " + std::to_string(index + 1)));
    page->addView(makeNote("网格也是同一个 RecyclerFrame，只是把 columns 设成 3。"));
    page->addView(makeNote("点这里等于点一个格子：详情页同样走内容层下钻。"));

    auto* back = new bklv::TextButton("返回");
    back->setOnClick([this] {
        if (this->root != nullptr)
            this->root->popContentView();
    });
    page->addView(back);

    this->root->fillLayer(page);
    this->root->pushContentView(page, "grid-detail");
}

void DemoApp::openDemoDialog()
{
    // 只建一次：关闭只是把对话框从弹层上摘下来（GONE），再开还是同一个对象。
    if (this->demoDialog == nullptr)
    {
        auto* dialog = new bklv::Dialog();
        dialog->setIcon(bklv::icon::glyph(bklv::icon::Id::WARNING));
        dialog->setTitle("删除这一项？");

        auto* body = dialog->getContentBox();
        body->setGap(10.0f);
        body->addView(makeParagraph("这条消息会被永久删除，无法恢复。"));
        body->addView(makeNote("面板与遮罩都是外观：base/box/dialog 与 base/box/dialog_backdrop。"));
        body->addView(makeNote("打开时自动把焦点拉进面板；B / Esc 关闭；底部按钮均分宽度。"));

        // 两个按钮点下去都会自动关窗（Dialog::addButton 的约定），
        // 这里只需要写"关掉之前要做什么"。
        dialog->addButton("取消");
        dialog->addButton("删除", [this] { this->notify("已删除（示例）"); });

        this->demoDialog = dialog;
    }

    this->demoDialog->open();
}

// ---- 调试页 ---------------------------------------------------------------

void DemoApp::buildDebugWidgets()
{
    if (this->root == nullptr)
        return;

    // 按页背景：铺在背景层最底下（它自己画网格/漂浮/渐变）
    this->backdrop = new bklv::PresetBackdrop(bklv::BackdropPreset::NONE);
    this->backdrop->setPositionType(bklv::PositionType::ABSOLUTE);
    this->backdrop->setPositionTop(0.0f);
    this->backdrop->setPositionRight(0.0f);
    this->backdrop->setPositionBottom(0.0f);
    this->backdrop->setPositionLeft(0.0f);
    this->root->getBackdropLayer()->addView(this->backdrop);

    // 应用内日志：挂调试层，默认隐藏
    this->logView = new bklv::DebugLogView();
    this->logView->setEnabled(false);
    this->root->getDebugLayer()->addView(this->logView);

    // FPS 浮层：挂 HUD，默认隐藏
    this->fpsOverlay = new bklv::FpsOverlay();
    this->fpsOverlay->setVisibility(bklv::Visibility::GONE);
    this->root->getHudLayer()->addView(this->fpsOverlay);
}

void DemoApp::toggleLogView()
{
    if (this->logView != nullptr)
        this->logView->toggle();
}

void DemoApp::toggleFpsOverlay()
{
    if (this->fpsOverlay == nullptr)
        return;

    const bool visible = this->fpsOverlay->getVisibility() == bklv::Visibility::VISIBLE;
    this->fpsOverlay->setVisibility(visible ? bklv::Visibility::GONE : bklv::Visibility::VISIBLE);
}

void DemoApp::toggleDebugLayer()
{
    this->application.setDebugLayerEnabled(!this->application.isDebugLayerEnabled());
}

void DemoApp::cycleBackdrop()
{
    static const bklv::BackdropPreset presets[] = { bklv::BackdropPreset::NONE, bklv::BackdropPreset::GRID,
        bklv::BackdropPreset::FLOATING, bklv::BackdropPreset::GRADIENT };

    this->backdropIndex = (this->backdropIndex + 1) % 4;
    if (this->backdrop != nullptr)
        this->backdrop->setPreset(presets[this->backdropIndex]);
}

void DemoApp::cycleSafeArea()
{
    static const float steps[] = { 0.0f, 24.0f, 48.0f };

    this->safeAreaIndex = (this->safeAreaIndex + 1) % 3;
    const float value   = steps[this->safeAreaIndex];
    if (this->root != nullptr)
        this->root->setSafeAreaInsets(value, value, value, value);
}

void DemoApp::cycleGlass()
{
    static const bklv::glass::GlassQuality qualities[] = {
        bklv::glass::GlassQuality::BALANCED,
        bklv::glass::GlassQuality::FULL,
        bklv::glass::GlassQuality::MINIMAL,
    };

    this->glassIndex = (this->glassIndex + 1) % 4;
    if (this->root == nullptr)
        return;

    if (this->glassIndex == 0)
    {
        this->root->setGlassEnabled(false);
        return;
    }

    this->root->setGlassEnabled(true);
    this->root->setGlassQuality(qualities[this->glassIndex - 1]);
}

std::string DemoApp::describeDebugState() const
{
    static const char* const kBackdrops[] = { "关", "网格", "漂浮", "渐变" };
    static const char* const kGlass[]     = { "关", "BALANCED", "FULL", "MINIMAL" };
    static const char* const kSafeArea[]  = { "0", "24", "48" };

    const bool logOn = this->logView != nullptr && this->logView->isEnabled();
    const bool fpsOn = this->fpsOverlay != nullptr && this->fpsOverlay->getVisibility() == bklv::Visibility::VISIBLE;
    const bool boxOn = this->application.isDebugLayerEnabled();

    std::string out = "日志 ";
    out += logOn ? "开" : "关";
    out += " · FPS ";
    out += fpsOn ? "开" : "关";
    out += " · 调试边框 ";
    out += boxOn ? "开" : "关";
    out += " · 背景 ";
    out += kBackdrops[this->backdropIndex % 4];
    out += " · 安全区 ";
    out += kSafeArea[this->safeAreaIndex % 3];
    out += " · 玻璃 ";
    out += kGlass[this->glassIndex % 4];
    return out;
}
int DemoApp::run(const std::string& title, int width, int height)
{
    return this->application.run(title, width, height);
}

} // namespace demo
