#include "bklv/views/debug_layer.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/core/font.hpp"
#include "bklv/core/graphics.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/view.hpp"

namespace bklv
{

namespace
{

constexpr float PANEL_WIDTH = 340.0f;
constexpr float PADDING     = 10.0f;
constexpr float LINE_HEIGHT = 16.0f;
constexpr float FONT_SIZE   = 12.0f;
constexpr size_t MAX_LINES  = 60;

struct TreeLine
{
    int depth = 0;
    bool focused = false;
    bool onFocusPath = false;
    std::string text;
};

// 这个视图是否在当前焦点路径上（自己或祖先就是当前焦点）。
bool isOnFocusPath(View* view)
{
    if (view == nullptr)
        return false;

    View* focus = getInputManager().getCurrentFocus();
    for (View* v = focus; v != nullptr; v = v->getParent())
    {
        if (v == view)
            return true;
    }
    return false;
}

void collectLines(View* view, int depth, std::vector<TreeLine>& out)
{
    if (view == nullptr || out.size() >= MAX_LINES)
        return;

    char buffer[256];
    std::string class_name = view->getClassString();
    const size_t pos       = class_name.find("::");
    if (pos != std::string::npos)
        class_name = class_name.substr(pos + 2);

    const bool focused = getInputManager().getCurrentFocus() == view;

    std::snprintf(buffer, sizeof(buffer), "%s%s%s  %.0fx%.0f @%.0f,%.0f", focused ? "> " : "",
        view->getId().empty() ? "" : (view->getId() + " ").c_str(), class_name.c_str(), view->getWidth(),
        view->getHeight(), view->getX(), view->getY());

    TreeLine line;
    line.depth       = depth;
    line.focused     = focused;
    line.onFocusPath = isOnFocusPath(view);
    line.text        = buffer;
    out.push_back(line);

    if (Box* box = dynamic_cast<Box*>(view))
    {
        for (View* child : box->getChildren())
            collectLines(child, depth + 1, out);
    }
}

} // namespace

void drawDebugLayer(ImDrawList* drawList, View* root, float screenWidth)
{
    ImFont* font = getDefaultFont();
    if (font == nullptr || root == nullptr)
        return;

    std::vector<TreeLine> lines;
    collectLines(root, 0, lines);

    // 顶部两行固定信息
    View* focus = getInputManager().getCurrentFocus();
    const Point pointer = getInputManager().getPointerPosition();

    char header[320];
    std::snprintf(header, sizeof(header), "fps %.0f  backend %s  tex %d", ImGui::GetIO().Framerate,
        getTextureBackendName(), getLiveTextureCount());

    char focusLine[256];
    std::snprintf(focusLine, sizeof(focusLine), "pointer %.0f,%.0f   focus %s", pointer.x, pointer.y,
        focus != nullptr ? focus->getClassString().c_str() : "(none)");

    const float panelHeight = (static_cast<float>(lines.size()) + 3.0f) * LINE_HEIGHT + PADDING * 2.0f;
    const float panelX      = screenWidth - PANEL_WIDTH - 10.0f;
    const float panelY      = 10.0f;

    drawList->AddRectFilled(ImVec2(panelX, panelY), ImVec2(panelX + PANEL_WIDTH, panelY + panelHeight),
        IM_COL32(14, 16, 20, 225), 8.0f);
    drawList->AddRect(ImVec2(panelX, panelY), ImVec2(panelX + PANEL_WIDTH, panelY + panelHeight),
        IM_COL32(80, 88, 100, 255), 8.0f, 0, 1.0f);

    float y = panelY + PADDING;

    // 标题
    drawList->AddText(font, FONT_SIZE + 1.0f, ImVec2(panelX + PADDING, y), IM_COL32(240, 200, 120, 255),
        "bklv debug");
    y += LINE_HEIGHT;

    drawList->AddText(font, FONT_SIZE, ImVec2(panelX + PADDING, y), IM_COL32(170, 180, 195, 255), header);
    y += LINE_HEIGHT;

    drawList->AddText(font, FONT_SIZE, ImVec2(panelX + PADDING, y), IM_COL32(170, 180, 195, 255), focusLine);
    y += LINE_HEIGHT;

    // 布局树
    for (const TreeLine& line : lines)
    {
        const float x = panelX + PADDING + static_cast<float>(line.depth) * 8.0f;

        // 当前焦点用亮蓝，焦点路径上的祖先用浅色，其余暗色。
        ImU32 color = IM_COL32(140, 150, 165, 255);
        if (line.focused)
            color = IM_COL32(255, 220, 140, 255);
        else if (line.onFocusPath)
            color = IM_COL32(120, 200, 255, 255);

        drawList->AddText(font, FONT_SIZE, ImVec2(x, y), color, line.text.c_str());
        y += LINE_HEIGHT;
    }
}

} // namespace bklv
