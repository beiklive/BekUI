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

// 自进程启动以来的秒数（double）。**不依赖 ImGui 上下文**，所以无窗口测试里也能用。
//
// 需要"过一个时间点"这类判断时用它，别用 ImGui::GetTime()：后者的时间基是 ImGui
// 上下文创建时刻，和这里的进程启动时刻不同源，混用会出现"设了一个未来时刻、却立刻过期"
// （Button 的按下反馈踩过：设置走 ImGui::GetTime()、清除走自累计时钟）。
inline double getSecondsNow()
{
    return (double)getCPUTimeUsec() / 1000000.0;
}

// 缓动曲线。对齐 borealis 用的 tweeny::easing::enumerated（他们的 EasingFunction
// 就是那个枚举），所以 In/Out/InOut 三挡在 9 种曲线上都齐。
enum class EasingFunction
{
    LINEAR,

    QUADRATIC_IN,
    QUADRATIC_OUT,
    QUADRATIC_IN_OUT,

    CUBIC_IN,
    CUBIC_OUT,
    CUBIC_IN_OUT,

    QUARTIC_IN,
    QUARTIC_OUT,
    QUARTIC_IN_OUT,

    QUINTIC_IN,
    QUINTIC_OUT,
    QUINTIC_IN_OUT,

    SINE_IN,
    SINE_OUT,
    SINE_IN_OUT,

    CIRCULAR_IN,
    CIRCULAR_OUT,
    CIRCULAR_IN_OUT,

    EXPONENTIAL_IN,
    EXPONENTIAL_OUT,
    EXPONENTIAL_IN_OUT,

    ELASTIC_IN,
    ELASTIC_OUT,
    ELASTIC_IN_OUT,

    BACK_IN,
    BACK_OUT,
    BACK_IN_OUT,

    BOUNCE_IN,
    BOUNCE_OUT,
    BOUNCE_IN_OUT,
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
    void addStep(float targetValue, float duration, EasingFunction easing = EasingFunction::LINEAR);

    void start();
    void stop();

    // 停止状态（补间跑完会自己停）
    bool isStopped() const { return !this->isTicking(); }

    void setEndCallback(std::function<void(bool finished)> callback);

    float getValue() const { return this->value; }
    void update() override;

    // 显式推进：不依赖全局帧时间。
    // 转场这类需要确定性步进的场景（以及测试）用这个，update() 内部就是调它。
    void update(float deltaSeconds);

    operator float() const { return this->value; }

  private:
    struct Step
    {
        float target = 0.0f;
        float duration = 0.0f;
        EasingFunction easing = EasingFunction::LINEAR;
    };

    float value = 0.0f;
    float stepStartValue = 0.0f;
    float stepElapsed = 0.0f;
    size_t currentStep = 0;
    std::vector<Step> steps;
    std::function<void(bool)> endCallback;
};

} // namespace bklv
