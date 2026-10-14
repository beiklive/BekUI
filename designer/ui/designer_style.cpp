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

    // ---- 玻璃风：圆角 + 半透明填充 ----
    // 面板/横条背后已经有一层玻璃（designer/glass/），所以控件自己**不再铺不透明底**：
    // 一律用半透明白做"磨砂片"，这样同一套样式放在玻璃上、放在普通面板上都成立，
    // 也不会再出现 ImGui 默认那种灰色实心方块。
    style.WindowRounding    = 12.0f * s;
    style.ChildRounding     = 10.0f * s;
    style.FrameRounding     = 10.0f * s;
    style.PopupRounding     = 12.0f * s;
    style.ScrollbarRounding = 8.0f * s;
    style.GrabRounding      = 8.0f * s;
    style.TabRounding       = 10.0f * s;
    style.WindowBorderSize  = 0.0f;
    style.ChildBorderSize   = 0.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;

    style.WindowPadding    = ImVec2(12.0f * s, 10.0f * s);
    style.FramePadding     = ImVec2(10.0f * s, 6.0f * s);
    style.CellPadding      = ImVec2(8.0f * s, 4.0f * s);
    style.ItemSpacing      = ImVec2(8.0f * s, 7.0f * s);
    style.ItemInnerSpacing = ImVec2(7.0f * s, 5.0f * s);
    style.IndentSpacing    = 18.0f * s;
    style.ScrollbarSize    = 12.0f * s;
    style.GrabMinSize      = 12.0f * s;

    auto set  = [&style](ImGuiCol index, const ImVec4& color) { style.Colors[index] = color; };
    auto alpha = [](const ImVec4& color, float a) {
        return ImVec4(color.x, color.y, color.z, a);
    };
    // 磨砂白：玻璃上叠一层很淡的白（悬停/按下再亮一档）
    auto frost = [](float a) { return ImVec4(1.0f, 1.0f, 1.0f, a); };

    // ---- 三层底色：主窗交给玻璃，浮窗留一层薄底 ----
    set(ImGuiCol_WindowBg, ImVec4(palette.page.x, palette.page.y, palette.page.z, 0.0f));
    set(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));         // 面板背后是玻璃
    set(ImGuiCol_PopupBg, alpha(palette.card, 0.55f)); // 弹窗：薄底 + 玻璃
    set(ImGuiCol_MenuBarBg, ImVec4(0, 0, 0, 0));       // 菜单栏背后是横条玻璃
    set(ImGuiCol_TitleBg, alpha(palette.raised, 0.8f));
    set(ImGuiCol_TitleBgActive, alpha(palette.raised, 0.9f));

    // ---- 文字层级 ----
    set(ImGuiCol_Text, palette.textPrimary);
    set(ImGuiCol_TextDisabled, palette.textDisabled);
    set(ImGuiCol_TextSelectedBg, alpha(palette.focusBorder, 0.35f));

    // ---- 边框：玻璃的"边"，一律淡白细线 ----
    set(ImGuiCol_Border, frost(0.12f));
    set(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));
    set(ImGuiCol_Separator, frost(0.10f));
    set(ImGuiCol_SeparatorHovered, alpha(palette.focusBorder, 0.7f));
    set(ImGuiCol_SeparatorActive, palette.focusBorder);

    // ---- 输入框 / 下拉框 / 滑块轨道：磨砂片 ----
    set(ImGuiCol_FrameBg, frost(0.07f));
    set(ImGuiCol_FrameBgHovered, frost(0.12f));
    set(ImGuiCol_FrameBgActive, frost(0.17f));

    // ---- 按钮：磨砂片；强调按钮仍用蓝（但半透明） ----
    set(ImGuiCol_Button, frost(0.10f));
    set(ImGuiCol_ButtonHovered, frost(0.16f));
    set(ImGuiCol_ButtonActive, alpha(palette.accent, 0.65f));
    set(ImGuiCol_CheckMark, palette.focusBorder);
    set(ImGuiCol_SliderGrab, frost(0.65f));
    set(ImGuiCol_SliderGrabActive, palette.focusBorder);

    // ---- 选中 / 悬停行 ----
    set(ImGuiCol_Header, alpha(palette.accent, 0.45f));
    set(ImGuiCol_HeaderHovered, frost(0.12f));
    set(ImGuiCol_HeaderActive, alpha(palette.accent, 0.60f));
    set(ImGuiCol_NavCursor, alpha(palette.focusBorder, 0.9f));

    // ---- 滚动条：几乎隐形，悬停才亮 ----
    set(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
    set(ImGuiCol_ScrollbarGrab, frost(0.22f));
    set(ImGuiCol_ScrollbarGrabHovered, frost(0.38f));
    set(ImGuiCol_ScrollbarGrabActive, frost(0.55f));

    set(ImGuiCol_ResizeGrip, ImVec4(0, 0, 0, 0));
    set(ImGuiCol_ResizeGripHovered, alpha(palette.focusBorder, 0.5f));
    set(ImGuiCol_ResizeGripActive, palette.focusBorder);

    // ---- 页签：玻璃片 ----
    set(ImGuiCol_Tab, frost(0.05f));
    set(ImGuiCol_TabHovered, frost(0.14f));
    set(ImGuiCol_TabSelected, frost(0.18f));
    set(ImGuiCol_TabSelectedOverline, alpha(palette.focusBorder, 0.9f));
    set(ImGuiCol_TabDimmed, frost(0.03f));
    set(ImGuiCol_TabDimmedSelected, frost(0.10f));

    // ---- 表格 ----
    set(ImGuiCol_TableHeaderBg, frost(0.08f));
    set(ImGuiCol_TableBorderStrong, frost(0.14f));
    set(ImGuiCol_TableBorderLight, frost(0.07f));
    set(ImGuiCol_TableRowBg, ImVec4(0, 0, 0, 0));
    set(ImGuiCol_TableRowBgAlt, frost(0.03f));

    set(ImGuiCol_TextLink, alpha(palette.focusBorder, 0.95f));

    set(ImGuiCol_PlotLines, palette.focusBorder);
    set(ImGuiCol_PlotHistogram, palette.focusBorder);
}

void panelHeader(const char* title, const char* hint)
{
    const DesignerPalette palette = currentDesignerPalette();

    ImGui::TextColored(palette.textPrimary, "%s", title);
    if (hint != nullptr && hint[0] != 0)
    {
        ImGui::SameLine();
        ImGui::TextColored(palette.textDim, "%s", hint);
    }
    ImGui::Separator(); // 扁平：只用一条细线分区，不画抬起的条
}

} // namespace designer
