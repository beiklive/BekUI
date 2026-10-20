#pragma once

// 预设背景（挂在 Shell 的 BackdropLayer 上，铺满视口）：
//
//   GRID      深色填充 + 彩色网格（画布感）
//   FLOATING  深色背景 + 灰色几何元素缓慢上浮（参考 GBAStation 的 DynamicBackgroundBox）
//   GRADIENT  纵向渐变（顶部强调色偏暗 → 底部页面底色）
//
// 三条约定：
// 1) 全部是**绘制期**内容：只在 draw 里往 ImDrawList 加图元，不建子视图、不触碰布局；
// 2) 颜色来自主题（surface.page / accent.base / common.grey），换肤自动跟随；
// 3) 只有 FLOATING 需要时间推进（update 里累加），其余是静态的。

#include <vector>

#include "bklv/core/view.hpp"

namespace bklv
{

enum class BackdropPreset
{
    NONE,
    GRID,
    FLOATING,
    GRADIENT,
};

class PresetBackdrop : public View
{
  public:
    explicit PresetBackdrop(BackdropPreset preset = BackdropPreset::GRID);

    void setPreset(BackdropPreset preset);
    BackdropPreset getPreset() const { return this->preset; }

    void update() override;
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        FrameContext* ctx) override;

    std::string getClassString() const override { return "bklv::PresetBackdrop"; }

  private:
    struct FloatingItem
    {
        float x = 0.0f;
        float y = 0.0f;
        float speedX = 0.0f;
        float speedY = 0.0f;
        float size = 0.0f;
        float rotation = 0.0f;
        float rotateSpeed = 0.0f;
        float alpha = 0.0f;
        int   shape = 0; // 0 圆 1 方 2 三角 3 十字
    };

    void ensureItems(float width, float height);
    void drawGrid(ImDrawList* drawList, float x, float y, float width, float height);
    void drawFloating(ImDrawList* drawList, float x, float y, float width, float height);
    void drawGradient(ImDrawList* drawList, float x, float y, float width, float height);

    BackdropPreset            preset = BackdropPreset::GRID;
    float                     time = 0.0f;
    std::vector<FloatingItem> items;
    float                     itemWidth = 0.0f;
    float                     itemHeight = 0.0f;
};

} // namespace bklv
