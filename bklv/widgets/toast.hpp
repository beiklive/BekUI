#pragma once

// Toast：右上角通知浮层（外观在 resources/theme/.../base/toast/*.json）。
//
//   ToastManager（Box，绝对定位右上角）
//     └─ 若干 ToastItem（AppearanceView，base/toast/toast.{success,error,info}）
//
// 行为（与 GUI_DEV 的 Toast 对齐）：
//   · 入场 250ms 从右侧滑入（EaseOutCubic），停留 3s（从入场完成算），出场 250ms 滑出；
//   · 多条按列排队、间距 10；
//   · 类型只决定外观变体（色条/图标颜色），卡片样式在 JSON 里。
//
// 用法：
//   ToastManager* toasts = new ToastManager();
//   shell->getHudLayer()->addView(toasts);
//   toasts->showSuccess("存档已保存");

#include <string>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/core/view.hpp"

namespace bklv
{

enum class ToastType
{
    SUCCESS,
    ERROR,
    INFO,
};

class AppearanceView;

class ToastManager : public Box
{
  public:
    ToastManager();

    void show(ToastType type, const std::string& message);
    void showSuccess(const std::string& message) { this->show(ToastType::SUCCESS, message); }
    void showError(const std::string& message) { this->show(ToastType::ERROR, message); }
    void showInfo(const std::string& message) { this->show(ToastType::INFO, message); }

    void clear();
    size_t count() const { return this->entries.size(); }

    // 停留时长（秒，从入场完成开始算）与动画时长
    void setVisibleDuration(float seconds) { this->visibleDuration = seconds; }
    void setEnterDuration(float seconds) { this->enterDuration = seconds; }
    void setExitDuration(float seconds) { this->exitDuration = seconds; }

    void update() override;

    std::string getClassString() const override { return "bklv::ToastManager"; }

  private:
    struct Entry
    {
        AppearanceView* view    = nullptr;
        float           elapsed = 0.0f; // 入场完成后的停留计时
        float           slide   = 0.0f; // 0 = 屏幕右侧外，1 = 停靠位
        bool            exiting = false;
        float           exitAt  = 0.0f;
    };

    std::vector<Entry> entries;
    float visibleDuration = 3.0f;
    float enterDuration   = 0.25f;
    float exitDuration    = 0.25f;
};

} // namespace bklv
