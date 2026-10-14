#pragma once

// 设计器的配色与 ImGui 样式。
//
// 配色用 **VS Code Dark+ 的语义体系**（固定，不跟随被编辑的主题）：
// 工具本身的界面必须是稳定的参照物 —— 如果设计器跟着主题变色，
// 调主题时"看到的界面"和"被调的界面"就混在一起了。
//
// 三层底色：编辑器 #1E1E1E / 侧栏 #252526 / 面板与标题 #333333；
// 交互蓝 #0E639C（悬停 #1177BB）、焦点框 #007FD4、选中背景 #094771；
// 语义色：错误 #F14C4C / 警告 #CCA700 / 通过 #89D185 / 未保存 #E2C08D。

#include "bklv/3rd/imgui/imgui.h"

namespace designer
{

struct DesignerPalette
{
    ImVec4 page;         // 编辑器底 #1E1E1E
    ImVec4 card;         // 侧栏/面板底 #252526
    ImVec4 raised;       // 面板抬头、按钮底 #333333
    ImVec4 sunken;       // 输入框底 #3C3C3C
    ImVec4 hover;        // 悬停底 #2A2D2E
    ImVec4 textPrimary;  // #CCCCCC
    ImVec4 textDim;      // #9D9D9D
    ImVec4 textDisabled; // #6A6A6A
    ImVec4 onAccent;
    ImVec4 border;       // #3C3C3C
    ImVec4 borderSubtle; // #2B2B2B
    ImVec4 accent;       // 交互蓝 #0E639C
    ImVec4 accentPressed;// #1177BB
    ImVec4 selection;    // 选中背景 #094771
    ImVec4 focusBorder;  // #007FD4
    ImVec4 error;        // #F14C4C
    ImVec4 warning;      // #CCA700
    ImVec4 success;      // #89D185
    ImVec4 unsaved;      // #E2C08D（VS Code 的 git modified 色）
};

DesignerPalette currentDesignerPalette();

// 把调色板套到 ImGui 全局样式上（圆角、间距、各控件配色）。
// uiScale 会同时作用到尺寸与字体（屏大或 Retina 下默认尺寸显得过大）。
void applyDesignerStyle(float uiScale = 1.0f);

// 常用快捷取色
ImVec4 accentColor();
// 自绘控件（iconButton / 列表行 / 分隔条）用的"磨砂片"取色：和 ImGui 那套样式同源，
// 免得自绘的地方自己写死颜色 —— 写死过一版：选中态用了不透明蓝，正好把玻璃盖掉
ImVec4 chipColor();        // 常态底
ImVec4 chipHoverColor();   // 悬停
ImVec4 chipActiveColor();  // 选中 / 打开
ImVec4 iconIdleColor();    // 常态图标
ImVec4 iconActiveColor();  // 高亮图标
ImVec4 rowTextColor(bool selected);
ImVec4 rowIconColor(bool selected);
ImVec4 mutedColor();
ImVec4 cardColor();
ImVec4 borderColor();
ImVec4 selectionColor();  // 选中背景
ImVec4 errorColor();
ImVec4 warningColor();
ImVec4 successColor();
ImVec4 unsavedColor();    // "改过 / 未保存"专用的琥珀色

// 面板小标题：一条带强调色标题的东西（统一各面板的头部观感）
void panelHeader(const char* title, const char* hint = nullptr);

// 面板标题栏（三个面板共用）：和内容区不是一个样式 ——
// 深色底 + 顶部两角跟面板圆角 + 固定高度 + **四边等边距** + 标题字号比正文小一档。
// 用法：barHeight = panelTitleBarStart(...) →（可选）在栏内放按钮/文字 →
//       panelTitleBarTitle(...) → panelTitleBarEnd(barHeight, uiScale)
float panelTitleBarStart(float uiScale, float cornerRadius);
void  panelTitleBarTitle(const char* title, const char* hint);
void  panelTitleBarEnd(float barHeight, float uiScale);

// 小标签（chip）：左栏底部的"节点/层/状态"计数与选中路径用它，
// 比一行裸文字 + 「·」分隔清楚得多。返回是否被点击。
bool statChip(const char* id, const char* label, const char* value, const char* tooltip);
bool crumbChip(const char* text, bool current, const char* tooltip);

} // namespace designer
