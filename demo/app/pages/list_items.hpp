#pragma once

// 列表页与调试页共用的列表项与数据源。
//
// 抽出来是因为两个页面都要用：调试页嵌了一个短列表，列表页是主列表。

#include <string>

#include "bklv/bklv.hpp"

namespace demo
{

// 带缩略图的列表项：图片 + 标题 + 副标题
class ListCell : public bklv::RecyclerCell
{
  public:
    ListCell();

    void prepareForReuse() override;
    // 两个索引入口都要走（分段路径走 setSectionIndex，只重写 setIndex 绑定不会生效）
    void setIndex(size_t value) override;
    void setSectionIndex(size_t sectionValue, size_t indexValue) override;

    bool isFocusable() const override { return true; }
    void onFocusGained() override;
    void onFocusLost() override;

  private:
    void applyIndex(size_t section, size_t index);
    bklv::AppearanceView* card = nullptr; // 玻璃卡片（行间距靠它的 margin）
    bklv::Image* thumbnail = nullptr;
    bklv::Label* title     = nullptr;
    bklv::Label* subtitle  = nullptr;
    bool activated         = false;
};

// 网格单元：方形玻璃卡片（上面方图、下面标题）——与列表项共用同一套观感
class GridCell : public bklv::RecyclerCell
{
  public:
    GridCell();

    // 两个索引入口都要走（分段路径走 setSectionIndex）
    void setIndex(size_t value) override;
    void setSectionIndex(size_t sectionValue, size_t indexValue) override;

    bool isFocusable() const override { return true; }
    void onFocusGained() override;
    void onFocusLost() override;

  private:
    void applyIndex(size_t section, size_t index);

    bklv::AppearanceView* card = nullptr;
    bklv::Image* image         = nullptr;
    bklv::Label* caption       = nullptr;
};

// 简单的一行文字列表项
class SimpleCell : public bklv::RecyclerCell
{
  public:
    SimpleCell();

    void setIndex(size_t value) override;

    bool isFocusable() const override { return true; }
    void onFocusGained() override;
    void onFocusLost() override;

  private:
    bklv::Label* label = nullptr;
};

// 固定行高的数据源
class ListSource : public bklv::RecyclerDataSource
{
  public:
    explicit ListSource(size_t count, float height)
        : count(count)
        , height(height)
    {
    }

    size_t getItemCount() override { return this->count; }
    float getItemHeight(size_t) override { return this->height; }

  private:
    size_t count;
    float height;
};

} // namespace demo
