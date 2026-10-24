#include "demo_app.hpp"

#include <utility>

#include "bklv/ui/input/input.hpp"
#include "bklv/ui/view/view.hpp"
#include "page.hpp"
#include "pages/home_page.hpp"
#include "pages/settings_page.hpp"
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
    this->root = new bklv::Box(bklv::Axis::COLUMN);

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

    // 底栏：把"能按什么"写出来（bklv 不再自带 hints 栏，宿主自己拼）。
    // Label 没有内边距，所以套一层 Box 做 padding，文字居中靠 Label 自己的对齐。
    auto* footer          = new bklv::Box(bklv::Axis::ROW);
    this->footerLabel     = new bklv::Label();
    footer->setPadding(12.0f, 20.0f, 16.0f, 20.0f);
    this->footerLabel->setText("方向键 / 摇杆移动焦点 · Enter 确认 · Esc 返回 · 鼠标可以直接点");
    this->footerLabel->setFontSize(16.0f);
    this->footerLabel->setHorizontalAlign(bklv::HorizontalAlign::CENTER);
    this->footerLabel->setGrow(1.0f);
    footer->addView(this->footerLabel);

    this->root->addView(header);
    this->root->addView(this->content);
    this->root->addView(footer);

    // 焦点框：绝对定位铺满根视图，加在最后 → 画在所有内容之上。
    // 它自己问输入管理器要当前焦点，不依赖任何控件知道它的存在。
    this->focusRing = new bklv::FocusRingView();
    this->root->addView(this->focusRing);

    // 根视图兜一个 BACK：焦点在内容区时 Esc 也能返回。
    this->root->registerAction(bklv::ControllerButton::BACK, "返回", [this](bklv::View*) { this->goBack(); });

    this->applyThemeColors();
}

void DemoApp::applyThemeColors()
{
    // 底色铺在根视图上（盖住 App 的深色清屏色），文字用主题里配套的深色。
    this->root->setBackgroundColor(pageBackgroundColor());
    this->titleLabel->setTextColor(textColor());
    this->footerLabel->setTextColor(mutedColor());
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
