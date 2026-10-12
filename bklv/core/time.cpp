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

    constexpr float PI = 3.14159265358979323846f;

    // 复用：In 型曲线做 Out 就是镜像（1 - f(1-t)），Out 做 InOut 就是两半拼接。
    // 这样每种曲线只写一个公式，其余两挡自动得到，也不容易写错。
    auto makeOut = [&](float (*in)(float), float x) { return 1.0f - in(1.0f - x); };
    auto makeInOut = [&](float (*in)(float), float (*out)(float), float x) {
        return x < 0.5f ? in(x * 2.0f) * 0.5f : out(x * 2.0f - 1.0f) * 0.5f + 0.5f;
    };

    auto quadIn = [](float x) { return x * x; };
    auto cubicIn = [](float x) { return x * x * x; };
    auto quartIn = [](float x) { return x * x * x * x; };
    auto quintIn = [](float x) { return x * x * x * x * x; };
    auto sineIn = [&](float x) { return 1.0f - std::cos(x * PI * 0.5f); };
    auto circIn = [](float x) { return 1.0f - std::sqrt(std::max(0.0f, 1.0f - x * x)); };
    auto expoIn = [](float x) { return x <= 0.0f ? 0.0f : std::pow(2.0f, 10.0f * x - 10.0f); };
    auto backIn = [](float x) {
        constexpr float c1 = 1.70158f;
        return (c1 + 1.0f) * x * x * x - c1 * x * x;
    };
    auto bounceOutFn = [](float x) {
        constexpr float n1 = 7.5625f;
        constexpr float d1 = 2.75f;
        if (x < 1.0f / d1)
            return n1 * x * x;
        if (x < 2.0f / d1)
        {
            x -= 1.5f / d1;
            return n1 * x * x + 0.75f;
        }
        if (x < 2.5f / d1)
        {
            x -= 2.25f / d1;
            return n1 * x * x + 0.9375f;
        }
        x -= 2.625f / d1;
        return n1 * x * x + 0.984375f;
    };
    auto bounceInFn = [&](float x) { return 1.0f - bounceOutFn(1.0f - x); };
    auto elasticInFn = [&](float x) {
        if (x <= 0.0f)
            return 0.0f;
        if (x >= 1.0f)
            return 1.0f;
        constexpr float c4 = (2.0f * PI) / 3.0f;
        return -std::pow(2.0f, 10.0f * x - 10.0f) * std::sin((x * 10.0f - 10.75f) * c4);
    };

    switch (easing)
    {
        case EasingFunction::LINEAR:
            return t;

        case EasingFunction::QUADRATIC_IN:
            return quadIn(t);
        case EasingFunction::QUADRATIC_OUT:
            return makeOut(+[](float x) { return x * x; }, t);
        case EasingFunction::QUADRATIC_IN_OUT:
            return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;

        case EasingFunction::CUBIC_IN:
            return cubicIn(t);
        case EasingFunction::CUBIC_OUT:
        {
            const float inv = t - 1.0f;
            return inv * inv * inv + 1.0f;
        }
        case EasingFunction::CUBIC_IN_OUT:
        {
            if (t < 0.5f)
                return 4.0f * t * t * t;
            const float inv = 2.0f * t - 2.0f;
            return 0.5f * inv * inv * inv + 1.0f;
        }

        case EasingFunction::QUARTIC_IN:
            return quartIn(t);
        case EasingFunction::QUARTIC_OUT:
            return makeOut(+[](float x) { return x * x * x * x; }, t);
        case EasingFunction::QUARTIC_IN_OUT:
        {
            if (t < 0.5f)
                return 8.0f * quartIn(t);
            const float inv = 1.0f - (2.0f * t - 1.0f);
            return 1.0f - 8.0f * quartIn(inv);
        }

        case EasingFunction::QUINTIC_IN:
            return quintIn(t);
        case EasingFunction::QUINTIC_OUT:
            return makeOut(+[](float x) { return x * x * x * x * x; }, t);
        case EasingFunction::QUINTIC_IN_OUT:
        {
            if (t < 0.5f)
                return 16.0f * quintIn(t);
            const float inv = 1.0f - (2.0f * t - 1.0f);
            return 1.0f - 16.0f * quintIn(inv);
        }

        case EasingFunction::SINE_IN:
            return sineIn(t);
        case EasingFunction::SINE_OUT:
            return std::sin(t * PI * 0.5f);
        case EasingFunction::SINE_IN_OUT:
            return -(std::cos(PI * t) - 1.0f) * 0.5f;

        case EasingFunction::CIRCULAR_IN:
            return circIn(t);
        case EasingFunction::CIRCULAR_OUT:
            return makeOut(+[](float x) {
                return 1.0f - std::sqrt(std::max(0.0f, 1.0f - x * x));
            }, t);
        case EasingFunction::CIRCULAR_IN_OUT:
        {
            if (t < 0.5f)
                return (1.0f - std::sqrt(std::max(0.0f, 1.0f - 4.0f * t * t))) * 0.5f;
            const float x = 2.0f * t - 2.0f;
            return (std::sqrt(std::max(0.0f, 1.0f - x * x)) + 1.0f) * 0.5f;
        }

        case EasingFunction::EXPONENTIAL_IN:
            return expoIn(t);
        case EasingFunction::EXPONENTIAL_OUT:
            return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
        case EasingFunction::EXPONENTIAL_IN_OUT:
        {
            if (t <= 0.0f)
                return 0.0f;
            if (t >= 1.0f)
                return 1.0f;
            return t < 0.5f ? std::pow(2.0f, 20.0f * t - 10.0f) * 0.5f
                            : (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) * 0.5f;
        }

        case EasingFunction::ELASTIC_IN:
            return elasticInFn(t);
        case EasingFunction::ELASTIC_OUT:
            return makeOut(+[](float x) {
                if (x <= 0.0f)
                    return 0.0f;
                if (x >= 1.0f)
                    return 1.0f;
                constexpr float c4 = (2.0f * 3.14159265358979323846f) / 3.0f;
                return std::pow(2.0f, -10.0f * x) * std::sin((x * 10.0f - 0.75f) * c4) + 1.0f;
            }, t);
        case EasingFunction::ELASTIC_IN_OUT:
        {
            if (t <= 0.0f)
                return 0.0f;
            if (t >= 1.0f)
                return 1.0f;
            constexpr float c5 = (2.0f * 3.14159265358979323846f) / 4.5f;
            return t < 0.5f
                ? -(std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) * 0.5f
                : (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) * 0.5f
                    + 1.0f;
        }

        case EasingFunction::BACK_IN:
            return backIn(t);
        case EasingFunction::BACK_OUT:
            return makeOut(+[](float x) {
                constexpr float c1 = 1.70158f;
                const float inv    = x - 1.0f;
                return 1.0f + (c1 + 1.0f) * inv * inv * inv + c1 * inv * inv;
            }, t);
        case EasingFunction::BACK_IN_OUT:
        {
            constexpr float c2 = 1.70158f * 1.525f;
            if (t < 0.5f)
            {
                const float x = 2.0f * t;
                return (x * x * ((c2 + 1.0f) * x - c2)) * 0.5f;
            }
            const float x = 2.0f * t - 2.0f;
            return (x * x * ((c2 + 1.0f) * x + c2) + 2.0f) * 0.5f;
        }

        case EasingFunction::BOUNCE_IN:
            return bounceInFn(t);
        case EasingFunction::BOUNCE_OUT:
            return bounceOutFn(t);
        case EasingFunction::BOUNCE_IN_OUT:
        {
            if (t < 0.5f)
                return (1.0f - bounceOutFn(1.0f - 2.0f * t)) * 0.5f;
            return (1.0f + bounceOutFn(2.0f * t - 1.0f)) * 0.5f;
        }
    }

    (void)makeInOut;
    (void)backIn;
    (void)circIn;
    (void)elasticInFn;
    (void)sineIn;
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

    // 用受保护的帧间隔：没有 ImGui 上下文时（无窗口测试）也能跑
    const float deltaTime = getFrameDeltaSeconds();
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
