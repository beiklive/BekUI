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

Box* buildWidgetsPage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeight(AUTO);

    page->addView(makeTitle("基础控件"));
    page->addView(makeHint("Label、自定义开关、进度条、加载指示器、按钮式的可聚焦行。"
                           "全部由 View + ImDrawList 绘制，没有调用任何 ImGui 控件。"));

    // 文本
    Box* textCard = makeCard();
    textCard->addView(makeText("文本", 16.0f, rgb(200, 208, 220)));
    textCard->addView(makeText("不同字号：14 / 18 / 26", 26.0f, rgb(235, 238, 242)));
    Label* small = makeText("这一行是 14 号字，用来说明 Label 支持独立字号与颜色。", 14.0f, rgb(150, 160, 175));
    small->setMarginTop(6.0f);
    textCard->addView(small);
    page->addView(textCard);

    // 自定义开关
    Box* toggleCard = makeCard();
    toggleCard->addView(makeText("自定义开关（InvisibleButton + ImDrawList）", 16.0f, rgb(200, 208, 220)));

    Box* toggleRow = makeRow(20.0f);
    toggleRow->setMarginTop(8.0f);

    ToggleSwitch* toggle1 = new ToggleSwitch();
    toggle1->registerAction(ControllerButton::A, "开关", [toggle1](View*) { toggle1->setValue(!toggle1->getValue()); });
    toggleRow->addView(toggle1);

    ToggleSwitch* toggle2 = new ToggleSwitch();
    toggle2->setValue(true);
    toggle2->registerAction(ControllerButton::A, "开关", [toggle2](View*) { toggle2->setValue(!toggle2->getValue()); });
    toggleRow->addView(toggle2);

    toggleCard->addView(toggleRow);
    page->addView(toggleCard);

    // 进度条
    Box* progressCard = makeCard();
    progressCard->addView(makeText("进度与加载", 16.0f, rgb(200, 208, 220)));

    Box* progressRow = makeRow(24.0f);
    progressRow->setMarginTop(10.0f);
    progressRow->setAlignItems(AlignItems::CENTER);

    CircularProgress* circle = new CircularProgress();
    circle->setValue(0.68f);
    progressRow->addView(circle);

    Box* bars = new Box(Axis::COLUMN);
    bars->setGrow(1.0f);
    bars->setGap(10.0f);

    struct BarSpec
    {
        const char* name;
        float value;
        ImU32 color;
    };
    const BarSpec specs[] = {
        { "下载 72%", 0.72f, IM_COL32(120, 190, 255, 255) },
        { "缓存 45%", 0.45f, IM_COL32(120, 205, 160, 255) },
        { "占用 88%", 0.88f, IM_COL32(230, 160, 90, 255) },
    };
    for (const BarSpec& spec : specs)
    {
        Box* line = new Box(Axis::COLUMN);
        line->setWidthPercentage(100.0f);
        line->addView(makeText(spec.name, 13.0f, rgb(160, 170, 185)));

        ProgressBar* bar = new ProgressBar();
        bar->setWidthPercentage(100.0f);
        bar->setValue(spec.value);
        bar->setColor(spec.color);
        bar->setMarginTop(5.0f);
        line->addView(bar);
        bars->addView(line);
    }
    progressRow->addView(bars);

    ProgressSpinner* spinner = new ProgressSpinner();
    spinner->setDimensions(40.0f, 40.0f);
    progressRow->addView(spinner);

    progressCard->addView(progressRow);
    page->addView(progressCard);

    // 可聚焦的行
    Box* listCard = makeCard();
    listCard->addView(makeText("可聚焦列表项（方向键移动，回车触发）", 16.0f, rgb(200, 208, 220)));

    Box* rows = new Box(Axis::COLUMN);
    rows->setWidthPercentage(100.0f);
    rows->setGap(8.0f);
    rows->setMarginTop(8.0f);

    const char* names[] = { "设置项 A", "设置项 B", "设置项 C" };
    for (int i = 0; i < 3; ++i)
    {
        // 用 Box 做列表项：聚焦就改背景，动作改文字
        class FocusRow : public Box
        {
          public:
            FocusRow()
                : Box(Axis::ROW)
            {
            }
            bool isFocusable() const override { return true; }
            void onFocusGained() override
            {
                this->setBackgroundColor(rgb(52, 74, 110));
                this->setBorderColor(rgb(90, 170, 245));
                this->setBorderThickness(2.0f);
            }
            void onFocusLost() override
            {
                this->setBackgroundColor(rgb(46, 51, 60));
                this->setBorderThickness(0.0f);
            }
        };
        FocusRow* focusRow = new FocusRow();
        focusRow->setWidthPercentage(100.0f);
        focusRow->setHeight(44.0f);
        focusRow->setAlignItems(AlignItems::CENTER);
        focusRow->setPadding(0.0f, 14.0f, 0.0f, 14.0f);
        focusRow->setBackgroundColor(rgb(46, 51, 60));
        focusRow->setCornerRadius(8.0f);

        Label* rowName = makeText(names[i], 15.0f, rgb(225, 230, 238), true);
        rowName->setGrow(1.0f);
        focusRow->addView(rowName);

        Label* rowState = makeText("按 A 切换", 13.0f, rgb(140, 150, 165), true);
        rowState->setHorizontalAlign(HorizontalAlign::RIGHT);
        focusRow->addView(rowState);

        focusRow->registerAction(ControllerButton::A, "切换", [rowState, i](View*) {
            static int counters[3] = { 0, 0, 0 };
            counters[i]++;
            rowState->setText("已触发 " + std::to_string(counters[i]) + " 次");
            rowState->setTextColor(rgb(120, 200, 160));
        });

        rows->addView(focusRow);
    }
    listCard->addView(rows);
    page->addView(listCard);

    return page;
}

} // namespace demo
