#pragma once

// 通知（Toast），对应 borealis 的 core/notification_manager。
//
// 与 borealis 的差别（有意）：
// - borealis 的 `NotificationManager` 是**全局静态单例**，多实例、测试都难受。
//   这里做成挂在 `App` 上的一份普通视图：`App::notify(...)` 转发到它。
// - 三种内容：纯文字 / 图标 + 文字 / 图片 + 文字（`NotificationKind`）。
//   卡片本身（底色 / 描边 / 阴影 / 圆角）还是外观 JSON 管：
//   `base/widget/notification.json` 只画卡片，内容用真视图拼（Label / Image）。
// - 动画在代码里做：从**屏幕右侧外面**滑进来，消失时滑回右边；换行（上移）
//   靠收缩自身高度实现，这样下面那几条会在收缩过程中跟着往上挪，而不是"跳一下"。
// - 堆叠**不设上限**：一列往下排，排到屏幕外也不管（外层容器会裁掉）；
//   每条各自倒计时，谁到期谁滑回去并让出位置。
//
// 结构：
//   NotificationManager（右上角一列，不吃焦点）
//     └─ Notification × N（卡片 + 内容行：图标/图片 + 文案，各自倒计时）

#include <string>
#include <vector>

#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/basic/image.hpp"
#include "bklv/ui/widgets/basic/label.hpp"
#include "bklv/ui/widgets/appearance/appearance_view.hpp"

namespace bklv
{

// 通知内容的三种形态
enum class NotificationKind
{
    TEXT,  // 纯文字
    ICON,  // 图标字体字形 + 文字
    IMAGE, // 图片 + 文字
};

// 一条通知：显示 duration 秒后自动滑回去并把自己从管理器里摘掉
class Notification : public AppearanceView
{
  public:
    explicit Notification(const std::string& text, float duration = 3.0f);

    // 图标 + 文字：glyph 是图标字体的字形（例如 `bklv::icon::glyph(icon::Id::INFO)`）
    static Notification* withIcon(
        const std::string& text, const std::string& glyph, float duration = 3.0f);

    // 图片 + 文字：path 走资源路径（`resources/...` 或 `@theme/...`），异步解码
    static Notification* withImage(
        const std::string& text, const std::string& imagePath, float duration = 3.0f);

    void setText(const std::string& value);
    const std::string& getText() const { return this->text; }

    NotificationKind getKind() const { return this->kind; }

    // 立刻开始消失（用户按键关掉 / 管理器清理时用）
    void dismiss();

    bool isDismissed() const { return this->dismissed; }

    // 消失动效也播完了，可以从管理器里摘掉
    bool isFinished() const { return this->dismissed && this->dismissProgress >= 1.0f; }

    float getRemaining() const { return this->remaining; }

    void update() override;
    void applyTheme() override;

    std::string getClassString() const override { return "bklv::Notification"; }

  private:
    // 三种形态共用的构造入口（public 的三个构造函数/工厂都转到这里）
    Notification(NotificationKind kind, const std::string& text,
        const std::string& glyphOrImagePath, float duration);

    // 建内容行（图标 / 图片 + 文案）
    void setupContent();

    void applyContentTheme();

    Box* row = nullptr;      // 内容行：图标/图片 + 文案
    Label* textLabel = nullptr;
    Label* iconLabel = nullptr;
    Image* image = nullptr;

    NotificationKind kind = NotificationKind::TEXT;
    std::string text;
    std::string glyphOrPath; // ICON 时是字形，IMAGE 时是图片路径
    float duration = 3.0f;
    float remaining = 0.0f;

    bool dismissed = false;

    // 动画进度（线性推进，够用且好读）
    float appearProgress = 0.0f;  // 0 → 1：从右边滑进来
    float dismissProgress = 0.0f; // 0 → 1：滑回去 + 高度收回
    float slideDistance = 360.0f; // 滑动的横向距离（主题里取，缺省按卡片宽度估算）
    float fullHeight = 0.0f;      // 消失开始时的高度，用来逐步收成全 0
};

// 右上角一列通知。不设可见上限：排到屏幕外也不管，各自倒计时各自收。
class NotificationManager : public Box
{
  public:
    NotificationManager();

    // 三种内容各一个入口
    Notification* notify(const std::string& text, float duration = 3.0f);
    Notification* notifyIcon(
        const std::string& text, const std::string& glyph, float duration = 3.0f);
    Notification* notifyImage(
        const std::string& text, const std::string& imagePath, float duration = 3.0f);

    size_t getCount() const { return this->items.size(); }
    const std::vector<Notification*>& getItems() const { return this->items; }

    // 浮层不吃焦点
    bool isFocusable() const override { return false; }

    // 清掉已经消失完的条目（每帧自己会调）
    void cleanup();

    void update() override;
    void applyTheme() override;

    std::string getClassString() const override { return "bklv::NotificationManager"; }

  private:
    Notification* add(Notification* item);

    std::vector<Notification*> items;
};

} // namespace bklv
