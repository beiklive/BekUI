#pragma once

// 指针手势：轻触 / 拖动 / 滚轮。
//
// 与 borealis 的差别（有意简化 + 改良）：
// - borealis 的手势状态机带 TouchState/MouseState、UNSURE/INTERRUPTED 互斥阶段与音效，
//   识别器还要自己处理"谁打断谁"。这里只保留够用的四个状态
//   （START/STAY/END/CANCEL）+ "返回值表示是否吃掉这次事件"，
//   互斥交给 `View::dispatchGesture` 沿父链逐级询问（吃到就停）。
// - **生命周期归 View**：`View::addGesture` 收 `std::unique_ptr`，
//   调用方不用管释放（borealis 存裸指针，归属不明确）。
// - 纯逻辑、不碰 ImGui，所以能无窗口测。

#include <functional>
#include <memory>

#include "bklv/geom/geometry.hpp"

namespace bklv
{

enum class GestureState
{
    START,  // 指针按下
    STAY,   // 按住并移动
    END,    // 抬起
    CANCEL, // 被上层打断/取消（比如弹窗抢走输入）
    SCROLL, // 滚轮/双指滚动（只有滚动识别器关心）
};

// 识别器基类：recognize 返回 true 表示这次事件被吃掉，后面的识别器与父视图不再收到
class GestureRecognizer
{
  public:
    virtual ~GestureRecognizer() = default;

    virtual bool recognize(GestureState state, const Point& position, const Point& delta) = 0;

    void setEnabled(bool value) { this->enabled = value; }
    bool isEnabled() const { return this->enabled; }

    // 手势进行中？（START 之后、END/CANCEL 之前）
    bool isActive() const { return this->active; }

    void reset() { this->active = false; }

  protected:
    bool enabled = true;
    bool active  = false;
};

// 轻触：按下到抬起之间位移不超过 maxDistance、时间不超过 maxDuration（秒）才算
class TapGestureRecognizer : public GestureRecognizer
{
  public:
    explicit TapGestureRecognizer(std::function<void()> onTap, float maxDistance = 8.0f,
        float maxDuration = 0.6f);

    bool recognize(GestureState state, const Point& position, const Point& delta) override;

  private:
    std::function<void()> onTap;
    float maxDistance = 8.0f;
    float maxDuration = 0.6f;
    Point start{ 0.0f, 0.0f };
    float elapsed = 0.0f;
};

// 拖动：START 记起点，STAY 报增量，END 收尾
class PanGestureRecognizer : public GestureRecognizer
{
  public:
    struct Callbacks
    {
        std::function<void(const Point& position)> onStart;
        std::function<void(const Point& delta)> onPan;
        std::function<void()> onEnd;
    };

    explicit PanGestureRecognizer(Callbacks callbacks);

    bool recognize(GestureState state, const Point& position, const Point& delta) override;

  private:
    Callbacks callbacks;
};

// 滚轮 / 触摸板双指滚动：直接报增量（左右上下的正负由调用方定义）
class ScrollGestureRecognizer : public GestureRecognizer
{
  public:
    explicit ScrollGestureRecognizer(std::function<void(const Point& delta)> onScroll);

    bool recognize(GestureState state, const Point& position, const Point& delta) override;

  private:
    std::function<void(const Point& delta)> onScroll;
};

} // namespace bklv
