#pragma once

// 设计器自己的界面状态（栏宽、画布、开关、上次在看哪个文件）。
//
// 放在 core 是为了能测：以前 load/save 直接写在 UI 文件里，等于没有回归保护。

#include <string>

namespace designer
{

struct UiState
{
    int leftWidth      = 320;
    int rightWidth     = 420;
    bool leftCollapsed = false;

    int canvasPreset   = 0;  // 0=自适应 1=1280×720 2=720×480 3=640×360 4=自定义
    bool canvasChecker = true;
    int canvasZoom     = 2;  // 0=50% 1=75% 2=100% 3=150%
    bool showGrid      = false; // 对齐网格 + 中线
    bool onlyLayers    = false; // 节点树只看层（把字段行收起来，找控件更快）

    bool previewContext   = false;
    bool previewCompare   = false;
    bool previewMaximized = false;
    bool previewSubtree   = false;

    std::string lastAppearance; // 上次打开的外观（编辑主题表时拿它当预览）
    std::string lastViewFile;   // 上次打开的页面视图
};

// 读不到 / 解析失败时保持默认值并返回 false（不是错误，第一次跑本来就没有）
bool loadUiState(const std::string& path, UiState& out);

bool saveUiState(const std::string& path, const UiState& state, std::string& error);

} // namespace designer
