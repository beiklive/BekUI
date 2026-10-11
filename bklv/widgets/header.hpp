#pragma once

// 页头：色条 + 标题 + 右侧副标题 + 下边线（对应 borealis 的 views/header）。
//
// 它的结构全部写在 `widgets/header.json` 里：
//   rect 层（色条，width=4）+ text 层（source="title"）+ text 层（source="subtitle"，右对齐）
//   + rect 层（下边线，height=1，valign=bottom）
// C++ 这边只提供 title/subtitle 两个数据源，没有一行绘制代码。

#include <string>

#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{

class Header : public AppearanceView
{
  public:
    explicit Header(const std::string& variant = "");

    void setTitle(const std::string& text);
    void setSubtitle(const std::string& text);

    const std::string& getTitle() const { return this->title; }
    const std::string& getSubtitle() const { return this->subtitle; }

    std::string getClassString() const override { return "bklv::Header"; }
    const AttributeTable& attributes() const override;

  protected:
    std::string getLayerText(const std::string& source) override;

  private:
    std::string title;
    std::string subtitle;
};

} // namespace bklv
