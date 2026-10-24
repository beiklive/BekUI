#include "home_page.hpp"

#include <string>

#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/controls/button.hpp"
#include "demo/demo_app.hpp"
#include "demo/style.hpp"

namespace demo
{

bklv::View* HomePage::build(DemoApp& app)
{
    auto* box = new bklv::Box(bklv::Axis::COLUMN);
    box->setGap(14.0f);

    box->addView(makeTitle("BekUI demo"));
    box->addView(makeParagraph("一个最小框架：顶栏 + 内容区 + 底栏，中间是页面栈。"));
    box->addView(makeNote("页面都放在 demo/pages/ 下，每个页面只是拼一棵视图树。"));

    // 三个入口：导航、换肤、通知。
    auto* actions = new bklv::Box(bklv::Axis::ROW);
    actions->setGap(12.0f);
    actions->setPadding(8.0f, 0.0f, 0.0f, 0.0f);

    auto* settingsButton = new bklv::Button("打开设置");
    settingsButton->setOnClick([&app]() { app.openSettings(); });

    auto* themeButton = new bklv::Button("切换主题");
    themeButton->setOnClick([&app]() { app.cycleTheme(); });

    auto* notifyButton = new bklv::Button("发一条通知");
    notifyButton->setOnClick([&app]() { app.app().notify("Hello, BekUI demo"); });

    actions->addView(settingsButton);
    actions->addView(themeButton);
    actions->addView(notifyButton);

    box->addView(actions);

    // 状态行：换肤后 cycleTheme() 会重建当前页，这里就会跟着变。
    box->addView(makeNote(std::string("当前主题：") + app.getThemeName()));

    return box;
}

} // namespace demo
