#pragma once

// 玻璃的调参数据：设计器界面上那几个滑块改的就是它。
//
// 放在这里（而不是 UI 文件里）的原因：它要能存进 ui_state（下次打开还是这套参数），
// 也要能被无窗口的逻辑测试覆盖。

#include <algorithm>

#include "bklv/core/glass/glass_style.hpp"

namespace bklv::glass
{

struct GlassTuning
{
    // 总开关（界面上也能关；BKLV_DESIGNER_GLASS=0 关的是另一层：后端不启用）
    bool enabled = true;

    // 背景模糊：半径按物理像素（源项目调参范围 4~24）。默认 14 ≈ 半分辨率下 σ≈7px：
    // 既能糊掉细节，又不会把底下的细结构（1px 网格、文字）抹成一片 —— 32 时 σ≈16px，
    // demo 的 1px 网格线被抹平，折射就"没东西可折"了（踩过）。
    float blurRadius = 14.0f;
    int quality      = 1; // 0 = 最小（5 对）/ 1 = 均衡（8 对）/ 2 = 完整（12 对）

    // 折射采样哪张底：1 = 纯模糊底（默认，磨砂感最强、折射最含蓄），
    // 0 = 原始底（折射最明显）。中间值按比例混合。
    float blurMix = 1.0f;

    // 染色（RGB + 透明度）：玻璃整体色调
    // 染色别太重：这是"玻璃"，底下的壁纸要看得见（0.30 时整个面板看着还是黑的）
    float tintR = 0.04f;
    float tintG = 0.06f;
    float tintB = 0.12f;
    float tintA = 0.34f;

    // 折射与色散。
    //
    // refractionHeight 是"离边缘多少像素之内参与折射" —— 它决定扭曲**看得见多少**：
    // shader 里 `-shadeDistance >= refractionHeight` 的像素直接采模糊底、不位移，所以
    // 22 时只有一圈很窄的边被掰弯（大面板上几乎看不出来，用户报过"只有模糊没扭曲"）。
    // 40 是折中：常见控件（100~300px）能明显看到边缘被掰弯，而小控件不会失控 ——
    // 合成时它会按 `最短边/2` 夹取（gl_glass.cpp 的 clampFinite），所以按钮那种小面上
    // 不会整块变成透镜。
    float refractionHeight    = 40.0f;
    float refractionAmount    = -120.0f;
    float depthEffect         = 0.75f;
    float chromaticAberration = 0.50f;
    float contrast            = 0.02f;

    // 光照
    float innerShadow = 0.12f;
    float rimGlow     = 0.25f;
    float specular    = 0.32f;
    float outerShadow = 0.10f;
    float shadowExtent = 5.0f;

    // 圆角：横条（菜单栏/状态栏）与面板（左右栏）分开
    float barRadius   = 6.0f;
    float panelRadius = 12.0f;

    // 横条/面板的外阴影分别控制：横条默认不外扩（会渗进预览面板）
    bool barOuterShadow = false;

    // 调参面板是否展开（存 ui_state）
    bool showPanel = false;

    void clampToValidRange()
    {
        this->blurRadius = std::min(std::max(this->blurRadius, 0.0f), 48.0f);
        this->quality    = std::min(std::max(this->quality, 0), 2);
        for (float* channel : { &this->tintR, &this->tintG, &this->tintB, &this->tintA })
            *channel = std::min(std::max(*channel, 0.0f), 1.0f);
        this->refractionHeight    = std::min(std::max(this->refractionHeight, 0.0f), 80.0f);
        this->refractionAmount    = std::min(std::max(this->refractionAmount, -240.0f), 0.0f);
        this->depthEffect         = std::min(std::max(this->depthEffect, 0.0f), 2.0f);
        this->chromaticAberration = std::min(std::max(this->chromaticAberration, 0.0f), 1.0f);
        this->contrast            = std::min(std::max(this->contrast, -1.0f), 1.0f);
        this->blurMix             = std::min(std::max(this->blurMix, 0.0f), 1.0f);
        this->innerShadow = std::min(std::max(this->innerShadow, 0.0f), 1.0f);
        this->rimGlow     = std::min(std::max(this->rimGlow, 0.0f), 1.0f);
        this->specular    = std::min(std::max(this->specular, 0.0f), 2.0f);
        this->outerShadow = std::min(std::max(this->outerShadow, 0.0f), 1.0f);
        this->shadowExtent = std::min(std::max(this->shadowExtent, 0.0f), 24.0f);
        this->barRadius   = std::min(std::max(this->barRadius, 0.0f), 40.0f);
        this->panelRadius = std::min(std::max(this->panelRadius, 0.0f), 40.0f);
    }
};

// 把全局调参折成一块玻璃的观感（折射/色散/染色/光照）。
// 与设计器"面板"那条映射一致；外观层将来可以在它之上覆盖单个字段。
GlassStyle makeGlassStyle(const GlassTuning& tuning) noexcept;

} // namespace bklv::glass
