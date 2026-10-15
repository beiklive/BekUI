#include "app/pages/list_items.hpp"

#include <cstdio>

#include "app/ui.hpp"

using namespace bklv;

namespace demo
{

// ---------------------------------------------------------------------------
// ListCell：图片 + 两行文字
// ---------------------------------------------------------------------------

ListCell::ListCell()
{
    // 行间距：cell 本身 64 高，真正的卡片由里面的 glass 视图承担，
    // 用上下 margin 让相邻两行之间留出空隙（列表不再一行贴一行）
    this->setHeight(64.0f);
    this->setAlignItems(AlignItems::CENTER);

    // 与整体观感一致的 **glass** 卡片（主题里的容器玻璃外观，和页头/侧栏同一套）
    AppearanceView* card = new AppearanceView(std::string("base/glass_surface"), false);
    card->setGrow(1.0f);
    card->setAlignItems(AlignItems::CENTER);
    card->setAxis(Axis::ROW); // 卡片是横向一行：缩略图在左、文字在右
    card->setMarginTop(4.0f);
    card->setMarginBottom(4.0f);
    // 左右内缩按 borealis 的 listitem/indent(40) 与 descriptionIndent(16)
    card->setPaddingTop(8.0f);
    card->setPaddingBottom(8.0f);
    card->setPaddingLeft(40.0f);
    card->setPaddingRight(16.0f);
    card->setGap(12.0f);
    this->addView(card);
    this->card = card;

    this->thumbnail = new Image();
    this->thumbnail->setDimensions(76.0f, 44.0f);
    this->thumbnail->setCornerRadius(6.0f);
    this->thumbnail->setScalingType(ImageScalingType::FIT); // 保持比例完整显示（FILL 会把方形图上下裁掉）
    card->addView(this->thumbnail);

    Box* textBox = new Box(Axis::COLUMN);
    textBox->setGrow(1.0f);
    textBox->setShrink(1.0f);

    this->title = makeText("", 15.0f, rgb(235, 238, 242), true);
    textBox->addView(this->title);

    this->subtitle = makeText("", 12.0f, rgb(150, 160, 175), true);
    this->subtitle->setMarginTop(3.0f);
    textBox->addView(this->subtitle);

    card->addView(textBox);
}

void ListCell::prepareForReuse()
{
    // 复用时必须清掉上一条数据的痕迹
    this->activated = false;
}

void ListCell::setIndex(size_t value)
{
    this->applyIndex(0, value); // 单段列表：section 恒为 0
}

void ListCell::setSectionIndex(size_t sectionValue, size_t indexValue)
{
    // 回收器的**分段路径**走这里：不重写它的话，绑定永远不生效（踩过两次）
    RecyclerCell::setSectionIndex(sectionValue, indexValue);
    this->applyIndex(sectionValue, indexValue);
}

void ListCell::applyIndex(size_t section, size_t index)
{
    (void)section;
    RecyclerCell::setIndex(index);

    // 缩略图用 borealis 的真实素材（8 张精灵图循环）——同一张图会被多行复用，
    // 走的正是纹理仓库的路径缓存（`core/graphics.cpp` 的 g_pathCache）
    static const char* const kThumbs[8] = {
        "resources/img/demo/pokemon_001.png", "resources/img/demo/pokemon_004.png",
        "resources/img/demo/pokemon_007.png", "resources/img/demo/pokemon_011.png",
        "resources/img/demo/pokemon_014.png", "resources/img/demo/pokemon_017.png",
        "resources/img/demo/pokemon_021.png", "resources/img/demo/pokemon_024.png",
    };
    const size_t pick = index % 8;
    this->thumbnail->setImageFromFile(kThumbs[pick]);

    char title[16] = { 0 };
    std::snprintf(title, sizeof(title), "No.%03d", (int)pick * 3 + 1); // 001/004/007/...
    this->title->setText(title);
    this->subtitle->setText(this->activated ? "已触发（A）"
                                            : "图片素材来自 borealis · 行高 64 · 滚出视口即回收复用");
}

void ListCell::onFocusGained()
{
    // 玻璃卡片上不加纯色底（会盖住玻璃质感），只用描边表达聚焦
    if (this->card != nullptr)
    {
        this->card->setBorderColor(rgb(90, 170, 245));
        this->card->setBorderThickness(2.0f);
    }
}

void ListCell::onFocusLost()
{
    if (this->card != nullptr)
        this->card->setBorderThickness(0.0f);
}

// ---------------------------------------------------------------------------
// SimpleCell：单行
// ---------------------------------------------------------------------------

SimpleCell::SimpleCell()
{
    this->setAlignItems(AlignItems::CENTER);
    this->setPadding(0.0f, 12.0f, 0.0f, 12.0f);
    this->setBackgroundColor(rgb(38, 42, 50));
    this->setCornerRadius(6.0f);

    this->label = makeText("", 14.0f, rgb(200, 208, 220), true);
    this->label->setGrow(1.0f);
    this->addView(this->label);
}

void SimpleCell::setIndex(size_t value)
{
    RecyclerCell::setIndex(value);
    this->label->setText("内嵌行 #" + std::to_string(value));
}

void SimpleCell::onFocusGained()
{
    this->setBackgroundColor(rgb(52, 74, 110));
    this->setBorderColor(rgb(90, 170, 245));
    this->setBorderThickness(2.0f);
}

void SimpleCell::onFocusLost()
{
    this->setBackgroundColor(rgb(38, 42, 50));
    this->setBorderThickness(0.0f);
}

} // namespace demo
