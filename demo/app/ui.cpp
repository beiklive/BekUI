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

// 玻璃背景：深色底 + 彩色线网格。
// 网格线是必须的 —— 纯色深底上玻璃模糊看不出来。
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

        // 深色底 + 彩色线网格。
        // 玻璃要能被看出"磨砂"，底下就得有细结构：纯色深底上模糊等于没效果（踩过），
        // 所以网格线不是装饰，是让玻璃有效果的必要条件。
        drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height),
            IM_COL32(11, 14, 20, 255));

        static const ImU32 kGridColors[4] = {
            IM_COL32(0, 255, 204, 46),   // 青
            IM_COL32(130, 90, 255, 42),  // 紫
            IM_COL32(255, 90, 190, 36),  // 粉
            IM_COL32(70, 150, 255, 42),  // 蓝
        };

        const float cell = 46.0f;
        drawList->PushClipRect(ImVec2(x, y), ImVec2(x + width, y + height), true);

        int column = 0;
        for (float gx = x; gx <= x + width; gx += cell, column++)
            drawList->AddLine(ImVec2(gx, y), ImVec2(gx, y + height),
                kGridColors[column % 4], 1.0f);

        int row = 0;
        for (float gy = y; gy <= y + height; gy += cell, row++)
            drawList->AddLine(ImVec2(x, gy), ImVec2(x + width, gy),
                kGridColors[(row + 2) % 4], 1.0f);

        // 中间一层很淡的柔光，让网格有纵深（网格本身仍清楚）
        const float radius = std::min(width, height) * 0.55f;
        drawList->AddCircleFilled(ImVec2(x + width * 0.5f, y + height * 0.45f), radius,
            IM_COL32(90, 130, 220, 26), 64);

        drawList->PopClipRect();

    }
};

} // namespace

View* makeGlassWallpaper()
{
    return new GlassWallpaperView();
}

} // namespace demo
