#pragma once

// 设计器的配色与 ImGui 样式。
//
// 配色**跟着主题走**：读当前主题的语义色（surface.* / text.* / border.* / accent.*），
// 所以换主题时设计器自己也跟着换色 —— 对一个"调主题的工具"来说这是最自然的行为。
// 取不到就退回一套内置的深色默认值（第一次跑、主题还没装载时用）。

#include "bklv/3rd/imgui/imgui.h"

namespace designer
{

struct DesignerPalette
{
    ImVec4 page;         // 窗口底
    ImVec4 card;         // 面板底
    ImVec4 raised;       // 悬浮/按钮
    ImVec4 sunken;       // 输入框底
    ImVec4 hover;        // 悬停
    ImVec4 textPrimary;
    ImVec4 textDim;
    ImVec4 textDisabled;
    ImVec4 onAccent;     // 强调色上的文字
    ImVec4 border;
    ImVec4 borderSubtle;
    ImVec4 accent;
    ImVec4 accentPressed;
};

DesignerPalette currentDesignerPalette();

// 把调色板套到 ImGui 全局样式上（圆角、间距、各控件配色）。
// uiScale 会同时作用到尺寸与字体（屏大或 Retina 下默认尺寸显得过大）。
void applyDesignerStyle(float uiScale = 1.0f);

// 常用快捷取色（跟着主题走）
ImVec4 accentColor();
ImVec4 mutedColor();
ImVec4 cardColor();
ImVec4 borderColor();

// 面板小标题：一条带强调色标题的东西（统一各面板的头部观感）
void panelHeader(const char* title, const char* hint = nullptr);

} // namespace designer
