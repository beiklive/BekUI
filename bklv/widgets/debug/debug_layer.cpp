#include "bklv/widgets/debug/debug_layer.hpp"

#include <cstdio>

#include "bklv/view/box.hpp"
#include "bklv/media/font.hpp"
#include "bklv/theme/theme_data.hpp"

namespace bklv
{

void collectDebugBounds(View* root, std::vector<DebugBounds>& out, int maxDepth, int depth)
{
    if (root == nullptr || depth > maxDepth)
        return;

    if (root->getVisibility() == Visibility::GONE)
        return;

    const Rect rect = root->getFrame();
    if (rect.width > 0.0f && rect.height > 0.0f)
    {
        DebugBounds item;
        item.rect      = rect;
        item.className = root->getClassString();
        item.depth     = depth;
        out.push_back(item);
    }

    if (Box* box = root->asBox())
        for (View* child : box->getChildren())
            collectDebugBounds(child, out, maxDepth, depth + 1);
}

DebugLayer::DebugLayer()
{
    // 铺满父容器，浮在最上层
    this->setWidthPercentage(100.0f);
    this->setHeightPercentage(100.0f);
    this->setPositionType(PositionType::ABSOLUTE);
    this->setPositionLeft(0.0f);
    this->setPositionTop(0.0f);

    this->applyTheme();
}

void DebugLayer::applyTheme()
{
    const ThemeData& theme = getTheme();
    this->boundsColor = theme.color("color.debug.bounds", IM_COL32(0, 255, 204, 140));
    this->textColor   = theme.color("color.debug.text", IM_COL32(255, 255, 255, 200));
}

void DebugLayer::draw(ImDrawList* drawList, float x, float y, float width, float height,
    FrameContext*)
{
    if (drawList == nullptr || this->root == nullptr)
        return;

    this->bounds.clear();
    collectDebugBounds(this->root, this->bounds);

    ImFont* font = getDefaultFont();

    for (const DebugBounds& item : this->bounds)
    {
        drawList->AddRect(ImVec2(item.rect.x, item.rect.y),
            ImVec2(item.rect.x + item.rect.width, item.rect.y + item.rect.height),
            this->boundsColor, 0.0f, 0, 1.0f);

        // 框太小就不写字了（写了也看不清）
        if (!this->showText || font == nullptr || item.rect.width < 70.0f
            || item.rect.height < 16.0f)
            continue;

        char label[96] = { 0 };
        std::snprintf(label, sizeof(label), "%s %.0fx%.0f", item.className.c_str(),
            item.rect.width, item.rect.height);

        drawList->AddText(font, 13.0f, ImVec2(item.rect.x + 3.0f, item.rect.y + 2.0f),
            this->textColor, label);
    }

    // 顺带把左上角坐标画出来，方便对着日志看
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

} // namespace bklv
