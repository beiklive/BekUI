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

    std::string title;
    std::string subtitle;
};

} // namespace bklv
