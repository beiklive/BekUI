#include "designer/ui/icon_draw.hpp"

#include "bklv/core/icon_font.hpp"
#include "designer/core/icon_align.hpp"
#include "designer/ui/designer_style.hpp"

#include <cmath>

namespace designer
{

namespace
{

ImFont* g_iconFont = nullptr;

// designer 的 Icon → 库里的图标 id（库自带 Material Icons）
bklv::icon::Id libraryIcon(Icon icon)
{
    switch (icon)
    {
        case Icon::WIDGET: return bklv::icon::Id::WIDGETS;
        case Icon::PAGE: return bklv::icon::Id::DESCRIPTION;
        case Icon::TABLE: return bklv::icon::Id::TABLE_CHART;
        case Icon::LAYER: return bklv::icon::Id::LAYERS;
        case Icon::STATE: return bklv::icon::Id::TUNE;
        case Icon::FIELD: return bklv::icon::Id::ADD;
        case Icon::EYE: return bklv::icon::Id::VISIBILITY;
        case Icon::LAYERS_ONLY: return bklv::icon::Id::LAYERS_CLEAR;
        case Icon::EXPAND: return bklv::icon::Id::UNFOLD_MORE;
        case Icon::COLLAPSE: return bklv::icon::Id::UNFOLD_LESS;
        case Icon::GRID: return bklv::icon::Id::GRID_ON;
        case Icon::ALIGN: return bklv::icon::Id::FORMAT_ALIGN_LEFT;
        case Icon::STRIP: return bklv::icon::Id::REORDER;
        case Icon::FIT: return bklv::icon::Id::FULLSCREEN;
        case Icon::RESET: return bklv::icon::Id::REFRESH;
        case Icon::COPY: return bklv::icon::Id::CONTENT_COPY;
        case Icon::PASTE: return bklv::icon::Id::CONTENT_PASTE;
        case Icon::PLUS: return bklv::icon::Id::ADD;
        case Icon::TRASH: return bklv::icon::Id::DELETE;
        case Icon::CHEVRON_RIGHT: return bklv::icon::Id::CHEVRON_RIGHT;
        case Icon::CHEVRON_DOWN: return bklv::icon::Id::EXPAND_MORE;
        default: return bklv::icon::Id::NONE;
    }
}

// Material Icons 码位表（Unicode 私有区）
unsigned int codepointFor(Icon icon)
{
    switch (icon)
    {
        case Icon::WIDGET:      return 0xE1BD; // widgets
        case Icon::PAGE:        return 0xE873; // description
        case Icon::TABLE:       return 0xE896; // list
        case Icon::LAYER:       return 0xE53B; // layers
        case Icon::STATE:       return 0xE429; // tune
        case Icon::FIELD:       return 0xE145; // add
        case Icon::EYE:         return 0xE8F4; // visibility
        case Icon::LAYERS_ONLY: return 0xE5D7; // unfold_more
        case Icon::EXPAND:      return 0xE5D7; // unfold_more
        case Icon::COLLAPSE:    return 0xE5CF; // expand_more
        case Icon::GRID:        return 0xE3EC; // grid_on
        case Icon::ALIGN:       return 0xE236; // format_align_left
        case Icon::STRIP:       return 0xE8FE; // reorder
        case Icon::FIT:         return 0xE5DB; // hmm 先用全屏
        case Icon::RESET:       return 0xE5D5; // refresh
        case Icon::COPY:        return 0xE14D; // content_copy
        case Icon::PASTE:       return 0xE14F; // content_paste
        case Icon::PLUS:        return 0xE145; // add
        case Icon::TRASH:       return 0xE872; // delete
        case Icon::CHEVRON_RIGHT: return 0xE5CC; // chevron_right
        case Icon::CHEVRON_DOWN:  return 0xE5CF; // expand_more
        default: return 0;
    }
}

} // namespace

bool setIconFontPath(const std::string& ttfPath, float pixelSize)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.Fonts == nullptr)
        return false;

    ImFontConfig config;
    config.MergeMode = false;
    ImFont* font     = io.Fonts->AddFontFromFileTTF(ttfPath.c_str(), pixelSize, &config);
    if (font == nullptr)
        return false;
    g_iconFont = font;
    return true;
}

bool hasIconFont() { return g_iconFont != nullptr; }

void drawIcon(ImDrawList* list, Icon icon, ImVec2 c, float size, ImU32 color)
{
    if (list == nullptr || icon == Icon::NONE)
        return;

    // 优先用**库自带**的 Material Icons 字形（用户要求必须用这个字体）
    if (bklv::icon::hasFont())
    {
        const char* glyph = bklv::icon::glyph(libraryIcon(icon));
        if (glyph != nullptr && glyph[0] != 0)
        {
            ImFont* font           = bklv::icon::font();
            const ImVec2 textSize  = font->CalcTextSizeA(size, FLT_MAX, 0.0f, glyph);
            // 纵向按"行高取中"落位（和矢量图标同一条规则，不许偏高）
            list->AddText(font, size, ImVec2(c.x - textSize.x * 0.5f, c.y - size * 0.5f), color,
                glyph);
            return;
        }
    }

    if (g_iconFont != nullptr)
    {
        const unsigned int cp = codepointFor(icon);
        if (cp != 0)
        {
            ImFont* font     = ImGui::GetFont();
            const float base = ImGui::GetFontSize();
            ImGui::PushFont(g_iconFont, size);
            const ImVec2 glyphSize = g_iconFont->CalcTextSizeA(size, FLT_MAX, 0.0f,
                (const char*)u8"\uE000"); // 用一个占位字形量高度
            const char text[5] = { (char)(0xE0 | (cp >> 12)), (char)(0x80 | ((cp >> 6) & 0x3F)),
                (char)(0x80 | (cp & 0x3F)), 0, 0 };
            const ImVec2 textSize = ImGui::CalcTextSize(text);
            list->AddText(g_iconFont, size, ImVec2(c.x - textSize.x * 0.5f, c.y - size * 0.5f),
                color, text);
            ImGui::PopFont();
            (void)font; (void)base; (void)glyphSize;
            return;
        }
    }

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
            ImGui::ColorConvertFloat4ToU32(activeState ? chipActiveColor() : chipHoverColor()),
            ImGui::GetStyle().FrameRounding);

    drawIcon(ImGui::GetWindowDrawList(), icon,
        ImVec2(start.x + box * 0.5f,
            iconTopForTextLine(start.y, lineHeight, size) + size * 0.5f),
        size, ImGui::ColorConvertFloat4ToU32(activeState ? iconActiveColor() : iconIdleColor()));
    if (tooltip != nullptr && hovered)
        ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

} // namespace designer
