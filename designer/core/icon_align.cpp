#include "designer/core/icon_align.hpp"

namespace designer
{

float iconTopForTextLine(float textY, float lineHeight, float iconSize)
{
    const float offset = (lineHeight - iconSize) * 0.5f;
    return textY + (offset > 0.0f ? offset : 0.0f);
}

} // namespace designer
