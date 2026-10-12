#include "designer/ui/designer_style.hpp"

#include "bklv/core/appearance.hpp"
#include "bklv/core/theme_data.hpp"

namespace designer
{

namespace
{

// VS Code Dark+ 的实际色值（rgb 分量按 0-1）
ImVec4 hex(unsigned int rgb, float alpha = 1.0f)
{
    return ImVec4((float)((rgb >> 16) & 0xFF) / 255.0f, (float)((rgb >> 8) & 0xFF) / 255.0f,
        (float)(rgb & 0xFF) / 255.0f, alpha);
}

} // namespace

DesignerPalette currentDesignerPalette()
{
    DesignerPalette palette;

    palette.page          = hex(0x1E1E1E); // editor.background
    palette.card          = hex(0x252526); // sideBar.background
    palette.raised        = hex(0x333333); // activityBar / panel 抬头
    palette.sunken        = hex(0x3C3C3C); // input.background
    palette.hover         = hex(0x2A2D2E); // list.hoverBackground
    palette.textPrimary   = hex(0xCCCCCC); // foreground
    palette.textDim       = hex(0x9D9D9D); // descriptionForeground
    palette.textDisabled  = hex(0x6A6A6A); // disabledForeground
    palette.onAccent      = hex(0xFFFFFF);
    palette.border        = hex(0x3C3C3C);
    palette.borderSubtle  = hex(0x2B2B2B);
    palette.accent        = hex(0x0E639C); // button.background
    palette.accentPressed = hex(0x1177BB); // button.hoverBackground
    palette.selection     = hex(0x094771); // list.activeSelectionBackground
    palette.focusBorder   = hex(0x007FD4); // focusBorder
    palette.error         = hex(0xF14C4C); // errorForeground
    palette.warning       = hex(0xCCA700); // editorWarning.foreground
    palette.success       = hex(0x89D185);
    palette.unsaved       = hex(0xE2C08D); // gitDecoration.modifiedResourceForeground
    return palette;
}

ImVec4 accentColor() { return currentDesignerPalette().accent; }
ImVec4 mutedColor() { return currentDesignerPalette().textDim; }
ImVec4 cardColor() { return currentDesignerPalette().card; }
ImVec4 borderColor() { return currentDesignerPalette().border; }
ImVec4 selectionColor() { return currentDesignerPalette().selection; }
ImVec4 errorColor() { return currentDesignerPalette().error; }
ImVec4 warningColor() { return currentDesignerPalette().warning; }
ImVec4 successColor() { return currentDesignerPalette().success; }
ImVec4 unsavedColor() { return currentDesignerPalette().unsaved; }

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

    // 尺寸：先按"100%"写死，再乘缩放 —— 每次调用都幂等，不用维护"原始尺寸"副本
    const float s = uiScale;

    // VS Code 的控件是**方角**、行高紧凑，这里照它的观感来
    style.WindowRounding    = 0.0f;
    style.ChildRounding     = 2.0f * s;
    style.FrameRounding     = 2.0f * s;
    style.PopupRounding     = 4.0f * s;
    style.ScrollbarRounding = 6.0f * s;
    style.GrabRounding      = 2.0f * s;
    style.TabRounding       = 2.0f * s;
    style.WindowBorderSize  = 0.0f;
    style.ChildBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;

    style.WindowPadding    = ImVec2(10.0f * s, 8.0f * s);
    style.FramePadding     = ImVec2(8.0f * s, 4.0f * s);
    style.CellPadding      = ImVec2(6.0f * s, 3.0f * s);
    style.ItemSpacing      = ImVec2(8.0f * s, 5.0f * s);
    style.ItemInnerSpacing = ImVec2(6.0f * s, 4.0f * s);
    style.IndentSpacing    = 16.0f * s;
    style.ScrollbarSize    = 10.0f * s;
    style.GrabMinSize      = 9.0f * s;

    auto set  = [&style](ImGuiCol index, const ImVec4& color) { style.Colors[index] = color; };
    auto alpha = [](const ImVec4& color, float a) {
        return ImVec4(color.x, color.y, color.z, a);
    };

    // ---- 三层底色 ----
    set(ImGuiCol_WindowBg, palette.page);       // 编辑器
    set(ImGuiCol_ChildBg, palette.card);        // 侧栏/面板
    set(ImGuiCol_PopupBg, palette.card);
    set(ImGuiCol_MenuBarBg, palette.raised);    // 菜单栏用抬头色
    set(ImGuiCol_TitleBg, palette.raised);
    set(ImGuiCol_TitleBgActive, palette.raised);

    // ---- 文字层级 ----
    set(ImGuiCol_Text, palette.textPrimary);
    set(ImGuiCol_TextDisabled, palette.textDisabled);
    set(ImGuiCol_TextSelectedBg, alpha(palette.focusBorder, 0.35f));

    // ---- 边框：只留很淡的一层，靠底色区分而不是靠框线 ----
    set(ImGuiCol_Border, palette.border);
    set(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));
    set(ImGuiCol_Separator, palette.borderSubtle);
    set(ImGuiCol_SeparatorHovered, palette.focusBorder);
    set(ImGuiCol_SeparatorActive, palette.focusBorder);

    // ---- 输入框（VS Code 的 input.background 比侧栏更亮）----
    set(ImGuiCol_FrameBg, palette.sunken);
    set(ImGuiCol_FrameBgHovered, ImVec4(0.29f, 0.29f, 0.29f, 1.0f));
    set(ImGuiCol_FrameBgActive, ImVec4(0.33f, 0.33f, 0.33f, 1.0f));

    // ---- 交互蓝：按钮 / 勾选 / 滑块 ----
    set(ImGuiCol_Button, palette.accent);
    set(ImGuiCol_ButtonHovered, palette.accentPressed);
    set(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.33f, 0.51f, 1.0f));
    set(ImGuiCol_CheckMark, palette.focusBorder);
    set(ImGuiCol_SliderGrab, palette.focusBorder);
    set(ImGuiCol_SliderGrabActive, palette.accentPressed);

    // ---- 选中：背景块（不能只靠文字变色）；失焦时用更暗的一档 ----
    set(ImGuiCol_Header, palette.selection);
    set(ImGuiCol_HeaderHovered, ImVec4(0.05f, 0.31f, 0.47f, 1.0f));
    set(ImGuiCol_HeaderActive, palette.selection);
    set(ImGuiCol_NavCursor, palette.focusBorder);

    // ---- 滚动条：默认几乎隐形，悬停才明显（VS Code 就是这样）----
    set(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
    set(ImGuiCol_ScrollbarGrab, ImVec4(0.47f, 0.47f, 0.47f, 0.35f));
    set(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.53f, 0.53f, 0.53f, 0.60f));
    set(ImGuiCol_ScrollbarGrabActive, ImVec4(0.53f, 0.53f, 0.53f, 0.85f));

    set(ImGuiCol_ResizeGrip, ImVec4(0, 0, 0, 0));
    set(ImGuiCol_ResizeGripHovered, alpha(palette.focusBorder, 0.5f));
    set(ImGuiCol_ResizeGripActive, palette.focusBorder);

    set(ImGuiCol_Tab, palette.card);
    set(ImGuiCol_TabHovered, palette.raised);
    set(ImGuiCol_TabSelected, palette.page);

    set(ImGuiCol_TableHeaderBg, palette.raised);
    set(ImGuiCol_TableBorderStrong, palette.border);
    set(ImGuiCol_TableBorderLight, palette.borderSubtle);
    set(ImGuiCol_TableRowBg, ImVec4(0, 0, 0, 0));
    set(ImGuiCol_TableRowBgAlt, ImVec4(1, 1, 1, 0.02f));

    set(ImGuiCol_PlotLines, palette.focusBorder);
    set(ImGuiCol_PlotHistogram, palette.focusBorder);
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
