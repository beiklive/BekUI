#pragma once

// 通知（Toast），对应 borealis 的 core/notification_manager。
//
// 结构（和 Button 家族一个套路）：
//   Notification（抽象基类）—— 只管行为与卡片外观
//     ├─ IconNotification   图标 + 文字
//     └─ ImageNotification  图片 + 文字
//   NotificationManager —— 右上角一列，管堆叠、各自倒计时、摘除
//
// 为什么没有"纯文字"的具体类：通知总得有个视觉锚点（图标或缩略图），
// 只放一行字在深色卡片里既难认也不好看。要纯文字就把图标换成一个字形，
// 不要为它单开一类。
//
// 与 borealis 的差别（有意）：
// - borealis 的 `NotificationManager` 是**全局静态单例**，多实例、测试都难受。
//   这里做成挂在 `App` 上的一份普通视图：`App::notify(...)` 转发到它。
// - 卡片本身（底色 / 描边 / 阴影 / 圆角）还是外观 JSON 管
//   （`base/widget/notification.json` 只画卡片），内容用真视图拼（Label / Image）。
// - 动画在代码里做：从**屏幕右侧外面**滑进来，消失时滑回右边；换行（上移）
//   靠收缩自身高度实现，这样下面那几条会在收缩过程中跟着往上挪，而不是"跳一下"。
// - 堆叠**不设上限**：一列往下排，排到屏幕外也不管（外层容器会裁掉）；
//   每条各自倒计时，谁到期谁滑回去并让出位置。

#include <string>
#include <vector>

#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/basic/label.hpp"
#include "bklv/ui/widgets/appearance/appearance_view.hpp"

namespace bklv
{

class Image;

// 通知内容的形态。基类不提供默认实现，派生类各自返回自己的
// （Switch 带 `-fno-rtti`，判断类型只能靠虚函数，不能 dynamic_cast）。
enum class NotificationKind
{
    ICON,  // 图标字体字形 + 文字
    IMAGE, // 图片 + 文字
};

// 一条通知：显示 duration 秒后自动滑回去并把自己从管理器里摘掉。
// 外观、动画、计时都在基类；派生类只决定"内容行里放什么"。
class Notification : public AppearanceView
{
  public:
    void dismiss();
    bool isDismissed() const { return this->dismissed; }

    // 消失动效也播完了，可以从管理器里摘掉
    bool isFinished() const { return this->dismissed && this->dismissProgress >= 1.0f; }

    float getRemaining() const { return this->remaining; }
    const std::string& getText() const { return this->text; }

    // 派生类返回自己的形态（替代 dynamic_cast 做类型判断）
    virtual NotificationKind getKind() const = 0;

    void update() override;
    void applyTheme() override;

    std::string getClassString() const override { return "bklv::Notification"; }

  protected:
    // 基类构造：套卡片外观、建内容行与文案标签，再交给派生类塞自己的东西
    explicit Notification(const std::string& text, float duration);

    // 派生类在这个函数里往内容行**最前面**塞图标 / 图片（文字已经在行里了）
    virtual void buildContent() = 0;

    // 派生类补自己的样式（基类管行间距、文案字号与颜色、滑动距离）
    virtual void applyContentTheme();

    Box* getContentRow() const { return this->row; }
    Label* getTextLabel() const { return this->textLabel; }

  private:
    void setupBaseContent();

    Box* row = nullptr; // 内容行：[图标/图片][文案]
    Label* textLabel = nullptr;

    std::string text;
    float duration = 3.0f;
    float remaining = 0.0f;

    bool dismissed = false;

    // 动画进度（线性推进，够用且好读）
    float appearProgress = 0.0f;  // 0 → 1：从右边滑进来
    float dismissProgress = 0.0f; // 0 → 1：滑回去 + 高度收回
    float slideDistance = 360.0f; // 滑动的横向距离（主题里取，缺省按卡片宽度估算）
    float fullHeight = 0.0f;      // 消失开始时的高度，用来逐步收成全 0
};

// 图标 + 文字：glyph 是图标字体的字形（例如 `bklv::icon::glyph(icon::Id::INFO)`）
class IconNotification : public Notification
{
  public:
    IconNotification(const std::string& text, const std::string& glyph, float duration = 3.0f);

    void setIcon(const std::string& glyph);
    const std::string& getIcon() const { return this->glyph; }

    NotificationKind getKind() const override { return NotificationKind::ICON; }
    std::string getClassString() const override { return "bklv::IconNotification"; }

  protected:
    void buildContent() override;
    void applyContentTheme() override;

  private:
    Label* iconLabel = nullptr;
    std::string glyph;
};

// 图片 + 文字：path 走资源路径（`resources/...`），异步解码
class ImageNotification : public Notification
{
  public:
    ImageNotification(
        const std::string& text, const std::string& imagePath, float duration = 3.0f);

    const std::string& getImagePath() const { return this->imagePath; }

    NotificationKind getKind() const override { return NotificationKind::IMAGE; }
    std::string getClassString() const override { return "bklv::ImageNotification"; }

  protected:
    void buildContent() override;
    void applyContentTheme() override;

  private:
    Image* image = nullptr;
    std::string imagePath;
};

// 右上角一列通知。不设可见上限：排到屏幕外也不管，各自倒计时各自收。
class NotificationManager : public Box
{
  public:
    NotificationManager();

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
