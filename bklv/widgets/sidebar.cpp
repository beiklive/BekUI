#include "bklv/widgets/sidebar.hpp"

#include <utility>

#include "bklv/core/attributes.hpp"
#include "bklv/core/shadow.hpp"
#include "bklv/core/glass/glass_service.hpp"
#include "bklv/core/theme_data.hpp"

namespace bklv
{

Sidebar::Sidebar(const std::string& title)
    : Box(Axis::COLUMN)
    , title(title)
{
    // 面板样式（宽/不压缩/内边距/间距/底色/圆角）全在 widgets/sidebar.json 的 box 段里。
    // 高度写的是 auto：父容器会 stretch 它，写 100% 而父高度又由内容决定时会形成死循环
    // （踩过：底栏被挤出可视区）。

    this->headerBox = new Box(Axis::ROW);
    this->headerBox->setWidthPercentage(100.0f);
    this->headerBox->setHeight(AUTO);
    this->headerBox->setPadding(
        getTheme().number("metric.spacing.sm", 8.0f), getTheme().number("metric.spacing.sm", 8.0f),
        getTheme().number("metric.spacing.sm", 8.0f), getTheme().number("metric.spacing.sm", 8.0f));
    this->addView(this->headerBox);

    // 标题用 Label（语义控件），颜色跟随主题
    this->titleLabel = new Label();
    this->titleLabel->setText(title);
    this->titleLabel->setFontSizeExplicit(false);
    this->titleLabel->setTextColorExplicit(false);
    this->headerBox->addView(this->titleLabel);

    // 条目放进可滚动容器：条目高度是 borealis 的 70，页数多了会超出可视区，
    // 滚动条会跟着焦点走（ScrollingFrame 自带"焦点必须在可视区内"）
    this->itemScroller = new ScrollingFrame();
    this->itemScroller->setWidthPercentage(100.0f);
    this->itemScroller->setGrow(1.0f);
    this->itemScroller->setShowScrollingIndicator(true);
    this->itemScroller->setScrollingBehavior(ScrollingBehavior::FOCUS);
    this->addView(this->itemScroller);

    this->itemBox = new Box(Axis::COLUMN);
    this->itemBox->setWidthPercentage(100.0f);
    this->itemBox->setHeight(AUTO);
    this->itemBox->setGap(getTheme().number("metric.spacing.xs", 4.0f));
    this->itemScroller->setContentView(this->itemBox);

    this->applyPanelLook();
}

void Sidebar::applyPanelLook()
{
    // 换肤/改缩放时重新套用（值都在 widgets/sidebar.json 的 box 段里）
    applyWidgetBox(this, "widget/sidebar");
}

void Sidebar::setPanelWidth(float width)
{
    this->setWidth(width);
}

void Sidebar::addItem(const std::string& itemTitle, std::function<void()> onActivate,
    std::function<void()> onConfirm)
{
    this->addItem(itemTitle, std::string(), std::move(onActivate), std::move(onConfirm));
}

void Sidebar::addItem(const std::string& itemTitle, const std::string& icon,
    std::function<void()> onActivate, std::function<void()> onConfirm)
{
    const size_t index = this->cells.size();

    TabCell* cell = new TabCell();
    cell->setTitle(itemTitle);
    if (!icon.empty())
        cell->setIcon(icon);
    // 侧栏条目比 tab 格子高（borealis 是 70）：走 tab_cell 的 "sidebar" 变体
    // 过渡期：Sidebar 还没并入 TabFrame，这里先把外观名钉回旧的那份
    // （P3 会删除 Sidebar，届时这条路一起没了）
    cell->setAppearanceName("base/tab_cell");
    cell->setAppearanceVariant("sidebar");
    // 聚焦 = 激活：直接切子页面（borealis 的语义）
    cell->setOnFocus([this, index, onActivate] {
        this->setSelectedIndex(index);
        if (onActivate)
            onActivate();
    });

    // A / 点击 = 确认：把焦点交给子页面的默认控件
    cell->setOnSelect([this, index, onConfirm] {
        this->setSelectedIndex(index);
        if (onConfirm)
            onConfirm();
    });

    this->itemBox->addView(cell);
    this->cells.push_back(cell);

    if (index == this->selected)
        cell->setSelected(true);
}

void Sidebar::addSeparator()
{
    const float height = getTheme().number("metric.sidebar.separatorHeight", 18.0f);

    Box* spacer = new Box(Axis::COLUMN);
    spacer->setHeight(height);
    spacer->setShrink(0.0f);
    spacer->setJustifyContent(JustifyContent::CENTER);
    spacer->setAlignItems(AlignItems::CENTER);

    // 一条 60% 宽的细线，用主题里的分隔色（这个 token 以前没人用）
    const ImU32 color = getTheme().color("color.sidebar.separator", 0x515151FF);
    if ((color >> 24) != 0)
    {
        Box* line = new Box(Axis::ROW);
        line->setHeight(getTheme().number("metric.sidebar.separatorThickness", 1.0f));
        line->setWidthPercentage(60.0f);
        line->setBackgroundColor(color);
        spacer->addView(line);
    }

    this->itemBox->addView(spacer);
    this->separators.push_back(spacer);
}

void Sidebar::draw(ImDrawList* drawList, float x, float y, float width, float height,
    FrameContext*)
{
    // 面板底 = 玻璃（和内容卡片同一个观感；tint 与降级底都用主题里的 sidebar.background）
    if (drawList != nullptr && width > 0.0f && height > 0.0f)
    {
        const ImU32 fill   = getTheme().color("color.sidebar.background", 0x1F2227E0);
        const float radius = this->getCornerRadius();

        glass::GlassService& service = glass::getGlassService();
        bool submitted               = false;
        if (service.isEnabled() && service.isSupported())
        {
            const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
            const float density = scale.x > 0.0f ? scale.x : 1.0f;

            glass::GlassLayerOptions options;
            options.hasTint = true;
            options.tint    = ImGui::ColorConvertU32ToFloat4(fill);
            submitted       = service.submitBackdrop(drawList,
                ImVec4(x * density, y * density, (x + width) * density, (y + height) * density),
                glass::CornerRadii::all(radius), glass::GlassShapeKind::ROUNDED_RECT, density,
                &options);
        }
        if (!submitted && (fill >> 24) != 0)
        {
            // 玻璃不可用（后端没起来 / 关掉了）：退回普通半透明底，面板照样看得清
            drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), fill, radius);
        }
    }

    const float feather = getTheme().number("metric.sidebar.shadowFeather", 0.0f);
    if (drawList == nullptr || feather <= 0.0f || width <= 0.0f || height <= 0.0f)
        return;

    const ImU32 color = getTheme().color("color.common.shadow", 0x00000059);
    if ((color >> 24) == 0)
        return;

    // cutout：只画面板外圈，内圈一个顶点都不落 —— 所以不会把自己的底色压暗
    shadow::drawSoft(drawList, x, y, x + width, y + height, this->getCornerRadius(), color, 0.9f,
        feather, 0.0f, true);
}

void Sidebar::clearItems()
{
    for (TabCell* cell : this->cells)
        this->itemBox->removeView(cell, true);
    for (View* separator : this->separators)
        this->itemBox->removeView(separator, true);
    this->cells.clear();
    this->separators.clear();
    this->selected = 0;
}

void Sidebar::setTitle(const std::string& value)
{
    this->title = value;
    if (this->titleLabel != nullptr)
        this->titleLabel->setText(value);
}

void Sidebar::setItemTitle(size_t index, const std::string& itemTitle)
{
    if (index >= this->cells.size())
        return;
    this->cells[index]->setTitle(itemTitle);
}

std::string Sidebar::getItemTitle(size_t index) const
{
    if (index >= this->cells.size())
        return {};
    return this->cells[index]->getText();
}

View* Sidebar::getItemView(size_t index) const
{
    if (index >= this->cells.size())
        return nullptr;
    return this->cells[index];
}

void Sidebar::setSelectedIndex(size_t index)
{
    if (index >= this->cells.size())
        return;

    this->selected = index;
    for (size_t i = 0; i < this->cells.size(); i++)
        this->cells[i]->setSelected(i == index);
}

const AttributeTable& Sidebar::attributes() const
{
    static const AttributeTable table = {
        { "title", AttributeKind::TEXT, [](View* v, const ResolvedValue& r) {
             static_cast<Sidebar*>(v)->setTitle(r.text);
         }, nullptr },
        { "panelWidth", AttributeKind::NUMBER, [](View* v, const ResolvedValue& r) {
             static_cast<Sidebar*>(v)->setPanelWidth(r.number);
         }, nullptr, true },
    };
    return table;
}

} // namespace bklv
