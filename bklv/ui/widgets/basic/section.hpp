#pragma once

// 一节的小标题：左侧主信息、右侧副信息，最左边一条竖条（强调色）。
//
// 外观在 `resources/theme/default/base/section/section.json`，三个层都按 `data_id`
// 绑定内容槽：
//   `bar`   竖条（rect 层，纯装饰，颜色/宽度/圆角都在 JSON 里）
//   `title` 左侧主信息（text 层，`Section::TITLE_SLOT`）
//   `info`  右侧副信息（text 层，`Section::INFO_SLOT`，可空）
//
// 它不可聚焦（只是视觉分组标题），所以不会抢焦点、也不会出现在按键提示里。

#include <string>

#include "bklv/ui/widgets/appearance/appearance_view.hpp"

namespace bklv
{

class Section : public AppearanceView
{
  public:
    // 内容槽 id（与外观 JSON 里的 "data_id" 逐字一致）
    static constexpr const char* TITLE_SLOT = "title";
    static constexpr const char* INFO_SLOT  = "info";

    explicit Section(const std::string& title = "", const std::string& info = "");
    ~Section() override;

    // 左侧主信息
    void setTitle(const std::string& title);
    const std::string& getTitle() const { return this->title; }

    // 右侧副信息；空串时那一层不画字
    void setInfo(const std::string& info);
    const std::string& getInfo() const { return this->info; }

    void applyTheme() override;

    std::string getClassString() const override { return "bklv::Section"; }

  private:
    void refreshContent();

    std::string title;
    std::string info;
};

} // namespace bklv
