#pragma once

// 设计器自己的界面状态（栏宽、画布、开关、上次在看哪个文件）。
//
// 放在 core 是为了能测：以前 load/save 直接写在 UI 文件里，等于没有回归保护。

#include "bklv/glass/glass_tuning.hpp"

#include <string>

namespace designer
{

struct UiState
{
    // 三栏宽度与左侧"文件列表"高度：都能拖分隔条改（拖完存这里）
    int leftWidth      = 320;
    int rightWidth     = 420;
    int fileListHeight = 250;
    bool leftCollapsed = false;

    // 设计器界面的整体缩放（不是预览画布的缩放）：0.75 / 0.85 / 0.92 / 1.0 / 1.15 / 1.3
    // 0.85：字号另有收紧系数（见 applyDesignerStyle），这里只决定整体疏密
    float uiScale = 0.85f;

    int canvasPreset   = 0;  // 0=自适应 1=1280×720 2=720×480 3=640×360 4=自定义
    bool canvasChecker = true;
    // 0 = 细棋盘 / 1 = 暗面 / 2 = 纸面 / 3 = 透明（老状态文件只有 canvasChecker，按它迁移）
    int canvasBackdrop = 0;
    int canvasZoom     = 2;  // 0=50% 1=75% 2=100% 3=150%
    bool showGrid      = false; // 对齐网格 + 中线
    bool onlyLayers    = true;  // 节点树只看层（字段归右侧检查器，树只留结构）

    bool previewContext   = false;
    bool previewCompare   = false;
    bool previewMaximized = false;
    bool previewSubtree   = false;

    // 玻璃观感（设计器界面上调的参数，见 DesignerGlass）
    bklv::glass::GlassTuning glass{};

    std::string lastAppearance; // 上次打开的外观（编辑主题表时拿它当预览）
    std::string lastViewFile;   // 上次打开的页面视图
};

// 拖分隔条时的区间钳制：保证两侧都留得住内容。
// available <= 0（第一帧还没算出宽度）时返回原值，避免把宽度钳成 0。
int clampPanelWidth(int width, int minWidth, int maxWidth);

// 界面缩放的合法区间（UI 里给的那几档都落在这里面）
float clampUiScale(float scale);

// 读不到 / 解析失败时保持默认值并返回 false（不是错误，第一次跑本来就没有）
bool loadUiState(const std::string& path, UiState& out);

bool saveUiState(const std::string& path, const UiState& state, std::string& error);

} // namespace designer
