#include "demo_app.hpp"

#include <string>
#include <utility>

#include "bklv/graphics/render/glass/glass_service.hpp"
#include "bklv/ui/input/input.hpp"
#include "bklv/ui/view/view.hpp"
#include "bklv/ui/widgets/basic/label.hpp"
#include "bklv/ui/widgets/container/list_box.hpp"
#include "bklv/ui/widgets/controls/button.hpp"
#include "page.hpp"
#include "pages/home_page.hpp"
#include "pages/settings_page.hpp"
#include "pages/tabs_page.hpp"
#include "style.hpp"

namespace demo
{

namespace
{

// 主题轮换顺序；themes.json 的 available 里是 default / glass。
const char* const kThemes[] = { "default", "glass" };
constexpr int kThemeCount    = static_cast<int>(sizeof(kThemes) / sizeof(kThemes[0]));

} // namespace

DemoApp::DemoApp()
{
    // 玻璃（高斯模糊）默认是关的：库不主动改变宿主观感，宿主自己开。
    // 对话框的遮罩就是一块玻璃，所以 demo 里打开它，切换主题时也能看到模糊。
    bklv::glass::getGlassService().setEnabled(true);

    this->buildShell();
    this->application.setRootView(this->root);

    // 导航动作在下一帧开头执行：回调里那棵子树还活着，等这一帧画完再换。
    // 用队列而不是单个槽：启动时可能连着压两页（首页 + --page 指定的页面）。
    this->application.onUpdate = [this]() {
        if (this->pendingNavigation.empty())
            return;
        std::vector<std::function<void()>> actions;
        actions.swap(this->pendingNavigation);
        for (auto& action : actions)
            action();
    };

    this->pushPage(new HomePage());
    this->showCurrentPage();
}

DemoApp::~DemoApp() = default;

void DemoApp::buildShell()
{
    // 外壳自带五层（背景 / 内容 / 覆盖 / 浮层 / 调试）。demo 只用到其中三层：
    //   backdropLayer  页底色
    //   contentLayer   顶栏 + 内容区 + 底栏（它本身就是竖直排布，布局与以前一致）
    //   hudLayer       焦点框（Shell 建好就挂好了，不用自己加）
    this->root = new bklv::Shell();

    // 顶栏：返回 + 标题。
    auto* header = new bklv::Box(bklv::Axis::ROW);
    header->setAlignItems(bklv::AlignItems::CENTER);
    header->setGap(16.0f);
    header->setPadding(20.0f, 20.0f, 10.0f, 20.0f);

    this->backButton = new bklv::TextButton("< 返回");
    this->backButton->setOnClick([this]() { this->goBack(); });
    // 手柄 / 键盘的 BACK（Esc）走同一条路：焦点停在顶栏按钮上时按键直达。
    this->backButton->registerAction(bklv::ControllerButton::BACK, "返回", [this](bklv::View*) { this->goBack(); });
    this->backButton->setVisibility(bklv::Visibility::GONE);

    this->titleLabel = new bklv::Label();
    this->titleLabel->setGrow(1.0f);
    this->titleLabel->setFontSize(24.0f);
    this->titleLabel->setVerticalAlign(bklv::VerticalAlign::CENTER);

    header->addView(this->backButton);
    header->addView(this->titleLabel);

    // 内容区：每个页面自己往里长，占满剩余高度。
    this->content = new bklv::Box(bklv::Axis::COLUMN);
    this->content->setGrow(1.0f);
    this->content->setPadding(20.0f);
    this->content->setClipsToBounds(true);

    // 底栏：bklv 自带的 BottomBar（覆盖层上的一条 100px 横条 + 右侧提示）。
    // 提示内容来自 currentFocus 沿父链注册的动作，宿主不用自己拼字符串。
    this->bottomBar = new bklv::BottomBar();
    // 覆盖层是铺满外壳的普通容器，里面的东西默认从顶往下摆；
    // 底栏要贴底，所以它自己绝对定位到左下角（高度仍由 JSON 的 box.height 给）。
    this->bottomBar->setPositionType(bklv::PositionType::ABSOLUTE);
    this->bottomBar->setPositionBottom(0.0f);

    // 内容层是竖直排布的层容器，直接往里放这两段，布局与"根 Box 一列"时一致。
    this->root->getContentLayer()->addView(header);
    this->root->getContentLayer()->addView(this->content);

    // 底栏挂在覆盖层（overlay）上：它不吃方向键焦点，但会盖住内容层底部，
    // 所以内容层要让出同样的高度（数值要跟 base/hint/bottombar.json 的 box.height 一致）。
    this->root->getOverlayLayer()->addView(this->bottomBar);
    this->root->getContentLayer()->setPadding(0.0f, 0.0f, 100.0f, 0.0f);

    // 焦点框归 Shell 所有（挂在浮层上，永远画在内容/覆盖层之上），这里只确认它开着。
    this->root->setFocusRingEnabled(true);

    // 内容层层级栈的入场/出场：两边都滑入淡出，0.22 秒（外观里写了 transitions 时以 JSON 为准）。
    this->root->setEnterTransition(bklv::ContentTransition::SLIDE_FADE);
    this->root->setExitTransition(bklv::ContentTransition::SLIDE_FADE);
    this->root->setTransitionDuration(0.22f);

    // 根视图兜一个 BACK：先关内容层的下钻层，再退回上一页；
    // 模态子树与内容树都没人处理 B 时才会冒泡到这里。
    this->root->setOnBackRequested([this]() -> bool {
        if (this->root != nullptr && this->root->getContentStackSize() > 0)
        {
            this->root->popContentView();
            return true;
        }
        if (this->pages.size() > 1)
        {
            this->goBack();
            return true;
        }
        return false;
    });

    this->applyThemeColors();
}

void DemoApp::applyThemeColors()
{
    // 底色铺在背景层上（盖住 App 的深色清屏色），文字用主题里配套的深色。
    // 铺背景层而不是外壳本身：背景层永远在最底下，页头/弹窗不会把它当底色刷掉。
    this->root->getBackdropLayer()->setBackgroundColor(pageBackgroundColor());
    this->titleLabel->setTextColor(textColor());
}

Page* DemoApp::currentPage() const
{
    return this->pages.empty() ? nullptr : this->pages.back().get();
}

void DemoApp::defer(std::function<void()> action)
{
    // 排进队列，帧末统一执行；同帧多次导航按插入顺序依次生效。
    this->pendingNavigation.push_back(std::move(action));
}

void DemoApp::pushPage(Page* page)
{
    if (page == nullptr)
        return;

    this->defer([this, page]() {
        this->pages.emplace_back(page);
        this->showCurrentPage();
    });
}

void DemoApp::goBack()
{
    if (this->pages.size() <= 1)
        return;

    this->defer([this]() {
        if (this->pages.size() <= 1)
            return;

        this->pages.pop_back();
        this->showCurrentPage();
    });
}

void DemoApp::openTabs(bool vertical)
{
    auto* page = new TabsPage();
    page->setVertical(vertical);
    this->pushPage(page);
}

void DemoApp::openSettings()
{
    this->pushPage(new SettingsPage());
}

void DemoApp::showCurrentPage()
{
    // clearViews() 会连同上一页的整棵子树一起释放，页面每次进入都重新 build。
    this->content->clearViews();

    if (Page* page = this->currentPage())
    {
        if (bklv::View* view = page->build(*this))
            this->content->addView(view);
    }

    this->refreshHeader();

    // 重新装页之后焦点要落在新页面上：`App::setRootView()` 只在换根那一刻找一次默认焦点，
    // 而这里是在第一帧（甚至每次换页）才把页面内容塞进树里，那次查找拿到的是 nullptr。
    // 没有焦点就没有焦点框，方向键也无从下手 —— 所以补一次（giveFocus 默认静音）。
    if (bklv::getInputManager().getCurrentFocus() == nullptr && this->root != nullptr)
        bklv::getInputManager().giveFocus(this->root->getDefaultFocus());
}

void DemoApp::refreshHeader()
{
    Page* page = this->currentPage();
    this->titleLabel->setText(page != nullptr ? page->getTitle() : "");
    // 只有一页时没有"上一页"，返回按钮不占位。
    this->backButton->setVisibility(this->pages.size() > 1 ? bklv::Visibility::VISIBLE : bklv::Visibility::GONE);
}

bool DemoApp::setTheme(const std::string& name)
{
    if (!this->application.setTheme(name))
        return false;

    this->themeName = name;
    // 底色与顶栏 / 底栏文字跟着主题走；页面自己的文字由 showCurrentPage() 重建时取新色。
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

    // 外观换过之后重建当前页，页面里写死的状态文字（比如"当前主题"）才会更新。
    this->defer([this]() { this->showCurrentPage(); });
    this->application.notify(std::string("主题：") + next);
}

int DemoApp::run(const std::string& title, int width, int height)
{
    return this->application.run(title, width, height);
}

} // namespace demo
