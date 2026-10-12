#include "bklv/views/padding.hpp"

#include "bklv/core/attributes.hpp"

namespace bklv
{

Padding::Padding()
{
    // 宽度/高度 auto 由 Yoga 默认值决定；grow=1 是它唯一的语义
    this->setGrow(1.0f);
}

void Padding::draw(ImDrawList*, float, float, float, float, FrameContext*)
{
    // 故意什么都不画
}

const AttributeTable& Padding::attributes() const
{
    // 没有自己的属性：宽高/边距/grow 都在 view.cpp 的公共表里
    static const AttributeTable table = {};
    return table;
}

} // namespace bklv
