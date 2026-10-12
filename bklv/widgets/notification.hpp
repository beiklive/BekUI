#pragma once

// 通知（Toast），对应 borealis 的 core/notification_manager。
//
// 与 borealis 的差别（有意）：
// - borealis 的 `NotificationManager` 是**全局静态单例**（`NotificationManager::notify`），
//   多实例、测试都难受。这里做成挂在 `App` 上的一份普通视图：`App::notify("...")`
//   转发到它，想自己建也可以（`new NotificationManager()` 加到任意容器里）。
// - 内容与动画都在外观 JSON 里（`widgets/notification.json`）：出现/消失是
//   `shown` / `hidden` 两个状态 + 补间，和库里的弹窗一套机制。
//
// 结构：
//   NotificationManager（右上角一列）
//     └─ Notification × N（文案 + 到期自动消失）

#include <string>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/views/label.hpp"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{

// 一条通知：显示 duration 秒后自动播放消失动效并把自己从管理器里摘掉
class Notification : public AppearanceView
{
  public:
    explicit Notification(const std::string& text, float duration = 3.0f);

    void setText(const std::string& value);
    const std::string& getText() const { return this->text; }

    // 立刻开始消失（用户按键关掉 / 管理器清理时用）
    void dismiss();

    bool isDismissed() const { return this->dismissed; }
    float getRemaining() const { return this->remaining; }

    void update() override;

    std::string getClassString() const override { return "bklv::Notification"; }

  private:
    Label* label   = nullptr;
    std::string text;
    float duration = 3.0f;
    float remaining = 0.0f;
    bool dismissed = false;
    bool hidden    = false;
};

class NotificationManager : public Box
{
  public:
    explicit NotificationManager(size_t maxVisible = 3);

    // 弹一条通知（超出上限时最旧的先消失）
    Notification* notify(const std::string& text, float duration = 3.0f);

    size_t getCount() const { return this->items.size(); }
    const std::vector<Notification*>& getItems() const { return this->items; }

    void setMaxVisible(size_t value) { this->maxVisible = value; }
    size_t getMaxVisible() const { return this->maxVisible; }

    // 浮层不吃焦点
    bool isFocusable() const override { return false; }

    // 清掉已经消失完的条目（每帧自己会调）
    void cleanup();

    void update() override;
    void applyTheme() override;

    std::string getClassString() const override { return "bklv::NotificationManager"; }

  private:
    std::vector<Notification*> items;
    size_t maxVisible = 3;
};

} // namespace bklv
