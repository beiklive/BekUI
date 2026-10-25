#include "container_tab.hpp"

#include <cstdio>
#include <string>

#include "bklv/system/media/icon_font.hpp"
#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/basic/label.hpp"
#include "bklv/ui/widgets/container/dialog.hpp"
#include "bklv/ui/widgets/container/list_box.hpp"
#include "bklv/ui/widgets/container/scroll_box.hpp"
#include "bklv/ui/widgets/controls/button.hpp"
#include "demo/demo_app.hpp"
#include "demo/layout_kit.hpp"
#include "demo/style.hpp"

namespace demo
{

namespace
{

bklv::Box* makeRow()
{
    auto* row = new bklv::Box(bklv::Axis::ROW);
    row->setGap(12.0f);
    row->setAlignItems(bklv::AlignItems::CENTER);
    return row;
}

// 一张示例对话框：图标 + 标题 + 几行正文 + 底部两个按钮。
bklv::Dialog* makeDemoDialog(DemoApp& app)
{
    auto* dialog = new bklv::Dialog();
    dialog->setIcon(bklv::icon::glyph(bklv::icon::Id::WARNING));
    dialog->setTitle("删除这一项？");

    auto* body = dialog->getContentBox();
    body->setGap(10.0f);
    body->addView(makeParagraph("这条消息会被永久删除，无法恢复。"));
    body->addView(makeNote("对话框的面板与遮罩都是外观（base/box/dialog、base/box/dialog_backdrop），"
                           "打开时自动接管焦点，B / Esc 关闭。"));
    body->addView(makeNote("底部按钮均分宽度：两个各占一半、一个时铺满整行。"));

    dialog->addButton("取消");
    dialog->addButton("删除", [&app] { app.notify("已删除（示例）"); });

    return dialog;
}

} // namespace

bklv::View* buildContainerTab(DemoApp& app)
{
    auto* scroll = new bklv::ScrollBox(bklv::Axis::COLUMN);
    scroll->setGrow(1.0f);

    auto* content = scroll->getContentBox();
    content->setGap(26.0f);
    content->setPadding(24.0f);

    // 1) 对话框
    {
        auto* row = makeRow();

        auto* open = new bklv::TextButton("打开对话框");
        open->setOnClick([&app] { app.openDemoDialog(); });

        row->addView(open);
        content->addView(makeSection("对话框", row, "Dialog · 玻璃遮罩 + 按钮均分"));
    }

    // 2) 列表容器
    {
        auto* column = new bklv::Box(bklv::Axis::COLUMN);
        column->setGap(12.0f);

        auto* list = new bklv::ListBox();
        list->setGrow(1.0f);
        list->setEmptyText("列表是空的（点下面的「补一条」试试）");
        // 6 条正好差不多填满 320 的视口：再多就要滚动，而「通知」那节会被顶到下面去
        for (int i = 0; i < 6; ++i)
            list->addItemText("列表项 " + std::to_string(i + 1) + "（方向键移动，焦点会自动滚进视口）");

        auto* row = makeRow();

        auto* add = new bklv::TextButton("补一条");
        add->setOnClick([list] { list->addItemText("列表项 " + std::to_string(list->getItemCount() + 1)); });

        auto* clear = new bklv::TextButton("清空");
        clear->setOnClick([list] { list->clearItems(); });

        row->addView(add);
        row->addView(clear);

        // ListBox 自己 setHeight 在拉伸容器里压不住（会按 100% 撑开），
        // 用外面这层固定高的盒子给它一个确定的视口。
        auto* holder = new bklv::Box(bklv::Axis::COLUMN);
        holder->setHeight(320.0f);
        holder->setClipsToBounds(true);
        holder->addView(list);

        column->addView(holder);
        column->addView(row);
        content->addView(makeSection("列表容器", column, "ListBox · 滚动条 / 空态 / 焦点跟随"));
    }

    // 3) 通知的三种形态
    {
        auto* row = makeRow();

        auto* icon = new bklv::TextButton("图标通知");
        icon->setOnClick([&app] {
            app.app().notifyIcon("已保存", bklv::icon::glyph(bklv::icon::Id::SAVE));
        });

        auto* image = new bklv::TextButton("图片通知");
        image->setOnClick([&app] { app.notifyImage("卡片已生成"); });

        auto* burst = new bklv::TextButton("连发 5 条");
        burst->setOnClick([&app] { app.notifyBurst(5); });

        row->addView(icon);
        row->addView(image);
        row->addView(burst);
        content->addView(makeSection("通知", row, "图标 / 图片 / 堆叠不设上限"));
    }

    return scroll;
}

} // namespace demo
