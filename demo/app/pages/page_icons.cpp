// 图标总览页：罗列 bklv 自带图标库（core/icon_font）里的全部 Material Icons。
//
// 一行一个：字形 + Material 名字 + 中文说明 + 私有区码位。
// 想确认"某个图标长什么样 / 码位是多少"时看这一页，不用去翻字体文件。

#include "app/pages.hpp"

#include "app/ui.hpp"
#include "bklv/bklv.hpp"
#include "bklv/core/icon_font.hpp"

#include <cstdio>

using namespace bklv;

namespace demo
{

namespace
{

// 一个图标条目：左边字形（用图标字体），右边名字 / 中文 / 码位
Box* makeIconRow(const icon::Info& info)
{
    Box* row = new Box();
    row->setAxis(Axis::ROW);
    row->setAlignItems(AlignItems::CENTER);
    row->setGap(getTheme().number("metric.gap.sm", 8.0f));
    row->setPadding(0.0f, getTheme().number("metric.padding.md", 16.0f), 0.0f,
        getTheme().number("metric.padding.md", 16.0f));
    row->setPaddingTop(6.0f);
    row->setPaddingBottom(6.0f);

    // 字形：Label 换成图标字体
    Label* glyph = new Label();
    glyph->setText(icon::glyph(info.id));
    glyph->setIconFontEnabled(true);
    glyph->setFontSize(getTheme().number("metric.font.size.lg", 26.0f));
    glyph->setTextColor(getTheme().color("color.text.primary"));
    glyph->setWidth(40.0f);
    row->addView(glyph);

    char line[128];
    std::snprintf(line, sizeof(line), "%s", info.name);
    Label* name = new Label();
    name->setText(line);
    name->setFontSize(getTheme().number("metric.font.size.md", 18.0f));
    name->setTextColor(getTheme().color("color.text.primary"));
    name->setGrow(1.0f);
    row->addView(name);

    std::snprintf(line, sizeof(line), "%s", info.chineseName);
    Label* chinese = new Label();
    chinese->setText(line);
    chinese->setFontSize(getTheme().number("metric.font.size.md", 18.0f));
    chinese->setTextColor(getTheme().color("color.text.dim"));
    chinese->setWidth(120.0f);
    row->addView(chinese);

    std::snprintf(line, sizeof(line), "U+%04X", info.codepoint);
    Label* code = new Label();
    code->setText(line);
    code->setFontSize(getTheme().number("metric.font.size.sm", 16.0f));
    code->setTextColor(getTheme().color("color.text.disabled"));
    code->setWidth(90.0f);
    row->addView(code);

    return row;
}

} // namespace

Box* buildIconsPage()
{
    Box* page = new Box();
    page->setAxis(Axis::COLUMN);
    page->setGrow(1.0f);

    Box* header = new Box();
    header->setAxis(Axis::COLUMN);
    header->setPadding(getTheme().number("metric.padding.md", 16.0f));
    header->setPaddingTop(8.0f);
    header->setPaddingBottom(8.0f);
    header->addView(makeText(getStr("icons/title"),
        getTheme().number("metric.font.size.lg", 20.0f),
        getTheme().color("color.text.primary")));
    header->addView(makeHint(getStr("icons/loaded", (int)icon::all().size())));
    page->addView(header);

    // 图标数量不多，直接全部铺出来，外面套滚动容器
    ScrollingFrame* scroller = new ScrollingFrame();
    scroller->setGrow(1.0f);
    scroller->setShrink(1.0f);

    Box* list = new Box();
    list->setAxis(Axis::COLUMN);
    for (const icon::Info& info : icon::all())
    {
        if (info.id == icon::Id::NONE)
            continue;
        list->addView(makeIconRow(info));
    }
    scroller->setContentView(list);
    page->addView(scroller);

    return page;
}

} // namespace demo
