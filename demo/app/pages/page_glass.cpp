#include "app/pages.hpp"

#include <cmath>

#include "bklv/bklv.hpp"
#include "bklv/glass/glass_service.hpp"

#include "app/ui.hpp"

using namespace bklv;

namespace demo
{

// 玻璃展示页：左边一块"玻璃卡"，右边一块同样大小的**不透明卡**，两者并排、压在彩色背景上。
//
// 验收方式（阶段 1 的硬要求）：
//   · 玻璃卡区域：开/关玻璃两张截图的**像素必须有差异**（否则玻璃没生效）
//   · 不透明卡区域：两张截图**逐像素一致**（opt-in 的保证：没声明玻璃就不受影响）
//   · 玻璃卡内部的像素标准差明显 > 0（说明它采到了背景，而不是一块死板的纯色）
//
// ⚠️ 背景必须**先画**：玻璃采的是"这一帧到此刻已经画好的内容"，背景在后就没有东西可采
//    （设计器踩过：采到一片黑，面板看着就是黑的）。

namespace
{

// 彩色背景：竖直色带 + 两个柔光圆。够亮才看得出玻璃（深色底 + 低 alpha 等于没有背景）。
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

        // 上蓝下紫的渐变
        drawList->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + width, y + height),
            IM_COL32(58, 96, 200, 255), IM_COL32(120, 70, 190, 255),
            IM_COL32(170, 60, 150, 255), IM_COL32(40, 110, 190, 255));

        // 斜条纹：模糊只在**有高频细节**的地方看得出来。
        // 只铺渐变的话玻璃几乎是隐形的（设计器那份文档里也踩过：深色底 + 低 alpha 等于没背景）。
        const float stripe = 5.0f;
        drawList->PushClipRect(ImVec2(x, y), ImVec2(x + width, y + height), true);
        for (float offset = -height; offset < width; offset += stripe * 2.0f)
        {
            drawList->AddQuadFilled(ImVec2(x + offset, y + height), ImVec2(x + offset + stripe, y + height),
                ImVec2(x + offset + stripe + height, y), ImVec2(x + offset + height, y),
                IM_COL32(255, 255, 255, 26));
        }
        drawList->PopClipRect();

        // 两个柔光圆：玻璃的模糊/折射在它们上面最明显
        const float radius = std::min(width, height) * 0.42f;
        drawList->AddCircleFilled(ImVec2(x + width * 0.28f, y + height * 0.55f), radius,
            IM_COL32(90, 190, 255, 90), 64);
        drawList->AddCircleFilled(ImVec2(x + width * 0.72f, y + height * 0.40f), radius * 0.8f,
            IM_COL32(255, 120, 220, 90), 64);
    }
};

// 玻璃卡：在 draw 里声明"这一块要玻璃"。
// （阶段 4 会把它换成外观层 `op: "glass"`，那时视图连这段代码都不用写。）
class GlassCardView : public Box
{
  public:
    GlassCardView(std::string title, float radius)
        : title(std::move(title))
        , radius(radius)
    {
        this->setAxis(Axis::COLUMN);
    }

  protected:
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        FrameContext* ctx) override
    {
        (void)ctx;

        glass::GlassService& service = glass::getGlassService();
        const bool active = service.isEnabled() && service.isSupported();

        if (active)
        {
            // 物理像素：着色器与 GL 视口都按帧缓冲像素算
            const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
            const float density = scale.x > 0.0f ? scale.x : 1.0f;

            glass::GlassRequest request;
            request.minMax            = ImVec4(x * density, y * density, (x + width) * density,
                (y + height) * density);
            request.radii             = glass::CornerRadii::all(this->radius * density);
            request.shape             = glass::GlassShapeKind::ROUNDED_RECT;
            request.density           = density;
            request.drawList          = drawList;
            // 注入点：玻璃画在"这张卡自己的内容"之前 —— 文字仍然压在最上面
            request.drawCommandOffset = drawList->CmdBuffer.Size;
            service.submit(request);
        }
        else
        {
            // 后端不可用 / 关掉时给块不透明底，页面照样能读
            drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height),
                IM_COL32(46, 50, 62, 255), this->radius);
        }

        drawList->AddText(ImVec2(x + 16.0f, y + 14.0f), IM_COL32(240, 246, 255, 235),
            this->title.c_str());
    }

  private:
    std::string title;
    float radius = 14.0f;
};

// 不透明卡：同样的尺寸与圆角，但**不声明玻璃** —— 用来证明"没声明就不受影响"
Rectangle* makeOpaqueCard()
{
    Rectangle* card = makeCard();
    card->addView(makeText(getStr("glass/opaque"), getTheme().number("metric.font.size.sm", 15.0f),
        getTheme().color("color.text.primary")));
    return card;
}

} // namespace

Box* buildGlassPage()
{
    Box* page = makeColumn(getTheme().number("metric.spacing.md", 12.0f));

    // 背景先画（绝对定位铺满整页，排在最前面）
    page->addView(new WallpaperView());

    page->addView(makeHint(getStr("glass/hint")));

    Box* row = makeRow(getTheme().number("metric.spacing.md", 12.0f));
    row->setHeight(200.0f);

    GlassCardView* glassCard = new GlassCardView(getStr("glass/glass_card"),
        getTheme().number("metric.component.radius", 6.0f) * 2.0f);
    // 纯 View 没有固有尺寸：行里不给高度的话它会被排成 0x0（踩过）
    glassCard->setHeight(200.0f);
    glassCard->setWidthPercentage(48.0f);   // 显式给宽度：纯容器没有固有宽度，grow 在行里拿不到空间
    row->addView(glassCard);

    Rectangle* opaque = makeOpaqueCard();
    opaque->setGrow(1.0f);
    row->addView(opaque);

    page->addView(row);
    page->addView(makeHint(getStr("glass/backend_hint")));
    return page;
}

} // namespace demo
