#include "app/ui.hpp"

#include "bklv/bklv.hpp"

using namespace bklv;

namespace demo
{

Label* makeText(const std::string& text, float size, ImU32 color, bool singleLine)
{
    Label* label = new Label();
    label->setText(text);
    label->setFontSize(size);
    label->setTextColor(color);
    label->setSingleLine(singleLine);
    return label;
}

Label* makeHint(const std::string& text)
{
    Label* label = new Label();
    label->setText(text);
    label->setFontSizeExplicit(false);
    label->setTextColorExplicit(false);
    label->setSingleLine(false);
    label->setLineHeight(getTheme().number("metric.font.lineHeight", 1.4f));
    return label;
}

Rectangle* makeCard()
{
    return makeCard("card");
}

Rectangle* makeCard(const std::string& variant)
{
    Rectangle* card = new Rectangle(variant);
    card->setWidthPercentage(100.0f);
    card->setHeight(AUTO);
    card->setPadding(getTheme().number("metric.card.padding", 16.0f));
    card->setGap(getTheme().number("metric.spacing.sm", 8.0f));
    return card;
}

Box* makeRow(float gap, AlignItems align)
{
    Box* row = new Box(Axis::ROW);
    row->setWidthPercentage(100.0f);
    row->setHeight(AUTO);
    row->setGap(gap);
    row->setAlignItems(align);
    return row;
}

Box* makeColumn(float gap)
{
    Box* column = new Box(Axis::COLUMN);
    column->setWidthPercentage(100.0f);
    column->setHeight(AUTO);
    column->setGap(gap);
    return column;
}

} // namespace demo

namespace demo
{
namespace
{

// 玻璃背景：渐变 + 斜条纹 + 两个柔光圆。
// 斜条纹是必须的 —— 只铺渐变时模糊几乎看不出来；纯色深底上玻璃等于没效果。
class GlassWallpaperView : public View
{
  public:
    GlassWallpaperView()
    {
        this->setPositionType(PositionType::ABSOLUTE);
        this->setPositionTop(0.0f);
        this->setPositionRight(0.0f);
        this->setPositionBottom(0.0f);
        this->setPositionLeft(0.0f);
    }

  protected:
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        FrameContext* ctx) override
    {
        (void)ctx;
        if (width <= 0.0f || height <= 0.0f)
            return;

        drawList->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + width, y + height),
            IM_COL32(58, 96, 200, 255), IM_COL32(120, 70, 190, 255),
            IM_COL32(170, 60, 150, 255), IM_COL32(40, 110, 190, 255));

        const float stripe = 5.0f;
        drawList->PushClipRect(ImVec2(x, y), ImVec2(x + width, y + height), true);
        for (float offset = -height; offset < width; offset += stripe * 2.0f)
        {
            drawList->AddQuadFilled(ImVec2(x + offset, y + height),
                ImVec2(x + offset + stripe, y + height),
                ImVec2(x + offset + stripe + height, y), ImVec2(x + offset + height, y),
                IM_COL32(255, 255, 255, 26));
        }
        drawList->PopClipRect();

        const float radius = std::min(width, height) * 0.42f;
        drawList->AddCircleFilled(ImVec2(x + width * 0.28f, y + height * 0.55f), radius,
            IM_COL32(90, 190, 255, 90), 64);
        drawList->AddCircleFilled(ImVec2(x + width * 0.72f, y + height * 0.40f), radius * 0.8f,
            IM_COL32(255, 120, 220, 90), 64);
    }
};

} // namespace

View* makeGlassWallpaper()
{
    return new GlassWallpaperView();
}

} // namespace demo
