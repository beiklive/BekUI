#pragma once

// 时间与补间动画，对应 borealis 的 core/time.hpp 与 core/animation.hpp。
// borealis 底层用 tweeny 实现，这里为了不让 bklv 依赖第三方，改写成自带的实现。
// 自定义的缓动函数可以覆盖 tweeny 的 EasingFunction，而 view.cpp 的用法保持不变。

#include <cstdint>
#include <functional>
#include <vector>

namespace bklv
{

// 这一帧的间隔（秒）。
//
// **带 ImGui 上下文保护**：无窗口测试里没有 ImGui 上下文，直接调 ImGui::GetIO()
// 会踩空指针（测试里踩过）。没有上下文时按 60fps 估算，测试也就能跑动画。
float getFrameDeltaSeconds();

// 返回自进程启动以来的微秒数，对应 borealis 的 getCPUTimeUsec()。
uint64_t getCPUTimeUsec();

enum class EasingFunction
{
    linear,
    quadraticIn,
    quadraticOut,
    quadraticInOut,
    cubicIn,
    cubicOut,
    cubicInOut,
    bounceOut,
    sineInOut,
};

// 按缓动函数把 t（0~1）映射到输出值。
float applyEasing(EasingFunction easing, float t);

// 可被逐帧推进的对象。所有动画都注册到全局列表，由 App 每帧统一更新。
class Ticking
{
  public:
    Ticking();
    virtual ~Ticking();

    virtual void update() = 0;

    static void updateTickings();

    bool isTicking() const { return this->ticking; }

  protected:
    void startTicking();
    void stopTicking();
    void resetTicking();

  private:
    bool ticking = false;

    friend void registerTicking(Ticking* ticking);
    friend void unregisterTicking(Ticking* ticking);
};

// 一段补间：从当前值运动到目标值，用给定缓动曲线。
class Animatable : public Ticking
{
  public:
    Animatable(float value = 0.0f);
    ~Animatable() override;

    // 停止并清空所有步骤，回到起始值。
    void reset(float initialValue);

    // 追加一段动画：经过 duration 秒到达 targetValue，并指定缓动曲线。
    void addStep(float targetValue, float duration, EasingFunction easing = EasingFunction::linear);

    void start();
    void stop();

    void setEndCallback(std::function<void(bool finished)> callback);

    float getValue() const { return this->value; }
    void update() override;

    operator float() const { return this->value; }

  private:
    struct Step
    {
        float target = 0.0f;
        float duration = 0.0f;
        EasingFunction easing = EasingFunction::linear;
    };

    float value = 0.0f;
    float stepStartValue = 0.0f;
    float stepElapsed = 0.0f;
    size_t currentStep = 0;
    std::vector<Step> steps;
    std::function<void(bool)> endCallback;
};

} // namespace bklv
