#include "designer/ui/designer_style.hpp"

#include "bklv/core/appearance.hpp"
#include "bklv/core/theme_data.hpp"

namespace designer
{

namespace
{

ImVec4 fromU32(ImU32 color)
{
    return ImGui::ColorConvertU32ToFloat4(color);
}

// 内置默认值：主题还没装载（或键缺失）时的兜底，和 default 主题观感一致
const ImVec4 kFallbackPage(0.176f, 0.176f, 0.176f, 1.0f);
const ImVec4 kFallbackCard(0.122f, 0.133f, 0.153f, 1.0f);
const ImVec4 kFallbackRaised(0.169f, 0.184f, 0.212f, 1.0f);
const ImVec4 kFallbackSunken(0.090f, 0.102f, 0.118f, 1.0f);
const ImVec4 kFallbackText(0.914f, 0.925f, 0.937f, 1.0f);
const ImVec4 kFallbackDim(0.545f, 0.573f, 0.608f, 1.0f);
const ImVec4 kFallbackBorder(0.227f, 0.247f, 0.278f, 1.0f);
const ImVec4 kFallbackAccent(0.0f, 1.0f, 0.8f, 1.0f);

const bklv::ThemeData& theme()
{
    return bklv::getTheme();
}

} // namespace

DesignerPalette currentDesignerPalette()
{
    DesignerPalette palette;

    // 注意带 `color.` 前缀：主题加载时把 colors.json 展平成 `color.<组>.<键>`，
    // 这里传 `surface.page` 是查不到的（踩过一次，值会静默退化成 fallback）
    auto color = [](const char* key, const ImVec4& fallback) {
        const ImU32 value = theme().color(key, 0);
        return value == 0 ? fallback : fromU32(value);
    };

    palette.page          = color("color.surface.page", kFallbackPage);
    palette.card          = color("color.surface.card", kFallbackCard);
    palette.raised        = color("color.surface.raised", kFallbackRaised);
    palette.sunken        = color("color.surface.sunken", kFallbackSunken);
    palette.hover         = color("color.surface.hover", kFallbackRaised);
    palette.textPrimary   = color("color.text.primary", kFallbackText);
    palette.textDim       = color("color.text.dim", kFallbackDim);
    palette.textDisabled  = color("color.text.disabled", kFallbackDim);
    palette.onAccent      = color("color.text.onAccent", ImVec4(0.03f, 0.13f, 0.11f, 1.0f));
    palette.border        = color("color.border.default", kFallbackBorder);
    palette.borderSubtle  = color("color.border.subtle", kFallbackBorder);
    palette.accent        = color("color.accent.base", kFallbackAccent);
    palette.accentPressed = color("color.accent.pressed", palette.accent);
    return palette;
}

ImVec4 accentColor() { return currentDesignerPalette().accent; }
ImVec4 mutedColor() { return currentDesignerPalette().textDim; }
ImVec4 cardColor() { return currentDesignerPalette().card; }
ImVec4 borderColor() { return currentDesignerPalette().border; }

void applyDesignerStyle(float uiScale)
{
    // 没有 ImGui 上下文时 GetStyle() 会直接踩空（attach 早于第一帧，踩过）
    if (ImGui::GetCurrentContext() == nullptr)
        return;

    if (uiScale < 0.7f)
        uiScale = 0.7f;
    if (uiScale > 1.6f)
        uiScale = 1.6f;

    const DesignerPalette palette = currentDesignerPalette();
    ImGuiStyle& style             = ImGui::GetStyle();

    // 字体整体缩放（ImGui 1.92+ 的全局字体缩放）
    style.FontScaleMain = uiScale;

    // 尺寸：先按"100%"写死，再乘缩放 —— 这样每次调用都是幂等的，
    // 不用自己去维护"原始尺寸"的一份副本
    const float s = uiScale;

    // 圆角与间距：和样板房一致（6px 圆角、8px 左右内距）
    style.WindowRounding    = 0.0f;
    style.ChildRounding     = 8.0f;
    style.FrameRounding     = 6.0f;
    style.PopupRounding     = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;
    style.WindowBorderSize  = 0.0f;
    style.ChildBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;

    style.WindowPadding    = ImVec2(12.0f * s, 12.0f * s);
    style.FramePadding     = ImVec2(9.0f * s, 5.0f * s);
    style.CellPadding      = ImVec2(6.0f * s, 4.0f * s);
    style.ItemSpacing      = ImVec2(8.0f * s, 6.0f * s);
    style.ItemInnerSpacing = ImVec2(6.0f * s, 4.0f * s);
    style.IndentSpacing    = 18.0f * s;
    style.ScrollbarSize    = 12.0f * s;
    style.GrabMinSize      = 10.0f * s;

    auto set = [&style](ImGuiCol index, const ImVec4& color) { style.Colors[index] = color; };

    set(ImGuiCol_WindowBg, palette.page);
    set(ImGuiCol_ChildBg, palette.card);
    set(ImGuiCol_PopupBg, palette.card);
    set(ImGuiCol_Border, palette.border);
    set(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));

    set(ImGuiCol_Text, palette.textPrimary);
    set(ImGuiCol_TextDisabled, palette.textDisabled);

    set(ImGuiCol_FrameBg, palette.sunken);
    set(ImGuiCol_FrameBgHovered, palette.raised);
    set(ImGuiCol_FrameBgActive, palette.raised);

    set(ImGuiCol_TitleBg, palette.card);
    set(ImGuiCol_TitleBgActive, palette.card);
    set(ImGuiCol_MenuBarBg, palette.card);

    set(ImGuiCol_ScrollbarBg, palette.sunken);
    set(ImGuiCol_ScrollbarGrab, palette.border);
    set(ImGuiCol_ScrollbarGrabHovered, palette.textDim);
    set(ImGuiCol_ScrollbarGrabActive, palette.accent);

    set(ImGuiCol_CheckMark, palette.accent);
    set(ImGuiCol_SliderGrab, palette.accent);
    set(ImGuiCol_SliderGrabActive, palette.accentPressed);

    set(ImGuiCol_Button, palette.raised);
    set(ImGuiCol_ButtonHovered, ImVec4(palette.border.x, palette.border.y, palette.border.z, 1.0f));
    set(ImGuiCol_ButtonActive, palette.accent);

    set(ImGuiCol_Header, palette.raised);
    set(ImGuiCol_HeaderHovered, ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.22f));
    set(ImGuiCol_HeaderActive, ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.35f));

    set(ImGuiCol_Separator, palette.borderSubtle);
    set(ImGuiCol_SeparatorHovered, palette.border);
    set(ImGuiCol_SeparatorActive, palette.accent);

    set(ImGuiCol_ResizeGrip, palette.border);
    set(ImGuiCol_ResizeGripHovered, palette.accent);
    set(ImGuiCol_ResizeGripActive, palette.accent);

    set(ImGuiCol_Tab, palette.card);
    set(ImGuiCol_TabHovered, palette.raised);
    set(ImGuiCol_TabSelected, palette.raised);

    set(ImGuiCol_TableHeaderBg, palette.card);
    set(ImGuiCol_TableBorderStrong, palette.border);
    set(ImGuiCol_TableBorderLight, palette.borderSubtle);
    set(ImGuiCol_TableRowBg, ImVec4(0, 0, 0, 0));
    set(ImGuiCol_TableRowBgAlt, ImVec4(1, 1, 1, 0.02f));

    set(ImGuiCol_PlotLines, palette.accent);
    set(ImGuiCol_PlotHistogram, palette.accent);

    set(ImGuiCol_TextSelectedBg, ImVec4(palette.accent.x, palette.accent.y, palette.accent.z, 0.35f));
    set(ImGuiCol_NavCursor, palette.accent);
}

void panelHeader(const char* title, const char* hint)
{
    const DesignerPalette palette = currentDesignerPalette();
    ImDrawList* drawList          = ImGui::GetWindowDrawList();

    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width  = ImGui::GetContentRegionAvail().x;
    const float height = ImGui::GetFrameHeight();

    // 一条"抬起来"的标题条 + 左侧强调色小竖条
    drawList->AddRectFilled(start, ImVec2(start.x + width, start.y + height),
        ImGui::ColorConvertFloat4ToU32(palette.raised), 6.0f);
    drawList->AddRectFilled(ImVec2(start.x + 6.0f, start.y + 5.0f),
        ImVec2(start.x + 9.0f, start.y + height - 5.0f),
        ImGui::ColorConvertFloat4ToU32(palette.accent), 2.0f);

    ImGui::SetCursorScreenPos(ImVec2(start.x + 16.0f, start.y + 4.0f));
    ImGui::TextColored(palette.accent, "%s", title);
    if (hint != nullptr)
    {
        ImGui::SameLine();
        ImGui::TextColored(palette.textDim, "%s", hint);
    }
    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + height + 4.0f));
}

} // namespace designer
