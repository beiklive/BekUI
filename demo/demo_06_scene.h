#pragma once

// Demo 06 的场景构建，抽成头文件是为了能用无窗口程序验证布局结果。

#include "bklv/core/box.hpp"
#include "bklv/views/label.hpp"

#include <string>

using namespace bklv;


// 造一个纯色方块，用来观察 Yoga 算出来的位置和尺寸。
static Box* makeSwatch(Axis axis, ImU32 color, float width, float height, float radius)
{
    Box* box = new Box(axis);
    box->setDimensions(width, height);
    box->setBackgroundColor(color);
    box->setCornerRadius(radius);
    return box;
}

// 造一个标题行：左侧标题 + 右侧单行说明。
static Box* makeSectionTitle(const std::string& title, const char* hint)
{
    Box* row = new Box(Axis::ROW);
    row->setWidthPercentage(100.0f);
    row->setAlignItems(AlignItems::CENTER);
    row->setJustifyContent(JustifyContent::SPACE_BETWEEN);
    row->setMarginBottom(10.0f);

    Label* titleLabel = new Label();
    titleLabel->setText(title);
    titleLabel->setFontSize(20.0f);
    titleLabel->setTextColor(rgb(235, 238, 242));
    row->addView(titleLabel);

    Label* hintLabel = new Label();
    hintLabel->setText(hint);
    hintLabel->setFontSize(14.0f);
    hintLabel->setSingleLine(true);
    hintLabel->setHorizontalAlign(HorizontalAlign::RIGHT);
    hintLabel->setTextColor(rgb(140, 150, 165));
    row->addView(hintLabel);

    return row;
}

static View* buildScene()
{
    // ---- 根容器：纵向排列，四周留白 ----
    Box* root = new Box(Axis::COLUMN);
    root->setWidthPercentage(100.0f);
    root->setHeightPercentage(100.0f);
    root->setBackgroundColor(rgb(26, 28, 33));
    root->setPadding(24.0f);
    root->setGap(20.0f);

    // ---- 标题 ----
    Box* header = new Box(Axis::COLUMN);
    header->setWidthPercentage(100.0f);

    Label* title = new Label();
    title->setText("bklv 移植层：Box 布局 + Label 文本");
    title->setFontSize(26.0f);
    title->setTextColor(rgb(255, 255, 255));
    header->addView(title);

    Label* subtitle = new Label();
    subtitle->setText("布局由 Yoga 计算，绘制交给 ImDrawList。改窗口大小会自动重排。");
    subtitle->setFontSize(15.0f);
    subtitle->setTextColor(rgb(150, 160, 175));
    subtitle->setMarginTop(6.0f);
    header->addView(subtitle);

    root->addView(header);

    // ---- 第一块：主轴分布 SPACE_BETWEEN + 交叉轴 CENTER ----
    Box* section1 = new Box(Axis::COLUMN);
    section1->setWidthPercentage(100.0f);
    section1->setBackgroundColor(rgb(38, 42, 50));
    section1->setCornerRadius(10.0f);
    section1->setPadding(16.0f);

    section1->addView(makeSectionTitle("1. justifyContent = SPACE_BETWEEN, alignItems = CENTER",
        "三个方块贴两端分布"));

    Box* row1 = new Box(Axis::ROW);
    row1->setWidthPercentage(100.0f);
    row1->setHeight(90.0f);
    row1->setJustifyContent(JustifyContent::SPACE_BETWEEN);
    row1->setAlignItems(AlignItems::CENTER);

    row1->addView(makeSwatch(Axis::COLUMN, rgb(57, 96, 145), 90.0f, 90.0f, 8.0f));
    row1->addView(makeSwatch(Axis::COLUMN, rgb(70, 125, 100), 90.0f, 50.0f, 8.0f));
    row1->addView(makeSwatch(Axis::COLUMN, rgb(145, 94, 67), 90.0f, 90.0f, 8.0f));
    section1->addView(row1);

    root->addView(section1);

    // ---- 第二块：gap + flex-end + margin ----
    Box* section2 = new Box(Axis::COLUMN);
    section2->setWidthPercentage(100.0f);
    section2->setBackgroundColor(rgb(38, 42, 50));
    section2->setCornerRadius(10.0f);
    section2->setPadding(16.0f);

    section2->addView(makeSectionTitle("2. gap = 14, justifyContent = FLEX_END, 中间块带 margin",
        "末端对齐 + 间距"));

    Box* row2 = new Box(Axis::ROW);
    row2->setWidthPercentage(100.0f);
    row2->setHeight(70.0f);
    row2->setGap(14.0f);
    row2->setJustifyContent(JustifyContent::FLEX_END);
    row2->setAlignItems(AlignItems::FLEX_END);

    row2->addView(makeSwatch(Axis::COLUMN, rgb(105, 84, 150), 80.0f, 70.0f, 8.0f));

    Box* middle = makeSwatch(Axis::COLUMN, rgb(60, 140, 160), 80.0f, 44.0f, 8.0f);
    middle->setMarginBottom(12.0f);
    row2->addView(middle);

    row2->addView(makeSwatch(Axis::COLUMN, rgb(180, 130, 60), 80.0f, 70.0f, 8.0f));
    section2->addView(row2);

    root->addView(section2);

    // ---- 第三块：flex-grow 撑满剩余宽度 ----
    Box* section3 = new Box(Axis::COLUMN);
    section3->setWidthPercentage(100.0f);
    section3->setBackgroundColor(rgb(38, 42, 50));
    section3->setCornerRadius(10.0f);
    section3->setPadding(16.0f);
    section3->setGrow(1.0f);

    section3->addView(makeSectionTitle("3. 左侧固定宽度，右侧 grow = 1 撑满剩余空间", "嵌套 Box + Label"));

    Box* row3 = new Box(Axis::ROW);
    row3->setWidthPercentage(100.0f);
    row3->setGrow(1.0f);
    row3->setGap(14.0f);
    row3->setAlignItems(AlignItems::STRETCH);

    Box* sidebar = makeSwatch(Axis::COLUMN, rgb(52, 58, 70), 150.0f, AUTO, 8.0f);
    sidebar->setShrink(0.0f); // Web 默认 flex-shrink = 1，不关掉的话固定宽度会被压缩
    sidebar->setPadding(12.0f);

    Label* sidebarLabel = new Label();
    sidebarLabel->setText("侧栏\n固定 150");
    sidebarLabel->setFontSize(16.0f);
    sidebarLabel->setTextColor(rgb(200, 208, 220));
    sidebarLabel->setVerticalAlign(VerticalAlign::CENTER);
    sidebar->addView(sidebarLabel);
    row3->addView(sidebar);

    Box* content = makeSwatch(Axis::COLUMN, rgb(45, 49, 56), AUTO, AUTO, 8.0f);
    content->setGrow(1.0f);
    content->setPadding(14.0f);

    Label* paragraph = new Label();
    paragraph->setText("这段文字用来验证自动换行。Yoga 不知道文字有多宽，"
                       "所以 Label 提供了一个测量函数：布局时按可用宽度逐字测量，"
                       "算好需要几行、总高多少，再把这行文本画到 ImDrawList 上。"
                       "窗口变窄时会重新断行，变宽时会合并回更少的行数。");
    paragraph->setFontSize(16.0f);
    paragraph->setLineHeight(1.5f);
    paragraph->setTextColor(rgb(210, 216, 226));
    paragraph->setAlignSelf(AlignSelf::STRETCH);
    content->addView(paragraph);
    row3->addView(content);

    section3->addView(row3);
    root->addView(section3);

    return root;
}

