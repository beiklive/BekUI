#include "bklv/core/shadow.hpp"

#include <cmath>
#include <cstdlib>

#include "bklv/core/logger.hpp"

namespace bklv
{
namespace shadow
{
namespace
{

ShadowQuality current = ShadowQuality::FAST;
bool probed          = false;

void probeEnvironment()
{
    if (probed)
        return;
    probed = true;

    const char* requested = std::getenv("BKLV_SHADOW_QUALITY");
    if (requested == nullptr || *requested == '\0')
        return;

    if (requested[0] == 's' || requested[0] == 'S')
    {
        current = ShadowQuality::SMOOTH;
        BKLV_LOG_INFO("阴影档位：smooth（环境变量 BKLV_SHADOW_QUALITY）");
    }
    else if (requested[0] == 'f' || requested[0] == 'F')
    {
        current = ShadowQuality::FAST;
        BKLV_LOG_INFO("阴影档位：fast（环境变量 BKLV_SHADOW_QUALITY）");
    }
    else
    {
        BKLV_LOG_WARNING("BKLV_SHADOW_QUALITY=%s 不认识（可用 fast / smooth），按 fast 处理",
            requested);
    }
}

} // namespace

ShadowQuality getQuality()
{
    probeEnvironment();
    return current;
}

void setQuality(ShadowQuality quality)
{
    probed  = true;
    current = quality;
}

bool setQualityByName(const std::string& name)
{
    if (name == "fast" || name == "FAST")
    {
        setQuality(ShadowQuality::FAST);
        return true;
    }
    if (name == "smooth" || name == "SMOOTH")
    {
        setQuality(ShadowQuality::SMOOTH);
        return true;
    }
    return false;
}

const char* qualityName(ShadowQuality quality)
{
    return quality == ShadowQuality::SMOOTH ? "smooth" : "fast";
}

int layerCount()
{
    return getQuality() == ShadowQuality::SMOOTH ? 24 : 8;
}

void drawSoft(ImDrawList* drawList, float minX, float minY, float maxX, float maxY, float radius,
    ImU32 color, float alphaScale, float feather, float spread, bool cutout)
{
    if (drawList == nullptr || maxX <= minX || maxY <= minY)
        return;

    const int steps = layerCount();

    const float target = (float)((color >> 24) & 0xFF) / 255.0f * alphaScale;
    if (target <= 0.001f)
        return;

    // 高斯权重：贴着形状的那层最实，最外层最淡
    float weights[24];
    float weightSum = 0.0f;
    const float span = (float)(steps > 1 ? steps - 1 : 1);
    for (int i = 0; i < steps; i++)
    {
        const float t = 1.0f - (float)i / span; // 1 = 最外
        weights[i]    = std::exp(-4.5f * t * t);
        weightSum += weights[i];
    }

    // 每层 alpha 之和 = 目标 alpha：叠加后的实际不透明度约等于 1-exp(-target)，
    // 在阴影常用的 0.2~0.4 区间偏差 <10%，够用且不引入魔数
    const float unit       = 255.0f * target / (weightSum > 0.0f ? weightSum : 1.0f);
    const unsigned int rgb = color & 0x00FFFFFFu;

    // 从最外层往内画：内层后画才盖在上面
    for (int i = 0; i < steps; i++)
    {
        const float t      = 1.0f - (float)i / span;
        const float expand = spread + feather * t;

        const unsigned int alpha = (unsigned int)std::lround(unit * weights[i]);
        if (alpha == 0)
            continue;

        const float outerMinX = minX - expand;
        const float outerMinY = minY - expand;
        const float outerMaxX = maxX + expand;
        const float outerMaxY = maxY + expand;
        const ImU32 col       = rgb | (alpha << 24);

        if (!cutout)
        {
            drawList->AddRectFilled(ImVec2(outerMinX, outerMinY), ImVec2(outerMaxX, outerMaxY),
                col, radius + expand);
            continue;
        }

        // 挖空内圈：沿控件圆角矩形的**中线**描一圈，厚度就是外扩量。
        // 不能用"四条矩形带子"：ImGui 的 AddRectFilled 会把圆角钳到边长的一半，
        // 而四条带子又矮又窄，圆角直接被钳没了、看着是直角（用户报的 bug）。
        // 描边法没有这个钳制，而且内缘正好贴合控件的圆角形状，阴影更准。
        const float half      = expand * 0.5f;
        const float midRadius = radius + half;
        drawList->PathRect(ImVec2(minX - half, minY - half), ImVec2(maxX + half, maxY + half),
            midRadius);
        // 注意参数顺序是 (col, thickness, flags)：这个 ImGui 版本的 PathStroke
        // 不是 (col, flags, thickness)。写成后者编译器不报错（都能隐式转换），
        // 但 thickness 变成 1、flags 变成垃圾 → 什么都画不出来（踩过）。
        drawList->PathStroke(col, expand, ImDrawFlags_Closed);
    }
}

void resetEnvironmentProbeForTest()
{
    probed = false;
}

} // namespace shadow
} // namespace bklv
