#include "settings_page.hpp"

#include <string>

#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/controls/button.hpp"
#include "demo/demo_app.hpp"
#include "demo/style.hpp"

namespace demo
{

bklv::View* SettingsPage::build(DemoApp& app)
{
    auto* box = new bklv::Box(bklv::Axis::COLUMN);
    box->setGap(14.0f);

    box->addView(makeTitle("设置"));
    box->addView(makeParagraph("这一页是压栈进来的：顶栏出现「返回」，按 Esc 或点它都会出栈。"));
    box->addView(makeNote("换肤只替换主题（tokens 与外观 JSON），不改视图树。"));

    auto* actions = new bklv::Box(bklv::Axis::ROW);
    actions->setGap(12.0f);
    actions->setPadding(8.0f, 0.0f, 0.0f, 0.0f);

    auto* themeButton = new bklv::Button("切换主题");
    themeButton->setOnClick([&app]() { app.cycleTheme(); });

    auto* backButton = new bklv::Button("返回上一页");
    backButton->setOnClick([&app]() { app.goBack(); });

    actions->addView(themeButton);
    actions->addView(backButton);
    box->addView(actions);

    box->addView(makeNote(std::string("当前主题：") + app.getThemeName()));

    return box;
}

} // namespace demo
