#include "bklv/core/time.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "imgui.h"

namespace bklv
{

float getFrameDeltaSeconds()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return 1.0f / 60.0f;

    return ImGui::GetIO().DeltaTime;
}

// ---- Ticking 全局注册表 ----

static std::vector<Ticking*>& tickingRegistry()
{
    static std::vector<Ticking*> registry;
    return registry;
}

void registerTicking(Ticking* ticking)
{
    tickingRegistry().push_back(ticking);
}

void unregisterTicking(Ticking* ticking)
{
    auto& registry = tickingRegistry();
    registry.erase(std::remove(registry.begin(), registry.end(), ticking), registry.end());
}

uint64_t getCPUTimeUsec()
{
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return static_cast<uint64_t>(duration_cast<microseconds>(steady_clock::now() - start).count());
}

float applyEasing(EasingFunction easing, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    switch (easing)
    {
        case EasingFunction::linear:
            return t;
        case EasingFunction::quadraticIn:
            return t * t;
        case EasingFunction::quadraticOut:
            return t * (2.0f - t);
        case EasingFunction::quadraticInOut:
            return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
        case EasingFunction::cubicIn:
            return t * t * t;
        case EasingFunction::cubicOut:
        {
            const float inv = t - 1.0f;
            return inv * inv * inv + 1.0f;
        }
        case EasingFunction::cubicInOut:
        {
            if (t < 0.5f)
                return 4.0f * t * t * t;
            const float inv = 2.0f * t - 2.0f;
            return 0.5f * inv * inv * inv + 1.0f;
        }
        case EasingFunction::bounceOut:
        {
            constexpr float n1 = 7.5625f;
            constexpr float d1 = 2.75f;
            if (t < 1.0f / d1)
                return n1 * t * t;
            if (t < 2.0f / d1)
            {
                t -= 1.5f / d1;
                return n1 * t * t + 0.75f;
            }
            if (t < 2.5f / d1)
            {
                t -= 2.25f / d1;
                return n1 * t * t + 0.9375f;
            }
            t -= 2.625f / d1;
            return n1 * t * t + 0.984375f;
        }
        case EasingFunction::sineInOut:
            return -(std::cos(3.14159265f * t) - 1.0f) * 0.5f;
    }
    return t;
}

Ticking::Ticking()
{
    registerTicking(this);
}

Ticking::~Ticking()
{
    unregisterTicking(this);
}

void Ticking::startTicking()
{
    this->ticking = true;
}

void Ticking::stopTicking()
{
    this->ticking = false;
}

void Ticking::resetTicking()
{
    this->ticking = false;
}

void Ticking::updateTickings()
{
    // 遍历时可能新增/移除元素，先拷贝一份快照。
    std::vector<Ticking*> snapshot = tickingRegistry();
    for (Ticking* ticking : snapshot)
    {
        if (ticking->isTicking())
            ticking->update();
    }
}

// ---- Animatable ----

Animatable::Animatable(float value)
    : value(value)
{
}

Animatable::~Animatable() = default;

void Animatable::reset(float initialValue)
{
    this->stop();
    this->steps.clear();
    this->value          = initialValue;
    this->stepStartValue = initialValue;
    this->stepElapsed    = 0.0f;
    this->currentStep    = 0;
}

void Animatable::addStep(float targetValue, float duration, EasingFunction easing)
{
    this->steps.push_back({ targetValue, duration, easing });
}

void Animatable::start()
{
    if (this->steps.empty())
        return;

    this->stepStartValue = this->value;
    this->stepElapsed    = 0.0f;
    this->currentStep    = 0;
    this->startTicking();
}

void Animatable::stop()
{
    this->stopTicking();
}

void Animatable::setEndCallback(std::function<void(bool finished)> callback)
{
    this->endCallback = std::move(callback);
}

void Animatable::update()
{
    if (this->currentStep >= this->steps.size())
    {
        this->stop();
        return;
    }

    const float deltaTime = ImGui::GetIO().DeltaTime;
    const Step& step      = this->steps[this->currentStep];

    this->stepElapsed += deltaTime;

    if (step.duration <= 0.0f || this->stepElapsed >= step.duration)
    {
        this->value          = step.target;
        this->stepElapsed    = 0.0f;
        this->stepStartValue = this->value;
        this->currentStep++;

        if (this->currentStep >= this->steps.size())
        {
            this->stop();
            if (this->endCallback)
                this->endCallback(true);
            return;
        }
        return;
    }

    const float t = this->stepElapsed / step.duration;
    this->value   = this->stepStartValue + (step.target - this->stepStartValue) * applyEasing(step.easing, t);
}

} // namespace bklv
