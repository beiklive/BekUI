#include "app/pages/pages.hpp"

#include <string>

#include "app/page_helpers.hpp"
#include "app/widgets.hpp"
#include "bklv/core/input.hpp"
#include "bklv/views/image.hpp"
#include "bklv/views/progress_spinner.hpp"
#include "bklv/views/recycler.hpp"
#include "bklv/views/scrolling_frame.hpp"

using namespace bklv;

namespace demo
{

Box* buildTextPage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeight(AUTO);

    page->addView(makeTitle("文本排版"));
    page->addView(makeHint("Label 自己实现了换行与截断：按字符测宽、二分找断点，"
                           "排版结果缓存在控件里，只在文本或宽度变化时重算。"));

    Box* wrapCard = makeCard();
    wrapCard->addView(makeText("自动换行", 16.0f, rgb(200, 208, 220)));

    Label* paragraph = makeText(
        "这段文字用来自动换行。Yoga 不知道文字有多宽，所以 Label 向它注册了测量函数："
        "布局时按可用宽度逐字测量，算出需要几行、总高多少，再把这行文本画到 ImDrawList 上。"
        "窗口变窄时会重新断行，变宽时会合并回更少的行数。",
        15.0f, rgb(215, 222, 232));
    paragraph->setLineHeight(1.6f);
    paragraph->setMarginTop(8.0f);
    wrapCard->addView(paragraph);
    page->addView(wrapCard);

    Box* truncCard = makeCard();
    truncCard->addView(makeText("单行截断（超宽自动加省略号）", 16.0f, rgb(200, 208, 220)));

    const char* longTexts[] = {
        "这是一条很长的单行文本，宽度不够时应该在末尾出现省略号而不是硬撑出去",
        "短文本",
        "中等的单行文本，刚好会被截断一部分内容显示省略号",
    };
    for (const char* text : longTexts)
    {
        Label* line = makeText(text, 15.0f, rgb(215, 222, 232), true);
        line->setMarginTop(8.0f);
        truncCard->addView(line);
    }
    page->addView(truncCard);

    Box* alignCard = makeCard();
    alignCard->addView(makeText("对齐", 16.0f, rgb(200, 208, 220)));

    const HorizontalAlign aligns[] = { HorizontalAlign::LEFT, HorizontalAlign::CENTER, HorizontalAlign::RIGHT };
    const char* alignNames[]        = { "左对齐", "居中", "右对齐" };
    for (int i = 0; i < 3; ++i)
    {
        Box* line = new Box(Axis::ROW);
        line->setWidthPercentage(100.0f);
        line->setHeight(34.0f);
        line->setMarginTop(6.0f);
        line->setBackgroundColor(rgb(46, 51, 60));
        line->setCornerRadius(6.0f);

        Label* label = makeText(alignNames[i], 15.0f, rgb(215, 222, 232), true);
        label->setWidthPercentage(100.0f);
        label->setHorizontalAlign(aligns[i]);
        line->addView(label);
        alignCard->addView(line);
    }
    page->addView(alignCard);

    return page;
}

} // namespace demo
