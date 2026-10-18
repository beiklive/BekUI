// 图标总览页：把两套图标字体里**实际存在**的字形全列出来。
//
// 结构（按用户要求）：
//   [Header "Switch Icon"]   → 网格（一排 10 个）
//   [Header "Material Icon"] → 网格（一排 10 个）
//   每个格子 = Box(column) = [图标字形, 该图标的码位（U+XXXX）]
//
// 数据来源：`bklv::getIconFontCodepoints()`（switch_icons.ttf）与 `bklv::icon::allGlyphs()`
// （MaterialIcons-Regular.ttf）—— 都是扫字体得到的"字体里真有的字形"，
// 不是 core/icon_font 里那张手写的常用表（那张只有几十个）。

#include "app/pages.hpp"

#include "bklv/bklv.hpp"
#include "bklv/core/icon_font.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace bklv;

namespace demo
{

namespace
{

// 一排几个
constexpr size_t kIconsPerRow = 10;

// 一个格子：Box(column) = [字形, U+XXXX]
Box* makeIconCell(unsigned int codepoint, ImFont* font)
{
    const ThemeData& theme = getTheme();

    // 一排 10 个 → 每个格子占 10%，格子很窄（约 60px），所以：
    //   · 子节点按 STRETCH 撑满格子宽度，文字各自居中（用 alignItems:CENTER 时
    //     Label 会按自己的测量宽度排版，"U+E000" 会被折成两行）
    //   · 码位文字必须 setSingleLine：窄格子里宁可裁掉也不能换行，否则行高会跳
    Box* cell = new Box(Axis::COLUMN);
    cell->setWidthPercentage(100.0f / static_cast<float>(kIconsPerRow));
    cell->setShrink(0.0f); // 不关收缩：10% 会被内容宽度压掉，整页变成细条（实测）
    cell->setAlignItems(AlignItems::STRETCH);
    cell->setJustifyContent(JustifyContent::CENTER);
    cell->setGap(theme.number("metric.spacing.xs", 4.0f));
    cell->setPadding(theme.number("metric.spacing.xs", 4.0f));
    cell->setMinHeight(theme.number("metric.list.itemHeight", 64.0f));

    // 图标字形：显式指定字体（Material 与 Switch 两套各用自己那份）
    Label* glyph = new Label();
    glyph->setText(icon::utf8Glyph(codepoint));
    glyph->setFont(font);
    glyph->setFontSize(theme.number("metric.font.size.lg", 26.0f));
    glyph->setTextColor(theme.color("color.text.primary"));
    glyph->setHorizontalAlign(HorizontalAlign::CENTER);
    cell->addView(glyph);

    // 码位
    char code[16];
    std::snprintf(code, sizeof(code), "U+%04X", codepoint);

    Label* codeLabel = new Label();
    codeLabel->setText(code);
    codeLabel->setFontSize(theme.number("metric.font.size.xs", 12.0f));
    codeLabel->setTextColor(theme.color("color.text.dim"));
    codeLabel->setHorizontalAlign(HorizontalAlign::CENTER);
    codeLabel->setSingleLine(true);
    cell->addView(codeLabel);

    return cell;
}

// 一个 header：标题 + 数量
Box* makeSectionHeader(const std::string& title, size_t count)
{
    // 同一个 section 外观：标题走 text 层，数量走右侧 info 层（borealis 的 Header 也是这个结构）
    AppearanceView* section = new AppearanceView("base/section", false);
    section->setText(title);
    section->setInfoText(getStr("icons/count", static_cast<int>(count)));
    return section;
}

// 网格：一排 10 个，格子等宽（10 × 10% = 100%，间距靠格子自己的 padding）
Box* makeIconGrid(const std::vector<unsigned int>& codepoints, ImFont* font)
{
    Box* grid = new Box(Axis::COLUMN);
    grid->setWidthPercentage(100.0f);
    grid->setShrink(0.0f);

    Box* row = nullptr;
    for (size_t i = 0; i < codepoints.size(); i++)
    {
        if (i % kIconsPerRow == 0)
        {
            row = new Box(Axis::ROW);
            row->setWidthPercentage(100.0f);
            row->setAlignItems(AlignItems::STRETCH);
            row->setShrink(0.0f);
            grid->addView(row);
        }

        row->addView(makeIconCell(codepoints[i], font));
    }

    return grid;
}

} // namespace

Box* buildIconsPage()
{
    // 注意：**不要**在页面里再套一层 ScrollingFrame。这个页面在 demo 里是"普通长页"
    // （selfScrolling=false），外层已经有滚动器；页面的外层框是 auto 高，内层滚动器
    // 拿不到确定高度 → 高度算成 0，整页内容都看不见（旧版正是这样：只看得见页头）。
    Box* page = new Box(Axis::COLUMN);
    // 这一页会放进页面的滚动器里：高度按内容撑（grow 会被压成一屏，滚不动），
    // 宽度必须留得下 10 列 —— 所有百分比容器都要显式关掉收缩，否则会被压成细条。
    page->setWidthPercentage(100.0f);
    page->setShrink(0.0f);

    // ---- Switch Icon ----
    const std::vector<unsigned int>& switchIcons = getIconFontCodepoints();
    page->addView(makeSectionHeader(getStr("icons/switch"), switchIcons.size()));
    page->addView(makeIconGrid(switchIcons, getIconFont()));

    // ---- Material Icon ----
    const std::vector<icon::Glyph>& materialGlyphs = icon::allGlyphs();

    std::vector<unsigned int> materialIcons;
    materialIcons.reserve(materialGlyphs.size());
    for (const icon::Glyph& glyph : materialGlyphs)
        materialIcons.push_back(glyph.codepoint);

    page->addView(makeSectionHeader(getStr("icons/material"), materialIcons.size()));
    page->addView(makeIconGrid(materialIcons, icon::font()));

    return page;
}

} // namespace demo
