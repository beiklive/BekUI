#include "designer/glass/glass_queue.hpp"

#include <algorithm>

namespace designer::glass
{

namespace
{

bool intersects(const ImVec4& a, const ImVec4& b) noexcept
{
    return a.x < b.z && a.z > b.x && a.y < b.w && a.w > b.y;
}

float area(const ImVec4& rect) noexcept
{
    return std::max(0.0f, rect.z - rect.x) * std::max(0.0f, rect.w - rect.y);
}

// 把 rect 裁到"避开 forbidden 的更大那一半"：上下左右四个候选里挑面积最大的。
// 不做真正的矩形差集（那不一定是矩形）——设计器里玻璃是横条/侧栏，
// 这样裁出来的结果就是"整块让开"，够用且可解释。
ImVec4 largestRemainder(const ImVec4& rect, const ImVec4& forbidden) noexcept
{
    ImVec4 best = rect;
    float bestArea = area(rect);

    const ImVec4 candidates[4] = {
        ImVec4(rect.x, rect.y, rect.z, forbidden.y),  // 留在禁区上方
        ImVec4(rect.x, forbidden.w, rect.z, rect.w),  // 留在禁区下方
        ImVec4(rect.x, rect.y, forbidden.x, rect.w),  // 留在禁区左侧
        ImVec4(forbidden.z, rect.y, rect.z, rect.w),  // 留在禁区右侧
    };

    for (const ImVec4& candidate : candidates)
    {
        const float candidateArea = area(candidate);
        if (candidateArea > bestArea && candidate.z > candidate.x && candidate.w > candidate.y)
        {
            best     = candidate;
            bestArea = candidateArea;
        }
    }
    return best;
}

} // namespace

void GlassRequestQueue::beginFrame(GlassBackdropStyle style, float frameDensity)
{
    this->requests.clear();
    this->forbiddenRects.clear();
    this->backdropStyle = style;
    this->density       = frameDensity > 0.0f ? frameDensity : 1.0f;
    this->sealed        = false;
}

void GlassRequestQueue::submit(GlassRequest request)
{
    if (this->sealed)
        return;
    this->requests.push_back(request);
}

void GlassRequestQueue::excludeRect(const ImVec4& forbidden)
{
    if (this->sealed)
        return;
    if (forbidden.z <= forbidden.x || forbidden.w <= forbidden.y)
        return;

    this->forbiddenRects.push_back(forbidden);
}

void GlassRequestQueue::seal()
{
    if (this->sealed)
        return;
    this->sealed = true;

    if (this->forbiddenRects.empty())
        return;

    std::vector<GlassRequest> kept;
    kept.reserve(this->requests.size());

    for (GlassRequest& request : this->requests)
    {
        ImVec4 rect = request.minMax;
        for (const ImVec4& forbidden : this->forbiddenRects)
        {
            if (!intersects(rect, forbidden))
                continue;
            rect = largestRemainder(rect, forbidden);
            if (rect.z <= rect.x || rect.w <= rect.y)
                break;
        }

        // 完全被禁区吃掉就丢掉这块玻璃（宁可没有玻璃，也不能糊到预览区上）
        if (rect.z <= rect.x || rect.w <= rect.y)
            continue;

        request.minMax = rect;
        kept.push_back(request);
    }

    this->requests.swap(kept);
}

} // namespace designer::glass
