#include "bklv/input/gesture.hpp"

#include <cmath>

#include "bklv/anim/time.hpp"

namespace bklv
{

// ---------------------------------------------------------------- 轻触

TapGestureRecognizer::TapGestureRecognizer(
    std::function<void()> onTap, float maxDistance, float maxDuration)
    : onTap(std::move(onTap))
    , maxDistance(maxDistance)
    , maxDuration(maxDuration)
{
}

bool TapGestureRecognizer::recognize(GestureState state, const Point& position, const Point& delta)
{
    (void)delta;

    switch (state)
    {
        case GestureState::START:
            this->active  = true;
            this->start   = position;
            this->elapsed = 0.0f;
            return true; // 先占住，松开时再决定算不算轻触

        case GestureState::STAY:
        {
            if (!this->active)
                return false;

            this->elapsed += getFrameDeltaSeconds();

            const float dx = position.x - this->start.x;
            const float dy = position.y - this->start.y;
            if (std::sqrt(dx * dx + dy * dy) > this->maxDistance
                || this->elapsed > this->maxDuration)
            {
                // 动了或者按太久：这次不是轻触，直接作废（但仍吃掉事件，免得又变成别的动作）
                this->active = false;
            }
            return true;
        }

        case GestureState::END:
        {
            const bool fired = this->active;
            this->active     = false;
            if (fired && this->onTap)
                this->onTap();
            return fired;
        }

        case GestureState::CANCEL:
            this->active = false;
            return true;

        case GestureState::SCROLL:
            return false; // 轻触不关心滚轮
    }

    return false;
}

// ---------------------------------------------------------------- 拖动

PanGestureRecognizer::PanGestureRecognizer(Callbacks callbacks)
    : callbacks(std::move(callbacks))
{
}

bool PanGestureRecognizer::recognize(GestureState state, const Point& position, const Point& delta)
{
    switch (state)
    {
        case GestureState::START:
            this->active = true;
            if (this->callbacks.onStart)
                this->callbacks.onStart(position);
            return true;

        case GestureState::STAY:
            if (!this->active)
                return false;
            if (this->callbacks.onPan)
                this->callbacks.onPan(delta);
            return true;

        case GestureState::END:
        {
            if (!this->active)
                return false;

            this->active = false;
            if (this->callbacks.onEnd)
                this->callbacks.onEnd();
            return true;
        }

        case GestureState::CANCEL:
            if (!this->active)
                return false;

            this->active = false;
            if (this->callbacks.onEnd)
                this->callbacks.onEnd();
            return true;

        case GestureState::SCROLL:
            return false; // 拖动不关心滚轮
    }

    return false;
}

// ---------------------------------------------------------------- 滚动

ScrollGestureRecognizer::ScrollGestureRecognizer(std::function<void(const Point& delta)> onScroll)
    : onScroll(std::move(onScroll))
{
}

bool ScrollGestureRecognizer::recognize(GestureState state, const Point&, const Point& delta)
{
    // 滚动是"瞬时"事件，用单独的 SCROLL 状态，免得和拖动/轻触互相干扰
    if (state != GestureState::SCROLL)
        return false;

    if (delta.x == 0.0f && delta.y == 0.0f)
        return false;

    if (this->onScroll)
        this->onScroll(delta);

    return true;
}

} // namespace bklv
