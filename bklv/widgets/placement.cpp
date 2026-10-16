#include "bklv/widgets/placement.hpp"

namespace bklv
{

namespace
{

// 铺满容器的盒子：四边 0 的绝对定位（居中类拿它当包装盒）。
Box* makeFullSizeWrapper(Axis axis, JustifyContent justify, AlignItems align)
{
    Box* wrapper = new Box(axis);
    wrapper->setPositionType(PositionType::ABSOLUTE);
    wrapper->setPositionTop(0.0f);
    wrapper->setPositionRight(0.0f);
    wrapper->setPositionBottom(0.0f);
    wrapper->setPositionLeft(0.0f);
    wrapper->setJustifyContent(justify);
    wrapper->setAlignItems(align);
    return wrapper;
}

// 居中类角落共用的三步：建包装盒 → 按方位给一条边偏移 → 把内容放进去。
Box* addCenteringWrapper(Box* layer, View* view, Axis axis, JustifyContent justify,
    AlignItems align)
{
    Box* wrapper = makeFullSizeWrapper(axis, justify, align);
    layer->addView(wrapper);
    wrapper->addView(view);
    return wrapper;
}

} // namespace

void placeInCorner(Box* layer, View* view, Corner corner, float insetX, float insetY)
{
    if (layer == nullptr || view == nullptr)
        return;

    switch (corner)
    {
        case Corner::TOP_LEFT:
            layer->addView(view);
            view->setPositionType(PositionType::ABSOLUTE);
            view->setPositionTop(insetY);
            view->setPositionLeft(insetX);
            break;
        case Corner::TOP_RIGHT:
            layer->addView(view);
            view->setPositionType(PositionType::ABSOLUTE);
            view->setPositionTop(insetY);
            view->setPositionRight(insetX);
            break;
        case Corner::BOTTOM_LEFT:
            layer->addView(view);
            view->setPositionType(PositionType::ABSOLUTE);
            view->setPositionBottom(insetY);
            view->setPositionLeft(insetX);
            break;
        case Corner::BOTTOM_RIGHT:
            layer->addView(view);
            view->setPositionType(PositionType::ABSOLUTE);
            view->setPositionBottom(insetY);
            view->setPositionRight(insetX);
            break;
        case Corner::TOP_CENTER:
            addCenteringWrapper(layer, view, Axis::ROW, JustifyContent::CENTER,
                AlignItems::FLEX_START)
                ->setPositionTop(insetY);
            break;
        case Corner::BOTTOM_CENTER:
            addCenteringWrapper(layer, view, Axis::ROW, JustifyContent::CENTER,
                AlignItems::FLEX_END)
                ->setPositionBottom(insetY);
            break;
        case Corner::CENTER_LEFT:
            addCenteringWrapper(layer, view, Axis::COLUMN, JustifyContent::CENTER,
                AlignItems::FLEX_START)
                ->setPositionLeft(insetX);
            break;
        case Corner::CENTER_RIGHT:
            addCenteringWrapper(layer, view, Axis::COLUMN, JustifyContent::CENTER,
                AlignItems::FLEX_END)
                ->setPositionRight(insetX);
            break;
        case Corner::CENTER:
            addCenteringWrapper(layer, view, Axis::COLUMN, JustifyContent::CENTER,
                AlignItems::CENTER);
            break;
    }
}

void placeOnEdge(Box* layer, View* view, Edge edge, float inset)
{
    if (layer == nullptr || view == nullptr)
        return;

    layer->addView(view);
    view->setPositionType(PositionType::ABSOLUTE);
    switch (edge)
    {
        case Edge::TOP:
            view->setPositionTop(inset);
            view->setPositionLeft(0.0f);
            view->setPositionRight(0.0f);
            break;
        case Edge::RIGHT:
            view->setPositionRight(inset);
            view->setPositionTop(0.0f);
            view->setPositionBottom(0.0f);
            break;
        case Edge::BOTTOM:
            view->setPositionBottom(inset);
            view->setPositionLeft(0.0f);
            view->setPositionRight(0.0f);
            break;
        case Edge::LEFT:
            view->setPositionLeft(inset);
            view->setPositionTop(0.0f);
            view->setPositionBottom(0.0f);
            break;
    }
}

void fillLayerArea(Box* layer, View* view)
{
    if (layer == nullptr || view == nullptr)
        return;

    layer->addView(view);
    view->setPositionType(PositionType::ABSOLUTE);
    view->setPositionTop(0.0f);
    view->setPositionRight(0.0f);
    view->setPositionBottom(0.0f);
    view->setPositionLeft(0.0f);
}

} // namespace bklv
