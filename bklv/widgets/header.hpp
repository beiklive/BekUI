#pragma once

// 页头（对应 borealis 的 views/header，以及旧 AppletFrame 里那个 88 高的应用页头）。
// 现在由宿主建好交给 `Shell::setHeaderView()`（外壳只负责挂到覆盖层）。
//
// 结构（和 borealis 的 header XML 一致：一行里放 图标 / 标题 / 右侧区）：
//
//   Header（装饰层在 widgets/header.json：下边线 + 可选底色）
//     ├─ icon        图标（AppearanceView("base/header_icon")，image 层；默认隐藏）
//     ├─ titleLabel  标题（Label）
//     ├─ padding     顶开标题与右侧区（Padding，grow 1）
//     └─ hintBox     右侧区（Box，右对齐；外部可以往里塞自己的东西）
//          └─ subtitleLabel  右侧文字（setSubtitle 用，可为空）
//
// 尺寸/留白/防线全在主题里：header 的 box（高 88、左右 margin=appletFrame.paddingSides、
// 上下 padding、alignItems=center）+ header_icon 的 box（尺寸与左边距）。
// 标题与副标题是 Label（语义控件），字号与颜色由 Header::applyTheme() 从主题键取
// （metric.header.titleFontSize / metric.font.size.sm、color.text.primary / color.text.dim）。

#include <string>

#include "bklv/core/graphics.hpp"
#include "bklv/views/label.hpp"
#include "bklv/views/padding.hpp"
#include "bklv/views/scrolling_frame.hpp"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{

// 标题图标：外观里的 image 层负责画，纹理由 Header 给（没设图标时隐藏）
class HeaderIcon : public AppearanceView
{
  public:
    HeaderIcon();

    void setTexture(const TextureRef& value) { this->texture = value; }
    bool hasTexture() const { return this->texture.isValid(); }

    std::string getClassString() const override { return "bklv::HeaderIcon"; }

  protected:
    TextureRef getLayerImage(const std::string& source) override;

  private:
    TextureRef texture;
};

class Header : public AppearanceView
{
  public:
    explicit Header(const std::string& variant = "");

    void setTitle(const std::string& text);
    void setSubtitle(const std::string& text);
    // 图标路径（相对资源目录，如 "resources/icon.png"）；传空串隐藏图标
    void setIcon(const std::string& path);

    const std::string& getTitle() const { return this->title; }
    const std::string& getSubtitle() const { return this->subtitle; }

    // 右侧区：外部可以往里加自己的视图（borealis 的 brls/applet_frame/hint_box）
    Box* getHintBox() const { return this->hintBox; }
    Label* getTitleLabel() const { return this->titleLabel; }
    HeaderIcon* getIcon() const { return this->icon; }

    // ---- 大标题（iOS 风格：内容在顶部时是大标题，滚动后收成页头里的小标题）----
    //
    // 大标题是**绝对定位**的子视图，落在页头下沿之外，所以不参与页头布局、不引起重排；
    // 整个收起过程只改**绘制期**的量（alpha / 缩放 / 位移）。宿主只需要：
    //
    //     header->setLargeTitle(translate("app/title"));
    //     header->setCollapseSource(this->scroller);   // 传包住内容的 ScrollingFrame
    //
    // 之后每帧由 Header::update() 自己按滚动偏移算进度（不绑滚动源时也可手动
    // setCollapseProgress()）。两套主题都适用：ios 主题字号更大，观感就是苹果那一套。
    void setLargeTitle(const std::string& text);
    const std::string& getLargeTitle() const { return this->largeTitle; }
    Label* getLargeTitleLabel() const { return this->largeTitleLabel; }

    // 收起进度从哪个滚动器读（nullptr = 手动模式，进度保持不变）
    void setCollapseSource(ScrollingFrame* scroller) { this->collapseSource = scroller; }
    ScrollingFrame* getCollapseSource() const { return this->collapseSource; }
    // 滚多少像素算"完全收起"（大约一个列表行的距离）
    void setCollapseDistance(float pixels);
    float getCollapseDistance() const { return this->collapseDistance; }
    // 0 = 展开（只有大标题），1 = 收起（只有页头小标题）
    void setCollapseProgress(float progress);
    float getCollapseProgress() const { return this->collapseProgress; }

    void update() override;

    void applyTheme() override;

    std::string getClassString() const override { return "bklv::Header"; }
    const AttributeTable& attributes() const override;

  private:
    void refreshLabels();

    HeaderIcon* icon       = nullptr;
    Label* titleLabel      = nullptr;
    Padding* padding       = nullptr; // 顶开标题与右侧区
    Box* hintBox           = nullptr;
    Label* subtitleLabel   = nullptr;
    Label* largeTitleLabel = nullptr; // 绝对定位在页头下沿之外（见 setLargeTitle）

    std::string title;
    std::string subtitle;
    std::string largeTitle;

    ScrollingFrame* collapseSource = nullptr;
    float collapseDistance = 44.0f;
    float collapseProgress = 0.0f;
};

} // namespace bklv
