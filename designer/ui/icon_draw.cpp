#include "designer/ui/icon_draw.hpp"

#include "designer/core/icon_align.hpp"

#include <cmath>

namespace designer
{

void drawIcon(ImDrawList* list, Icon icon, ImVec2 c, float size, ImU32 color)
{
    if (list == nullptr || icon == Icon::NONE)
        return;

    const float r = size * 0.5f;
    const float t = 1.3f; // 线宽

    switch (icon)
    {
        case Icon::WIDGET:
            list->AddRect(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), color, 0.0f, 0, t);
            break;
        case Icon::PAGE:
            list->AddRect(ImVec2(c.x - r * 0.8f, c.y - r), ImVec2(c.x + r * 0.8f, c.y + r), color,
                0.0f, 0, t);
            list->AddLine(ImVec2(c.x - r * 0.3f, c.y - r * 0.3f),
                ImVec2(c.x + r * 0.4f, c.y - r * 0.3f), color, t);
            list->AddLine(ImVec2(c.x - r * 0.3f, c.y + r * 0.3f),
                ImVec2(c.x + r * 0.4f, c.y + r * 0.3f), color, t);
            break;
        case Icon::TABLE:
            for (int i = -1; i <= 1; i++)
            {
                const float y = c.y + (float)i * r * 0.65f;
                list->AddLine(ImVec2(c.x - r, y), ImVec2(c.x + r, y), color, t);
            }
            break;
        case Icon::LAYER:
            list->AddLine(ImVec2(c.x, c.y - r * 0.85f), ImVec2(c.x + r, c.y), color, t);
            list->AddLine(ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r * 0.85f), color, t);
            list->AddLine(ImVec2(c.x, c.y + r * 0.85f), ImVec2(c.x - r, c.y), color, t);
            list->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x, c.y - r * 0.85f), color, t);
            break;
        case Icon::STATE:
            list->AddCircle(c, r * 0.85f, color, 0, t);
            break;
        case Icon::FIELD:
            list->AddLine(ImVec2(c.x - r * 0.8f, c.y), ImVec2(c.x + r * 0.8f, c.y), color, t);
            list->AddLine(ImVec2(c.x, c.y - r * 0.8f), ImVec2(c.x, c.y + r * 0.8f), color, t);
            break;
        case Icon::EYE:
            list->AddCircle(c, r * 0.55f, color, 0, t);
            list->AddCircle(c, r * 0.9f, color, 12, t * 0.7f, ImDrawFlags_None);
            break;
        case Icon::LAYERS_ONLY:
            list->AddLine(ImVec2(c.x, c.y - r * 0.9f), ImVec2(c.x + r * 0.7f, c.y - r * 0.35f),
                color, t);
            list->AddLine(ImVec2(c.x + r * 0.7f, c.y - r * 0.35f), ImVec2(c.x, c.y + r * 0.2f),
                color, t);
            list->AddLine(ImVec2(c.x, c.y + r * 0.2f), ImVec2(c.x - r * 0.7f, c.y - r * 0.35f),
                color, t);
            list->AddLine(ImVec2(c.x - r * 0.7f, c.y - r * 0.35f), ImVec2(c.x, c.y - r * 0.9f),
                color, t);
            list->AddLine(ImVec2(c.x - r * 0.7f, c.y + r * 0.45f),
                ImVec2(c.x, c.y + r * 0.95f), color, t);
            list->AddLine(ImVec2(c.x, c.y + r * 0.95f), ImVec2(c.x + r * 0.7f, c.y + r * 0.45f),
                color, t);
            break;
        case Icon::EXPAND:
            list->AddLine(ImVec2(c.x - r * 0.7f, c.y - r * 0.15f), ImVec2(c.x, c.y - r * 0.8f),
                color, t);
            list->AddLine(ImVec2(c.x + r * 0.7f, c.y - r * 0.15f), ImVec2(c.x, c.y - r * 0.8f),
                color, t);
            list->AddLine(ImVec2(c.x - r * 0.7f, c.y + r * 0.15f), ImVec2(c.x, c.y + r * 0.8f),
                color, t);
            list->AddLine(ImVec2(c.x + r * 0.7f, c.y + r * 0.15f), ImVec2(c.x, c.y + r * 0.8f),
                color, t);
            break;
        case Icon::COLLAPSE:
            list->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), color, t);
            break;
        case Icon::GRID:
            list->AddLine(ImVec2(c.x - r, c.y - r * 0.35f), ImVec2(c.x + r, c.y - r * 0.35f), color, t);
            list->AddLine(ImVec2(c.x - r, c.y + r * 0.35f), ImVec2(c.x + r, c.y + r * 0.35f), color, t);
            list->AddLine(ImVec2(c.x - r * 0.35f, c.y - r), ImVec2(c.x - r * 0.35f, c.y + r), color, t);
            list->AddLine(ImVec2(c.x + r * 0.35f, c.y - r), ImVec2(c.x + r * 0.35f, c.y + r), color, t);
            break;
        case Icon::ALIGN:
            for (int i = -1; i <= 1; i++)
            {
                const float x = c.x + (float)i * r * 0.7f;
                list->AddLine(ImVec2(x, c.y - r * 0.85f), ImVec2(x, c.y + r * 0.85f), color, t);
            }
            break;
        case Icon::STRIP:
            list->AddLine(ImVec2(c.x - r, c.y - r * 0.6f), ImVec2(c.x + r, c.y - r * 0.6f), color, t);
            list->AddLine(ImVec2(c.x - r, c.y - r * 0.15f), ImVec2(c.x + r, c.y - r * 0.15f), color, t);
            list->AddLine(ImVec2(c.x - r, c.y + r * 0.4f), ImVec2(c.x + r * 0.2f, c.y + r * 0.4f), color, t);
            break;
        case Icon::FIT:
            list->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), color, t);
            list->AddLine(ImVec2(c.x - r * 0.4f, c.y - r * 0.4f), ImVec2(c.x - r, c.y), color, t);
            list->AddLine(ImVec2(c.x - r * 0.4f, c.y + r * 0.4f), ImVec2(c.x - r, c.y), color, t);
            list->AddLine(ImVec2(c.x + r * 0.4f, c.y - r * 0.4f), ImVec2(c.x + r, c.y), color, t);
            list->AddLine(ImVec2(c.x + r * 0.4f, c.y + r * 0.4f), ImVec2(c.x + r, c.y), color, t);
            break;
        case Icon::RESET:
            list->PathArcTo(c, r * 0.8f, 0.6f, 5.4f, 20);
            list->PathStroke(color, 0, t);
            list->AddTriangleFilled(ImVec2(c.x + r * 0.75f, c.y - r * 0.55f),
                ImVec2(c.x + r * 1.05f, c.y - r * 0.05f), ImVec2(c.x + r * 0.45f, c.y - r * 0.05f),
                color);
            break;
        case Icon::COPY:
            list->AddRect(ImVec2(c.x - r * 0.8f, c.y - r * 0.8f), ImVec2(c.x + r * 0.2f, c.y + r * 0.2f),
                color, 0.0f, 0, t);
            list->AddRect(ImVec2(c.x - r * 0.2f, c.y - r * 0.2f), ImVec2(c.x + r * 0.8f, c.y + r * 0.8f),
                color, 0.0f, 0, t);
            break;
        case Icon::PASTE:
            list->AddRect(ImVec2(c.x - r * 0.7f, c.y - r * 0.7f), ImVec2(c.x + r * 0.7f, c.y + r * 0.8f),
                color, 0.0f, 0, t);
            list->AddRectFilled(ImVec2(c.x - r * 0.35f, c.y - r * 0.95f),
                ImVec2(c.x + r * 0.35f, c.y - r * 0.55f), color);
            break;
        case Icon::PLUS:
            list->AddLine(ImVec2(c.x - r * 0.8f, c.y), ImVec2(c.x + r * 0.8f, c.y), color, t);
            list->AddLine(ImVec2(c.x, c.y - r * 0.8f), ImVec2(c.x, c.y + r * 0.8f), color, t);
            break;
        case Icon::TRASH:
            list->AddRect(ImVec2(c.x - r * 0.6f, c.y - r * 0.5f), ImVec2(c.x + r * 0.6f, c.y + r * 0.85f),
                color, 0.0f, 0, t);
            list->AddLine(ImVec2(c.x - r * 0.8f, c.y - r * 0.7f), ImVec2(c.x + r * 0.8f, c.y - r * 0.7f),
                color, t);
            break;
        case Icon::CHEVRON_RIGHT:
            list->AddLine(ImVec2(c.x - r * 0.35f, c.y - r * 0.6f), ImVec2(c.x + r * 0.3f, c.y), color, t);
            list->AddLine(ImVec2(c.x + r * 0.3f, c.y), ImVec2(c.x - r * 0.35f, c.y + r * 0.6f), color, t);
            break;
        case Icon::CHEVRON_DOWN:
            list->AddLine(ImVec2(c.x - r * 0.6f, c.y - r * 0.3f), ImVec2(c.x, c.y + r * 0.35f), color, t);
            list->AddLine(ImVec2(c.x, c.y + r * 0.35f), ImVec2(c.x + r * 0.6f, c.y - r * 0.3f), color, t);
            break;
        default:
            break;
    }
}

void iconText(Icon icon, const char* text, ImU32 iconColor, const ImVec4& textColor)
{
    const float lineHeight = ImGui::GetTextLineHeight();
    const float size       = lineHeight * 0.9f;
    const ImVec2 start     = ImGui::GetCursorScreenPos();

    drawIcon(ImGui::GetWindowDrawList(), icon, ImVec2(start.x + size * 0.5f,
                 iconTopForTextLine(start.y, lineHeight, size) + size * 0.5f), size, iconColor);

    ImGui::Dummy(ImVec2(size + 4.0f, lineHeight)); // 给图标占位（Dummy 会换行，所以紧接着 SameLine）
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(textColor, "%s", text);
}

bool iconButton(const char* id, Icon icon, float size, bool activeState, const char* tooltip)
{
    const float lineHeight = ImGui::GetTextLineHeight();
    const float box        = size + 6.0f;
    const ImVec2 start     = ImGui::GetCursorScreenPos();

    const bool clicked = ImGui::InvisibleButton(id, ImVec2(box, lineHeight));
    const bool hovered = ImGui::IsItemHovered();

    if (activeState || hovered)
        ImGui::GetWindowDrawList()->AddRectFilled(start, ImVec2(start.x + box, start.y + lineHeight),
            ImGui::ColorConvertFloat4ToU32(activeState ? ImVec4(0.14f, 0.28f, 0.44f, 1.0f)
                                                       : ImVec4(1, 1, 1, 0.06f)));

    drawIcon(ImGui::GetWindowDrawList(), icon,
        ImVec2(start.x + box * 0.5f,
            iconTopForTextLine(start.y, lineHeight, size) + size * 0.5f),
        size, ImGui::ColorConvertFloat4ToU32(activeState ? ImVec4(0.85f, 0.9f, 0.98f, 1.0f)
                                                        : ImVec4(0.62f, 0.62f, 0.62f, 1.0f)));
    if (tooltip != nullptr && hovered)
        ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

} // namespace designer
