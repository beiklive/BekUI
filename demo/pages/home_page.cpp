#include "home_page.hpp"

#include <string>

#include "bklv/system/media/icon_font.hpp"
#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/basic/label.hpp"
#include "bklv/ui/widgets/container/dialog.hpp"
#include "bklv/ui/widgets/container/list_box.hpp"
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

    // 导航与换肤
    auto* actions = new bklv::Box(bklv::Axis::ROW);
    actions->setGap(12.0f);
    actions->setPadding(8.0f, 0.0f, 0.0f, 0.0f);

    auto* settingsButton = new bklv::TextButton("打开设置");
    settingsButton->setOnClick([&app]() { app.openSettings(); });

    auto* themeButton = new bklv::TextButton("切换主题");
    themeButton->setOnClick([&app]() { app.cycleTheme(); });

    actions->addView(settingsButton);
    actions->addView(themeButton);
    box->addView(actions);

    // 两种通知形态各一个按钮（没有纯文字通知），再加一个连发，
    // 用来看"不设上限、一直往下堆"和逐条收回时下面的条目往上挪。
    auto* toasts = new bklv::Box(bklv::Axis::ROW);
    toasts->setGap(12.0f);

    auto* infoToast = new bklv::TextButton("信息通知");
    infoToast->setOnClick([&app]() { app.app().notify("Hello, BekUI demo"); });

    auto* iconToast = new bklv::TextButton("图标通知");
    iconToast->setOnClick(
        [&app]() { app.app().notifyIcon("已保存", bklv::icon::glyph(bklv::icon::Id::SAVE)); });

    auto* imageToast = new bklv::TextButton("图片通知");
    imageToast->setOnClick(
        [&app]() { app.app().notifyImage("卡片已生成", "resources/testcard.png"); });

    auto* burstButton = new bklv::TextButton("连发 5 条");
    burstButton->setOnClick(
        [&app]()
        {
            // 5 条一起发、时长错开（4~8 秒），图标与图片交替，
            // 方便看清"到期的先收回、下面的往上挪"
            for (int i = 0; i < 5; i++)
            {
                const std::string text = "队列消息 #" + std::to_string(i + 1);
                if (i % 2 == 0)
                    app.app().notifyImage(text, "resources/testcard.png", 4.0f + (float)i);
                else
                    app.app().notifyIcon(
                        text, bklv::icon::glyph(bklv::icon::Id::INFO), 4.0f + (float)i);
            }
        });

    toasts->addView(infoToast);
    toasts->addView(iconToast);
    toasts->addView(imageToast);
    toasts->addView(burstButton);
    box->addView(toasts);

    // 两个派生容器：对话框（自带玻璃遮罩）与列表（纵向滚动）。
    auto* containers = new bklv::Box(bklv::Axis::ROW);
    containers->setGap(12.0f);

    auto* dialogButton = new bklv::TextButton("对话框");
    dialogButton->setOnClick(
        [&app]()
        {
            auto* dialog = new bklv::Dialog();
            dialog->setIcon(bklv::icon::glyph(bklv::icon::Id::INFO));
            dialog->setTitle("删除这一项？");

            // 内容区是可滚动的：内容多了自己会出现滚动条
            auto* text = new bklv::Label();
            text->setSingleLine(false);
            text->setText("这条消息会被永久删除，无法恢复。下面的内容故意写长一点，"
                          "用来验证内容区是滚动的、而按钮行固定在底部。");
            dialog->getContentBox()->addView(text);
            for (int i = 1; i <= 6; i++)
            {
                auto* line = new bklv::Label();
                line->setSingleLine(false);
                line->setText("附加说明第 " + std::to_string(i) + " 行：内容再多也只滚这一块。");
                dialog->getContentBox()->addView(line);
            }

            dialog->addButton("取消", [dialog]() { dialog->close(true); });
            dialog->addButton("删除", [&app, dialog]()
            {
                dialog->close(true);
                app.app().notifyIcon("已删除", bklv::icon::glyph(bklv::icon::Id::SAVE));
            });
            dialog->open();
        });

    auto* listButton = new bklv::TextButton("列表容器");
    listButton->setOnClick(
        [&app]()
        {
            auto* dialog = new bklv::Dialog();
            dialog->setTitle("列表容器");

            auto* list = new bklv::ListBox();
            // 滚动容器必须有界的高度：高度不定时它没法算"视口多高"，会退化成一个点。
            // 这里固定 420，够放 8 条左右，剩下的靠滚动。
            list->setHeight(420.0f);
            list->setEmptyText("这里还没有内容");
            for (int i = 1; i <= 20; i++)
                list->addItemText("列表项 " + std::to_string(i) + "（用方向键或滚轮滚动）");
            dialog->getContentBox()->addView(list);

            dialog->addButton("关闭", [dialog]() { dialog->close(true); });
            dialog->open();
        });

    auto* stackButton = new bklv::TextButton("层级页");
    stackButton->setOnClick([&app]() { app.openStackList(); });

    auto* tabsButton = new bklv::TextButton("标签页");
    tabsButton->setOnClick([&app]() { app.openTabs(); });
    containers->addView(tabsButton);

    containers->addView(dialogButton);
    containers->addView(stackButton);
    containers->addView(listButton);
    box->addView(containers);

    // 状态行：换肤后 cycleTheme() 会重建当前页，这里就会跟着变。
    box->addView(makeNote(std::string("当前主题：") + app.getThemeName()));

    return box;
}

} // namespace demo
