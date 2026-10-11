#include "bklv/views/progress_spinner.hpp"

#include <cmath>

namespace bklv
{

ProgressSpinner::ProgressSpinner()
{
    this->setWidth(32.0f);
    this->setHeight(32.0f);
}

void ProgressSpinner::update()
{
    const float deltaTime = ImGui::GetIO().DeltaTime;
    if (this->duration <= 0.0f)
        return;

    this->angle += (2.0f * 3.14159265f / this->duration) * deltaTime;

    // 保持在 0~2π，避免长时间运行后浮点精度下降。
    while (this->angle > 2.0f * 3.14159265f)
        this->angle -= 2.0f * 3.14159265f;
}

void ProgressSpinner::draw(ImDrawList* drawList, float x, float y, float width, float height, Style,
    FrameContext*)
{
    if (width <= 0.0f || height <= 0.0f || this->segments < 2)
        return;

    const float centerX = x + width * 0.5f;
    const float centerY = y + height * 0.5f;
    const float outerRadius = std::min(width, height) * 0.5f;
    const float innerRadius = std::max(0.0f, outerRadius - this->thickness);

    // 每段用一个梯形（两个圆上的点组成四边形），透明度沿圆周渐变。
    for (int i = 0; i < this->segments; ++i)
    {
        const float t0 = this->angle + (2.0f * 3.14159265f) * (static_cast<float>(i) / this->segments);
        const float t1 = this->angle + (2.0f * 3.14159265f) * (static_cast<float>(i + 1) / this->segments);

        // 越靠"头部"越亮
        const float fade = static_cast<float>(i) / static_cast<float>(this->segments);
        const ImU32 color = colorWithAlpha(this->spinnerColor, this->getAlpha() * fade);

        const ImVec2 p0(centerX + std::cos(t0) * outerRadius, centerY + std::sin(t0) * outerRadius);
        const ImVec2 p1(centerX + std::cos(t1) * outerRadius, centerY + std::sin(t1) * outerRadius);
        const ImVec2 p2(centerX + std::cos(t1) * innerRadius, centerY + std::sin(t1) * innerRadius);
        const ImVec2 p3(centerX + std::cos(t0) * innerRadius, centerY + std::sin(t0) * innerRadius);

        drawList->AddQuadFilled(p0, p1, p2, p3, color);
    }
}

} // namespace bklv
