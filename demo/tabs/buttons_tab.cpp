#include "buttons_tab.hpp"

#include "bklv/system/media/icon_font.hpp"
#include "bklv/ui/view/box.hpp"
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

} // namespace

bklv::View* buildButtonsTab(DemoApp& app)
{
    // 标签页内容可能比窗口高，所以整页可滚。
    auto* scroll = new bklv::ScrollBox(bklv::Axis::COLUMN);
    scroll->setGrow(1.0f);

    auto* content = scroll->getContentBox();
    content->setGap(26.0f);
    content->setPadding(24.0f);

    // 1) 文字按钮：普通 / 禁用 / 开关
    {
        auto* row = makeRow();

        auto* normal = new bklv::TextButton("普通按钮");
        normal->setOnClick([&app]() { app.notify("普通按钮被点击"); });

        auto* disabled = new bklv::TextButton("禁用按钮");
        disabled->setEnabled(false);

        auto* switched = new bklv::TextButton("开关按钮");
        switched->setSwitched(true);

        row->addView(normal);
        row->addView(disabled);
        row->addView(switched);
        content->addView(makeSection("文字按钮 TextButton", row));
    }

    // 2) 图标按钮：只有图标，正方形
    {
        auto* row = makeRow();

        const bklv::icon::Id icons[] = { bklv::icon::Id::SETTINGS, bklv::icon::Id::SAVE, bklv::icon::Id::STAR };
        for (bklv::icon::Id id : icons)
        {
            auto* button = new bklv::IconButton(bklv::icon::glyph(id));
            button->setOnClick([&app]() { app.notify("图标按钮被点击"); });
            row->addView(button);
        }

        content->addView(makeSection("图标按钮 IconButton", row));
    }

    // 3) 图标 + 文字
    {
        auto* row = makeRow();

        auto* favorite = new bklv::IconTextButton("收藏", bklv::icon::glyph(bklv::icon::Id::STAR));
        favorite->setOnClick([&app]() { app.notify("已收藏"); });

        auto* save = new bklv::IconTextButton("保存", bklv::icon::glyph(bklv::icon::Id::SAVE));
        save->setOnClick([&app]() { app.notify("已保存"); });

        row->addView(favorite);
        row->addView(save);
        content->addView(makeSection("图标 + 文字 IconTextButton", row));
    }

    // 4) 点击反馈与换肤
    {
        auto* row = makeRow();

        auto* toast = new bklv::TextButton("发一条通知");
        toast->setOnClick([&app]() { app.notify("这是来自按钮的通知"); });

        auto* theme = new bklv::TextButton("切换主题");
        theme->setOnClick([&app]() { app.cycleTheme(); });

        row->addView(toast);
        row->addView(theme);
        content->addView(makeSection("点击反馈", row));
    }

    // 5) 标签条排列（"切换 tab 类型"的开关就放在这个标签页里）
    {
        auto* row = makeRow();

        auto* toggle = new bklv::TextButton("切换标签条排列（顶部 / 左侧）");
        toggle->setOnClick([&app]() { app.setTabOrientation(!app.isTabVertical()); });

        row->addView(toggle);
        content->addView(makeSection("标签条排列", row));
    }

    return scroll;
}

} // namespace demo
