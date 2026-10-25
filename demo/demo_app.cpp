#include "demo_app.hpp"

#include <string>
#include <utility>

#include "bklv/graphics/render/glass/glass_service.hpp"
#include "bklv/ui/input/input.hpp"
#include "bklv/ui/shell/shell.hpp"
#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/basic/label.hpp"
#include "bklv/ui/widgets/container/tab_view.hpp"
#include "bklv/ui/widgets/controls/hint_bar.hpp"
#include "bklv/ui/widgets/controls/tab_bar.hpp"
#include "style.hpp"
#include "tabs/buttons_tab.hpp"
#include "tabs/layout_tab.hpp"

namespace demo
{

namespace
{

// 主题轮换顺序；themes.json 的 available 里是 default / glass。
const char* const kThemes[] = { "default", "glass" };
constexpr int kThemeCount    = static_cast<int>(sizeof(kThemes) / sizeof(kThemes[0]));

// 两个标签页的标题（顶栏显示的就是它）
const char* const kTabTitles[] = { "按钮", "布局" };
constexpr int kTabCount        = static_cast<int>(sizeof(kTabTitles) / sizeof(kTabTitles[0]));

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

    this->tabs = new bklv::TabView();
    this->tabs->addTab("buttons", buildButtonsTab(*this));
    this->tabs->addTab("layout", buildLayoutTab(*this));

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

int DemoApp::run(const std::string& title, int width, int height)
{
    return this->application.run(title, width, height);
}

} // namespace demo
