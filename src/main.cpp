// bklv 能力总览：Tab 页面。
//
// 用 bklv 自己的控件搭出 tab 导航 + 多页内容，逐页展示已经实现的框架能力。
// 没有任何 ImGui 窗口调用，也没有任何图形 API 调用——全部走 View 树。
//
// 操作：
//   左右 / AD            切换 tab（焦点在 tab 栏时）
//   上下 / WS            在页面内容里移动焦点
//   回车 / 空格 / 手柄 A 触发当前焦点控件的动作
//   鼠标点击             直接选中 tab 或控件
//   滚轮 / 拖拽          滚动页面
//   F1                  开关调试层（布局树 + 当前焦点 + 后端信息）

#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "bklv/core/app.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/graphics.hpp"
#include "bklv/views/h_scrolling_frame.hpp"
#include "bklv/views/image.hpp"
#include "bklv/views/label.hpp"
#include "bklv/views/progress_spinner.hpp"
#include "bklv/views/recycler.hpp"
#include "bklv/views/scrolling_frame.hpp"

using namespace bklv;

namespace
{

// ---------------------------------------------------------------------------
// 页面里用到的小控件
// ---------------------------------------------------------------------------

// 自定义开关：InvisibleButton + ImDrawList 手绘。用来展示"怎么自己画一个控件"。
class ToggleSwitch : public View
{
  public:
    ToggleSwitch()
    {
        this->setDimensions(200.0f, 32.0f);
    }

    bool isFocusable() const override { return true; }
    void setValue(bool v) { this->value = v; }
    bool getValue() const { return this->value; }

    void onFocusGained() override { this->focused = true; }
    void onFocusLost() override { this->focused = false; }

  protected:
    void draw(ImDrawList* drawList, float x, float y, float /*width*/, float height, Style, FrameContext*) override
    {
        const float trackWidth  = 54.0f;
        const float trackHeight = 24.0f;
        const float trackY      = y + (height - trackHeight) * 0.5f;

        const ImU32 track = this->value ? IM_COL32(74, 144, 226, 255) : IM_COL32(70, 76, 86, 255);
        const ImU32 knob  = this->value ? IM_COL32(250, 252, 255, 255) : IM_COL32(150, 158, 170, 255);

        drawList->AddRectFilled(ImVec2(x, trackY), ImVec2(x + trackWidth, trackY + trackHeight), track,
            trackHeight * 0.5f);

        const float knobX = x + (this->value ? trackWidth - 17.0f : 17.0f);
        drawList->AddCircleFilled(ImVec2(knobX, trackY + trackHeight * 0.5f), 9.0f, knob);

        const ImU32 textColor = this->focused ? IM_COL32(235, 238, 242, 255) : IM_COL32(150, 158, 170, 255);
        drawList->AddText(ImVec2(x + trackWidth + 12.0f, y + (height - 20.0f) * 0.5f), textColor,
            this->value ? "已开启" : "已关闭");

        if (this->focused)
            drawList->AddRect(ImVec2(x - 3.0f, trackY - 3.0f), ImVec2(x + trackWidth + 3.0f, trackY + trackHeight + 3.0f),
                IM_COL32(120, 190, 255, 200), 6.0f, 0, 2.0f);
    }

  private:
    bool value   = false;
    bool focused = false;
};

// 环形进度：用 PathArcTo + PathStroke 画弧。展示"路径式绘制"。
class CircularProgress : public View
{
  public:
    CircularProgress()
    {
        this->setDimensions(72.0f, 72.0f);
    }

    void setValue(float v) { this->value = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    float getValue() const { return this->value; }

    void update() override
    {
        // 自己缓慢转一圈，证明 update() 每帧都会走到。
        this->phase += ImGui::GetIO().DeltaTime * 0.35f;
        if (this->phase > 1.0f)
            this->phase -= 1.0f;
    }

  protected:
    void draw(ImDrawList* drawList, float x, float y, float width, float height, Style, FrameContext*) override
    {
        const ImVec2 center(x + width * 0.5f, y + height * 0.5f);
        const float radius = (width < height ? width : height) * 0.5f - 6.0f;

        // 底环
        drawList->PathArcTo(center, radius, 0.0f, 2.0f * 3.14159265f, 48);
        drawList->PathStroke(IM_COL32(60, 66, 78, 255), 0, 6.0f);

        // 进度弧：从 -90 度开始顺时针
        const float start = -1.5707963f;
        const float end   = start + 2.0f * 3.14159265f * this->value;
        if (this->value > 0.001f)
        {
            drawList->PathArcTo(center, radius, start, end, 48);
            drawList->PathStroke(IM_COL32(120, 190, 255, 255), 0, 6.0f);
        }

        // 中心文字用百分比
        char text[16];
        std::snprintf(text, sizeof(text), "%.0f%%", this->value * 100.0f);
        ImFont* font = getDefaultFont();
        if (font != nullptr)
        {
            const ImVec2 size = font->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, text);
            drawList->AddText(font, 14.0f, ImVec2(center.x - size.x * 0.5f, center.y - 8.0f),
                IM_COL32(225, 230, 238, 255), text);
        }
    }

  private:
    float value = 0.0f;
    float phase = 0.0f;
};

// 横向进度条
class ProgressBar : public View
{
  public:
    ProgressBar()
    {
        this->setHeight(10.0f);
    }

    void setValue(float v) { this->value = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    void setColor(ImU32 c) { this->color = c; }

  protected:
    void draw(ImDrawList* drawList, float x, float y, float width, float height, Style, FrameContext*) override
    {
        drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), IM_COL32(52, 58, 70, 255),
            height * 0.5f);
        if (this->value > 0.001f)
            drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width * this->value, y + height), this->color,
                height * 0.5f);
    }

  private:
    float value = 0.0f;
    ImU32 color = IM_COL32(120, 190, 255, 255);
};

// ---------------------------------------------------------------------------
// 通用小工具
// ---------------------------------------------------------------------------

Label* makeText(const std::string& text, float size, ImU32 color, bool singleLine = false)
{
    Label* label = new Label();
    label->setText(text);
    label->setFontSize(size);
    label->setTextColor(color);
    label->setSingleLine(singleLine);
    return label;
}

Label* makeTitle(const std::string& text)
{
    Label* label = makeText(text, 22.0f, rgb(255, 255, 255));
    label->setMarginBottom(6.0f);
    return label;
}

Label* makeHint(const std::string& text)
{
    Label* label = makeText(text, 14.0f, rgb(150, 160, 175));
    label->setLineHeight(1.5f);
    label->setMarginBottom(14.0f);
    return label;
}

// 一张卡片：带背景、圆角、内边距，内部纵向排列
Box* makeCard(float padding = 16.0f)
{
    Box* card = new Box(Axis::COLUMN);
    card->setWidthPercentage(100.0f);
    card->setHeight(AUTO);
    card->setBackgroundColor(rgb(38, 42, 50));
    card->setCornerRadius(10.0f);
    card->setPadding(padding);
    card->setMarginBottom(14.0f);
    return card;
}

// 一行：横向排列，居中，间距
Box* makeRow(float gap = 12.0f, AlignItems align = AlignItems::CENTER)
{
    Box* row = new Box(Axis::ROW);
    row->setWidthPercentage(100.0f);
    row->setAlignItems(align);
    row->setGap(gap);
    return row;
}

// ---------------------------------------------------------------------------
// Tab 栏
// ---------------------------------------------------------------------------

class TabCell : public RecyclerCell
{
  public:
    TabCell()
        : RecyclerCell()
    {
        this->setHeight(38.0f);
        this->setAlignItems(AlignItems::CENTER);
        this->setJustifyContent(JustifyContent::CENTER);
        this->setCornerRadius(8.0f);
        this->setPadding(0.0f, 18.0f, 0.0f, 18.0f);

        this->label = makeText("", 15.0f, rgb(170, 180, 195), true);
        this->label->setHorizontalAlign(HorizontalAlign::CENTER);
        this->addView(this->label);
    }

    void setTab(const std::string& name, bool selected)
    {
        this->label->setText(name);
        this->setSelected(selected);
    }

    void setSelected(bool selected)
    {
        this->selected = selected;
        this->setBackgroundColor(selected ? rgb(48, 86, 140) : rgb(45, 49, 56));
        this->label->setTextColor(selected ? rgb(255, 255, 255) : rgb(170, 180, 195));
    }

    bool isFocusable() const override { return true; }

    void onFocusGained() override
    {
        this->setBorderColor(rgb(120, 190, 255));
        this->setBorderThickness(2.0f);
    }

    void onFocusLost() override
    {
        this->setBorderThickness(0.0f);
    }

  private:
    Label* label  = nullptr;
    bool selected = false;
};

// tab 栏：横向滚动，放在 HScrollingFrame 里
class TabBar : public HScrollingFrame
{
  public:
    TabBar()
    {
        this->setHeight(46.0f);
        this->setShowScrollingIndicator(false);

        this->content = new Box(Axis::ROW);
        this->content->setHeight(46.0f);
        this->content->setGap(8.0f);
        this->setContentView(this->content);
    }

    void addTab(const std::string& name, const std::function<void()>& onSelect,
        const std::function<void()>& onFocus)
    {
        TabCell* cell = new TabCell();
        cell->setTab(name, false);
        cell->registerAction(ControllerButton::A, "切换", [onSelect](View*) { onSelect(); });
        cell->registerAction(ControllerButton::B, "选中", [onSelect](View*) { onSelect(); });
        this->cells.push_back(cell);
        this->content->addView(cell);

        // 焦点移到这个 tab 上时自动选中，符合 tab 的直觉
        this->focusHooks.push_back(onFocus);
    }

    // 内容宽度 = 各 tab 宽度 + 间距，纵向撑满
    void finish()
    {
        float total = 0.0f;
        for (TabCell* cell : this->cells)
            total += cell->getWidth() > 0.0f ? cell->getWidth() : 110.0f;
        total += 8.0f * static_cast<float>(this->cells.size() > 0 ? this->cells.size() - 1 : 0);
        this->content->setWidth(total);
    }

    void setSelectedIndex(size_t index)
    {
        for (size_t i = 0; i < this->cells.size(); ++i)
            this->cells[i]->setSelected(i == index);
    }

    // LEFT/RIGHT 留在 tab 栏里；UP/DOWN 交给内容区。
    View* getNextFocus(FocusDirection direction, View* currentView) override
    {
        if (direction == FocusDirection::UP || direction == FocusDirection::DOWN)
            return HScrollingFrame::getNextFocus(direction, currentView);

        return HScrollingFrame::getNextFocus(direction, currentView);
    }

    void update() override
    {
        HScrollingFrame::update();

        // 焦点落在哪个 tab 上就选中哪个
        View* focus = getInputManager().getCurrentFocus();
        for (size_t i = 0; i < this->cells.size(); ++i)
        {
            if (this->cells[i] == focus && i != this->lastFocusedIndex)
            {
                this->lastFocusedIndex = i;
                this->focusHooks[i]();
            }
        }
    }

  private:
    Box* content = nullptr;
    std::vector<TabCell*> cells;
    std::vector<std::function<void()>> focusHooks;
    size_t lastFocusedIndex = static_cast<size_t>(-1);
};

// ---------------------------------------------------------------------------
// 各页内容
// ---------------------------------------------------------------------------

// 页 1：基础控件
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

// 页 2：文本
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

// 页 3：布局
Box* buildLayoutPage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeight(AUTO);

    page->addView(makeTitle("Flexbox 布局"));
    page->addView(makeHint("全部由 Yoga 计算：主轴方向、分布方式、交叉轴对齐、gap、padding、"
                           "grow/shrink 与最小最大尺寸。改窗口大小会实时重排。"));

    // 主轴分布
    Box* justifyCard = makeCard();
    justifyCard->addView(makeText("justifyContent", 16.0f, rgb(200, 208, 220)));

    struct JustifySpec
    {
        const char* name;
        JustifyContent value;
    };
    const JustifySpec justifies[] = {
        { "FLEX_START", JustifyContent::FLEX_START },
        { "CENTER", JustifyContent::CENTER },
        { "SPACE_BETWEEN", JustifyContent::SPACE_BETWEEN },
    };

    for (const JustifySpec& spec : justifies)
    {
        Box* line = new Box(Axis::COLUMN);
        line->setWidthPercentage(100.0f);
        line->setMarginTop(10.0f);
        line->addView(makeText(spec.name, 13.0f, rgb(160, 170, 185)));

        Box* row = makeRow(0.0f);
        row->setHeight(44.0f);
        row->setMarginTop(5.0f);
        row->setBackgroundColor(rgb(30, 33, 40));
        row->setCornerRadius(6.0f);
        row->setPadding(6.0f);
        row->setJustifyContent(spec.value);

        const ImU32 colors[] = { IM_COL32(57, 96, 145, 255), IM_COL32(70, 125, 100, 255),
            IM_COL32(145, 94, 67, 255) };
        for (int i = 0; i < 3; ++i)
        {
            Box* block = new Box(Axis::COLUMN);
            block->setDimensions(46.0f, 32.0f);
            block->setBackgroundColor(colors[i]);
            block->setCornerRadius(5.0f);
            row->addView(block);
        }
        line->addView(row);
        justifyCard->addView(line);
    }
    page->addView(justifyCard);

    // 交叉轴对齐
    Box* alignCard = makeCard();
    alignCard->addView(makeText("alignItems", 16.0f, rgb(200, 208, 220)));

    struct AlignSpec
    {
        const char* name;
        AlignItems value;
    };
    const AlignSpec alignSpecs[] = {
        { "FLEX_START", AlignItems::FLEX_START },
        { "CENTER", AlignItems::CENTER },
        { "FLEX_END", AlignItems::FLEX_END },
    };

    for (const AlignSpec& spec : alignSpecs)
    {
        Box* line = new Box(Axis::COLUMN);
        line->setWidthPercentage(100.0f);
        line->setMarginTop(10.0f);
        line->addView(makeText(spec.name, 13.0f, rgb(160, 170, 185)));

        Box* row = makeRow(10.0f, spec.value);
        row->setHeight(70.0f);
        row->setMarginTop(5.0f);
        row->setBackgroundColor(rgb(30, 33, 40));
        row->setCornerRadius(6.0f);
        row->setPadding(6.0f);

        const float heights[] = { 56.0f, 30.0f, 44.0f };
        const ImU32 colors[]  = { IM_COL32(57, 96, 145, 255), IM_COL32(70, 125, 100, 255),
            IM_COL32(145, 94, 67, 255) };
        for (int i = 0; i < 3; ++i)
        {
            Box* block = new Box(Axis::COLUMN);
            block->setDimensions(46.0f, heights[i]);
            block->setBackgroundColor(colors[i]);
            block->setCornerRadius(5.0f);
            row->addView(block);
        }
        line->addView(row);
        alignCard->addView(line);
    }
    page->addView(alignCard);

    // grow
    Box* growCard = makeCard();
    growCard->addView(makeText("grow 撑满剩余空间", 16.0f, rgb(200, 208, 220)));

    Box* growRow = makeRow(10.0f);
    growRow->setHeight(80.0f);
    growRow->setMarginTop(10.0f);

    Box* side = new Box(Axis::COLUMN);
    side->setDimensions(110.0f, 80.0f);
    side->setShrink(0.0f);
    side->setBackgroundColor(rgb(57, 96, 145));
    side->setCornerRadius(8.0f);
    Label* sideText = makeText("固定 110", 13.0f, rgb(235, 238, 242), true);
    sideText->setHorizontalAlign(HorizontalAlign::CENTER);
    side->addView(sideText);
    growRow->addView(side);

    Box* grow1 = new Box(Axis::COLUMN);
    grow1->setHeight(80.0f);
    grow1->setGrow(1.0f);
    grow1->setBackgroundColor(rgb(70, 125, 100));
    grow1->setCornerRadius(8.0f);
    Label* growText = makeText("grow = 1", 13.0f, rgb(235, 238, 242), true);
    growText->setHorizontalAlign(HorizontalAlign::CENTER);
    grow1->addView(growText);
    growRow->addView(grow1);

    Box* grow2 = new Box(Axis::COLUMN);
    grow2->setHeight(80.0f);
    grow2->setGrow(2.0f);
    grow2->setBackgroundColor(rgb(145, 94, 67));
    grow2->setCornerRadius(8.0f);
    Label* grow2Text = makeText("grow = 2", 13.0f, rgb(235, 238, 242), true);
    grow2Text->setHorizontalAlign(HorizontalAlign::CENTER);
    grow2->addView(grow2Text);
    growRow->addView(grow2);

    growCard->addView(growRow);
    page->addView(growCard);

    return page;
}

// 页 4：虚拟列表
class ListCell : public RecyclerCell
{
  public:
    ListCell()
    {
        this->setHeight(64.0f);
        this->setAlignItems(AlignItems::CENTER);
        this->setPadding(10.0f);
        this->setGap(12.0f);
        this->setCornerRadius(8.0f);
        this->setBackgroundColor(rgb(38, 42, 50));

        this->thumbnail = new Image();
        this->thumbnail->setDimensions(76.0f, 44.0f);
        this->thumbnail->setCornerRadius(6.0f);
        this->thumbnail->setScalingType(ImageScalingType::FILL);
        this->thumbnail->setImageFromFile("resources/testcard.png");
        this->addView(this->thumbnail);

        Box* textBox = new Box(Axis::COLUMN);
        textBox->setGrow(1.0f);
        textBox->setShrink(1.0f);

        this->title = makeText("", 15.0f, rgb(235, 238, 242), true);
        textBox->addView(this->title);

        this->subtitle = makeText("", 12.0f, rgb(150, 160, 175), true);
        this->subtitle->setMarginTop(3.0f);
        textBox->addView(this->subtitle);

        this->addView(textBox);
    }

    void prepareForReuse() override { this->activated = false; }

    void setIndex(size_t value) override
    {
        RecyclerCell::setIndex(value);
        this->title->setText("列表项 #" + std::to_string(value));
        this->subtitle->setText(this->activated ? "已触发" : "固定 64 高的行，滚出视口后会被回收复用");
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
        this->setBackgroundColor(rgb(38, 42, 50));
        this->setBorderThickness(0.0f);
    }

  private:
    Image* thumbnail = nullptr;
    Label* title     = nullptr;
    Label* subtitle  = nullptr;
    bool activated   = false;
};

class ListSource : public RecyclerDataSource
{
  public:
    explicit ListSource(size_t count)
        : count(count)
    {
    }

    size_t getItemCount() override { return this->count; }
    float getItemHeight(size_t) override { return 64.0f; }

  private:
    size_t count;
};

// outSelfScrolling 置 true 表示这一页自己管滚动（内部已经有 ScrollingFrame），
// 切页时直接铺满内容区，不要再套外层 ScrollingFrame——
// 否则 height:100% 会被外层当成"内容高度"，列表就撑不到底。
Box* buildListPage(RecyclerFrame** outList, bool* outSelfScrolling)
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeightPercentage(100.0f);
    page->setGap(10.0f);
    *outSelfScrolling = true;

    page->addView(makeTitle("虚拟列表"));
    page->addView(makeHint("1000 条数据，滚出视口的行会被回收进对象池，"
                           "新进入视口的行复用池里的控件。左右或上下键切换焦点，滚动条会跟随。"));

    RecyclerFrame* list = new RecyclerFrame();
    list->setWidthPercentage(100.0f);
    list->setGrow(1.0f);
    list->setItemHeight(64.0f);
    list->setPreFetchLine(1);
    list->setShowScrollingIndicator(true);
    list->registerCell(RecyclerDataSource::DEFAULT_CELL_ID, [] { return new ListCell(); });

    // 数据源要活到列表销毁
    static ListSource source(1000);
    list->setDataSource(&source);

    *outList = list;
    page->addView(list);
    return page;
}

// 页 5：布局调试
// 调试页内嵌列表用的简单 cell
class SimpleCell : public RecyclerCell
{
  public:
    SimpleCell()
    {
        this->setAlignItems(AlignItems::CENTER);
        this->setPadding(0.0f, 12.0f, 0.0f, 12.0f);
        this->setBackgroundColor(rgb(38, 42, 50));
        this->setCornerRadius(6.0f);

        this->label = makeText("", 14.0f, rgb(200, 208, 220), true);
        this->label->setGrow(1.0f);
        this->addView(this->label);
    }

    void setIndex(size_t value) override
    {
        RecyclerCell::setIndex(value);
        this->label->setText("内嵌行 #" + std::to_string(value));
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
        this->setBackgroundColor(rgb(38, 42, 50));
        this->setBorderThickness(0.0f);
    }

  private:
    Label* label = nullptr;
};

Box* buildDebugPage(RecyclerFrame** outList, Label** outStats)
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeight(AUTO);

    page->addView(makeTitle("布局与调试"));
    page->addView(makeHint("这一页用来说明调试能力。按 F1 打开调试层，右上角会显示实时布局树、"
                           "当前焦点视图、帧率、图形后端与存活纹理数。"));

    Box* infoCard = makeCard();
    infoCard->addView(makeText("当前运行信息", 16.0f, rgb(200, 208, 220)));

    Label* stats = makeText("", 14.0f, rgb(180, 190, 205));
    stats->setLineHeight(1.6f);
    stats->setMarginTop(8.0f);
    *outStats = stats;
    infoCard->addView(stats);
    page->addView(infoCard);

    Box* tipsCard = makeCard();
    tipsCard->addView(makeText("调试技巧", 16.0f, rgb(200, 208, 220)));

    const char* tips[] = {
        "F1 开关调试层：高亮当前焦点路径，列出布局树每个节点的尺寸与坐标",
        "树里带 > 前缀的那一行就是当前焦点",
        "滚动 / 焦点不对时，先看调试层里那一项的 w/h 是不是 0",
        "recycler 的 cell 数量稳定在视口行数附近，说明回收生效；一直涨就是漏了复用",
    };
    for (const char* tip : tips)
    {
        Box* line = makeRow(8.0f, AlignItems::FLEX_START);
        line->setMarginTop(8.0f);

        Box* dot = new Box(Axis::COLUMN);
        dot->setDimensions(6.0f, 6.0f);
        dot->setMarginTop(7.0f);
        dot->setShrink(0.0f);
        dot->setBackgroundColor(rgb(120, 190, 255));
        dot->setCornerRadius(3.0f);
        line->addView(dot);

        Label* text = makeText(tip, 14.0f, rgb(180, 190, 205));
        text->setGrow(1.0f);
        line->addView(text);

        tipsCard->addView(line);
    }
    page->addView(tipsCard);

    // 顺便在页内嵌一个短列表，方便看回收情况
    Box* cellsCard = makeCard();
    cellsCard->addView(makeText("内嵌短列表（260 条）", 16.0f, rgb(200, 208, 220)));

    RecyclerFrame* inner = new RecyclerFrame();
    inner->setWidthPercentage(100.0f);
    inner->setHeight(220.0f);
    inner->setItemHeight(44.0f);
    inner->setPreFetchLine(1);
    inner->setMarginTop(10.0f);
    inner->registerCell(RecyclerDataSource::DEFAULT_CELL_ID, [] { return new SimpleCell(); });

    static ListSource innerSource(260);
    inner->setDataSource(&innerSource);
    *outList = inner;
    cellsCard->addView(inner);
    page->addView(cellsCard);

    return page;
}

} // namespace

// ---------------------------------------------------------------------------
// 入口
// ---------------------------------------------------------------------------

// 构建整个界面。抽成函数是为了能被无窗口测试复用（验证布局数值）。
static RecyclerFrame* g_mainList = nullptr;
static Label* g_statsLabel = nullptr;

View* buildScene()
{
    // 外层：标题 + tab 栏 + 内容区
    Box* root = new Box(Axis::COLUMN);
    root->setWidthPercentage(100.0f);
    root->setHeightPercentage(100.0f);
    root->setBackgroundColor(rgb(26, 28, 33));
    root->setPadding(20.0f);
    root->setGap(14.0f);

    // 标题
    Box* header = new Box(Axis::ROW);
    header->setWidthPercentage(100.0f);
    header->setAlignItems(AlignItems::CENTER);
    header->setJustifyContent(JustifyContent::SPACE_BETWEEN);

    Label* title = makeText("bklv 能力总览", 26.0f, rgb(255, 255, 255));
    title->setSingleLine(true);
    header->addView(title);

    Label* subtitle = makeText("左右切换标签 · 上下移动焦点 · 回车触发 · F1 调试层", 13.0f,
        rgb(150, 160, 175));
    subtitle->setSingleLine(true);
    subtitle->setHorizontalAlign(HorizontalAlign::RIGHT);
    header->addView(subtitle);
    root->addView(header);

    // 内容区：一个 ScrollingFrame，切 tab 时换里面的内容
    ScrollingFrame* content = new ScrollingFrame();
    content->setWidthPercentage(100.0f);
    content->setGrow(1.0f);
    content->setShowScrollingIndicator(true);

    // 页面
    RecyclerFrame* mainList  = nullptr;
    RecyclerFrame* debugList = nullptr;
    Label* statsLabel        = nullptr;

    std::vector<Box*> pages;
    std::vector<bool> selfScrolling; // true = 这一页自己管滚动
    pages.push_back(buildWidgetsPage());
    selfScrolling.push_back(false);
    pages.push_back(buildTextPage());
    selfScrolling.push_back(false);
    pages.push_back(buildLayoutPage());
    selfScrolling.push_back(false);

    bool listSelfScrolling = false;
    pages.push_back(buildListPage(&mainList, &listSelfScrolling));
    selfScrolling.push_back(listSelfScrolling);

    pages.push_back(buildDebugPage(&debugList, &statsLabel));
    selfScrolling.push_back(false);

    // tab 栏
    TabBar* tabBar = new TabBar();
    tabBar->setWidthPercentage(100.0f);

    const char* tabNames[] = { "控件", "文本", "布局", "列表", "调试" };

    // 内容区容器：滚动内容的页面放在 ScrollingFrame 里，
    // 自己管滚动的页面（列表）直接铺满，避免多层滚动嵌套。
    Box* contentHost = new Box(Axis::COLUMN);
    contentHost->setWidthPercentage(100.0f);
    contentHost->setGrow(1.0f);
    contentHost->addView(content);

    // 切页。
    //
    // 两个要点：
    //   1. pages / selfScrolling 必须**按值**捕获：它们是 buildScene 的局部变量，
    //      而这个 lambda 要活到程序结束（tab 回调持有它）。捕获引用会悬空。
    //   2. 视图在 contentHost 与 ScrollingFrame 之间搬动时，必须先"取走"再"放入"。
    //      ScrollingFrame::setContentView 会销毁旧内容；直接覆盖会让另一方
    //      留下悬空指针（实测崩在 removeView 里的野指针上）。
    auto selectPage = [content, contentHost, tabBar, pages, selfScrolling](size_t index) {
        if (index >= pages.size())
            return;

        tabBar->setSelectedIndex(index);

        Box* holder = contentHost;
        View* target = pages[index];

        // 已经在目标位置就不用动
        if (target->getParent() == holder)
            return;

        // 从旧的父节点取走（ScrollingFrame 用 evict，避免被销毁）
        if (content->getContentView() == target)
            content->evictContentView();
        else if (target->getParent() != nullptr)
            target->getParent()->removeView(target, false);

        // 把 ScrollingFrame 自己也从内容区取走（它是普通页的容器）
        if (content->getParent() == holder)
            holder->removeView(content, false);

        if (selfScrolling[index])
        {
            // 自滚动页（内部已有 ScrollingFrame）：直接铺满内容区
            holder->addView(target);
        }
        else
        {
            // 普通页：交给 ScrollingFrame 包滚动
            holder->addView(content);
            content->setContentView(target);
            content->setContentOffsetY(0.0f, false);
        }
    };

    for (size_t i = 0; i < pages.size(); ++i)
    {
        const size_t index = i;
        tabBar->addTab(tabNames[i], [selectPage, index] { selectPage(index); },
            [selectPage, index] { selectPage(index); });

    }
    tabBar->finish();
    root->addView(tabBar);

    content->setContentView(pages[0]);
    tabBar->setSelectedIndex(0);
    root->addView(contentHost);

    g_mainList   = mainList;
    g_statsLabel = statsLabel;
    return root;
}

int main(int argc, char** argv)
{
    App app;
    app.setRootView(buildScene());

    // 每帧刷新调试页上的统计信息
    app.onUpdate = []() {
        Label* statsLabel = g_statsLabel;
        RecyclerFrame* mainList = g_mainList;
        if (statsLabel == nullptr)
            return;

        static float timer = 0.0f;
        timer += ImGui::GetIO().DeltaTime;
        if (timer < 0.25f)
            return;
        timer = 0.0f;

        char text[512];
        std::snprintf(text, sizeof(text),
            "图形后端：%s\n"
            "存活纹理：%d\n"
            "帧率：%.0f FPS\n"
            "主列表当前显示的 cell：%zu（固定 1000 条数据）\n"
            "主列表控件总数：%zu",
            getGraphicsBackendName(), getLiveTextureCount(), ImGui::GetIO().Framerate,
            mainList != nullptr ? mainList->getVisibleCellCount() : 0,
            mainList != nullptr ? mainList->getCreatedCellCount() : 0);
        statsLabel->setText(text);
    };

    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "-v")
            app.setDebugLayerEnabled(true);
    }

    return app.run("bklv 能力总览", 1280, 800);
}
