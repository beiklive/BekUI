#include "app/pages.hpp"

#include <cmath>

#include "bklv/bklv.hpp"
#include "bklv/glass/glass_service.hpp"
#include "bklv/widgets/appearance_view.hpp"

#include "app/ui.hpp"

using namespace bklv;

namespace demo
{

// 玻璃展示页：左边一块"玻璃卡"，右边一块同样大小的**不透明卡**，两者并排、压在彩色背景上。
//
// 玻璃卡这里**不写一行玻璃代码** —— 它用的外观 `base/glass_panel` 里有一层 `op: "glass"`。
// 视图只负责给标题文字，以及"后端不可用 / 关掉玻璃"时的回退底色。
//
// 验收方式：
//   · 玻璃卡区域：开/关玻璃两张截图必须有差异；卡内"无文字区"的横向梯度要**远低于**壁纸
//     （说明采到了背景并被模糊），同时标题亮像素 > 0（说明内容压在玻璃之上）
//   · 不透明卡区域：两张截图**逐像素一致**（opt-in 的保证）
//
// ⚠️ 背景必须**先画**：玻璃采的是"这一帧到此刻已经画好的内容"，背景在后就没有东西可采。

namespace
{

// 彩色背景：渐变 + 斜条纹 + 两个柔光圆。
// 斜条纹是必须的 —— 只铺渐变时模糊几乎看不出来（深色底 + 低 alpha 等于没有背景）。
class WallpaperView : public View
{
  public:
    WallpaperView()
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

// 玻璃卡：外观 `base/glass_panel` 里的 `op: "glass"` 层负责"这块要玻璃"。
class GlassPanelView : public AppearanceView
{
  public:
    GlassPanelView(std::string cardTitle, float cornerRadius)
        : AppearanceView("base/glass_panel", false)
        , title(std::move(cardTitle))
        , radius(cornerRadius)
    {
        this->setCornerRadius(cornerRadius);
    }

  protected:
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        FrameContext* ctx) override
    {
        // 玻璃不可用时给块不透明底：页面照样能读（玻璃层自己什么都不画）
        glass::GlassService& service = glass::getGlassService();
        if (!service.isEnabled() || !service.isSupported())
        {
            drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height),
                IM_COL32(46, 50, 62, 255), this->radius);
        }

        AppearanceView::draw(drawList, x, y, width, height, ctx);
    }

    std::string getLayerText(const std::string& layerId) override
    {
        return layerId == "title" ? this->title : std::string();
    }

  private:
    std::string title;
    float radius = 14.0f;
};

// 不透明卡：同样的尺寸与圆角，但**没有任何玻璃层** —— 证明"没声明就不受影响"
Rectangle* makeOpaqueCard()
{
    Rectangle* card = makeCard();
    card->addView(makeText(getStr("glass/opaque"),
        getTheme().number("metric.font.size.sm", 15.0f), getTheme().color("color.text.primary")));
    return card;
}

} // namespace

Box* buildGlassPage()
{
    Box* page = makeColumn(getTheme().number("metric.spacing.md", 12.0f));

    page->addView(new WallpaperView());
    page->addView(makeHint(getStr("glass/hint")));

    Box* row = makeRow(getTheme().number("metric.spacing.md", 12.0f));
    row->setHeight(200.0f);

    // 纯容器没有固有宽度：行里只给 grow 会排成 0 宽，必须显式给宽度/高度（踩过）
    GlassPanelView* glassCard = new GlassPanelView(getStr("glass/glass_card"),
        getTheme().number("metric.component.radius", 6.0f) * 2.0f);
    glassCard->setHeight(200.0f);
    glassCard->setWidthPercentage(48.0f);
    row->addView(glassCard);

    Rectangle* opaque = makeOpaqueCard();
    opaque->setGrow(1.0f);
    row->addView(opaque);

    page->addView(row);
    page->addView(makeHint(getStr("glass/backend_hint")));
    return page;
}

} // namespace demo
