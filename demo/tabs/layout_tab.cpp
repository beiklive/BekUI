#include "layout_tab.hpp"

#include "bklv/ui/theme/theme_data.hpp"
#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/container/scroll_box.hpp"
#include "demo/demo_app.hpp"
#include "demo/layout_kit.hpp"
#include "demo/style.hpp"

namespace demo
{

namespace
{

ImU32 themeColor(const char* key, ImU32 fallback)
{
    return bklv::getTheme().color(key, fallback);
}

bklv::Box* makeTrack(float height)
{
    auto* track = new bklv::Box(bklv::Axis::ROW);
    track->setHeight(height);
    track->setGap(12.0f);
    track->setPadding(10.0f);
    track->setBackgroundColor(themeColor("color.surface.sunken", IM_COL32(227, 229, 234, 255)));
    return track;
}

} // namespace

bklv::View* buildLayoutTab(DemoApp& app)
{
    (void)app;

    const ImU32 blue  = themeColor("color.accent.base", IM_COL32(25, 118, 210, 255));
    const ImU32 green = themeColor("color.text.value", IM_COL32(0, 122, 99, 255));
    const ImU32 red   = themeColor("color.state.danger", IM_COL32(230, 106, 106, 255));

    auto* scroll = new bklv::ScrollBox(bklv::Axis::COLUMN);
    scroll->setGrow(1.0f);

    auto* content = scroll->getContentBox();
    content->setGap(26.0f);
    content->setPadding(24.0f);

    // 1) 横向排列 + gap：三块固定宽度的矩形
    {
        auto* row = new bklv::Box(bklv::Axis::ROW);
        row->setGap(12.0f);
        row->setAlignItems(bklv::AlignItems::CENTER);

        row->addView(makeColorRect(blue, 56.0f, 80.0f));
        row->addView(makeColorRect(green, 56.0f, 120.0f));
        row->addView(makeColorRect(red, 56.0f, 80.0f));

        content->addView(makeSection("横向排列", row, "gap 12"));
    }

    // 2) 纵向排列：宽度按容器拉伸
    {
        auto* column = new bklv::Box(bklv::Axis::COLUMN);
        column->setGap(10.0f);

        column->addView(makeColorRect(blue, 40.0f));
        column->addView(makeColorRect(green, 64.0f));

        content->addView(makeSection("纵向排列", column, "宽度拉伸"));
    }

    // 3) grow：按 1 : 2 : 3 分配剩余宽度
    {
        auto* row = new bklv::Box(bklv::Axis::ROW);
        row->setGap(12.0f);

        auto* a = makeColorRect(blue, 56.0f);
        a->setGrow(1.0f);
        auto* b = makeColorRect(green, 56.0f);
        b->setGrow(2.0f);
        auto* c = makeColorRect(red, 56.0f);
        c->setGrow(3.0f);

        row->addView(a);
        row->addView(b);
        row->addView(c);

        content->addView(makeSection("grow", row, "1 : 2 : 3"));
    }

    // 4) 内边距 + 居中
    {
        auto* panel = new bklv::Box(bklv::Axis::ROW);
        panel->setHeight(140.0f);
        panel->setPadding(20.0f);
        panel->setGap(12.0f);
        panel->setBackgroundColor(themeColor("color.surface.raised", IM_COL32(236, 238, 242, 255)));
        panel->setJustifyContent(bklv::JustifyContent::CENTER);
        panel->setAlignItems(bklv::AlignItems::CENTER);

        panel->addView(makeColorRect(blue, 48.0f, 48.0f));
        panel->addView(makeColorRect(green, 48.0f, 48.0f));
        panel->addView(makeColorRect(red, 48.0f, 48.0f));

        content->addView(makeSection("内边距 + 居中", panel, "padding 20"));
    }

    // 5) 主轴对齐：flexStart / center / flexEnd
    {
        auto* column = new bklv::Box(bklv::Axis::COLUMN);
        column->setGap(8.0f);

        struct Row
        {
            const char* label;
            bklv::JustifyContent justify;
            ImU32 color;
        };

        const Row rows[] = {
            { "flexStart（贴左）", bklv::JustifyContent::FLEX_START, blue },
            { "center（居中）", bklv::JustifyContent::CENTER, green },
            { "flexEnd（贴右）", bklv::JustifyContent::FLEX_END, red },
        };

        for (const Row& item : rows)
        {
            auto* track = makeTrack(56.0f);
            track->setJustifyContent(item.justify);
            track->addView(makeColorRect(item.color, 36.0f, 96.0f));
            column->addView(track);
        }

        content->addView(makeSection("主轴对齐", column, "justifyContent"));
    }

    return scroll;
}

} // namespace demo
