#pragma once

// Demo 07 的场景构建，抽出来供 demo_07_test 无窗口验证布局。

#include <string>

#include "bklv/core/box.hpp"
#include "bklv/core/graphics.hpp"
#include "bklv/views/h_scrolling_frame.hpp"
#include "bklv/views/image.hpp"
#include "bklv/views/label.hpp"
#include "bklv/views/progress_spinner.hpp"
#include "bklv/views/recycler.hpp"

using namespace bklv;


// 图片资源路径（相对运行目录）
static const char* CARD_IMAGE = "resources/testcard.png";

// ---------------------------------------------------------------------------
// 列表项：缩略图 + 标题 + 副标题，可聚焦
// ---------------------------------------------------------------------------

class VideoCardCell : public RecyclerCell
{
  public:
    VideoCardCell()
    {
        this->setBackgroundColor(rgb(38, 42, 50));
        this->setCornerRadius(8.0f);
        this->setAlignItems(AlignItems::CENTER);
        this->setGap(12.0f);
        this->setPadding(10.0f);

        // 缩略图
        this->thumbnail = new Image();
        this->thumbnail->setDimensions(96.0f, 60.0f);
        this->thumbnail->setCornerRadius(6.0f);
        this->thumbnail->setScalingType(ImageScalingType::FILL);
        this->thumbnail->setImageFromFile(CARD_IMAGE);
        this->addView(this->thumbnail);

        // 右侧文字
        Box* textBox = new Box(Axis::COLUMN);
        textBox->setGrow(1.0f);
        textBox->setShrink(1.0f);

        this->title = new Label();
        this->title->setFontSize(17.0f);
        this->title->setSingleLine(true);
        this->title->setTextColor(rgb(235, 238, 242));
        textBox->addView(this->title);

        this->subtitle = new Label();
        this->subtitle->setFontSize(13.0f);
        this->subtitle->setSingleLine(true);
        this->subtitle->setMarginTop(4.0f);
        this->subtitle->setTextColor(rgb(150, 160, 175));
        textBox->addView(this->subtitle);

        this->addView(textBox);

        // 焦点高亮：注册一个 A 键动作，顺便让提示栏有内容
        this->registerAction(ControllerButton::A, "播放", [](View* view) {
            auto* cell = dynamic_cast<VideoCardCell*>(view);
            if (cell != nullptr)
                cell->activated = true;
        });
    }

    void prepareForReuse() override
    {
        // 复用前必须清掉上一条数据的痕迹
        this->activated = false;
    }

    void setIndex(size_t value) override
    {
        RecyclerCell::setIndex(value);
        this->title->setText("视频标题 #" + std::to_string(value));
        this->subtitle->setText(this->activated ? "已按下确认键" : "UP 主 · 12.3 万播放 · 04:2" + std::to_string(value % 10));
        this->setBackgroundColor(this->activated ? rgb(56, 92, 140) : rgb(38, 42, 50));
    }

    void onFocusGained() override
    {
        this->setBackgroundColor(rgb(52, 74, 110));
        this->setBorderColor(rgb(90, 170, 245));
        this->setBorderThickness(2.0f);
    }

    void onFocusLost() override
    {
        this->setBackgroundColor(rgb(38, 42, 50));
        this->setBorderThickness(0.0f);
    }

    bool isFocusable() const override { return true; }

  private:
    Image* thumbnail  = nullptr;
    Label* title      = nullptr;
    Label* subtitle   = nullptr;
    bool activated    = false;
};

class VideoDataSource : public RecyclerDataSource
{
  public:
    explicit VideoDataSource(size_t count)
        : count(count)
    {
    }

    size_t getItemCount() override { return this->count; }
    float getItemHeight(size_t) override { return 80.0f; }

  private:
    size_t count;
};

// ---------------------------------------------------------------------------
// 顶部分类条（横向滚动）
// ---------------------------------------------------------------------------

class CategoryChip : public Box
{
  public:
    CategoryChip(const std::string& text, bool active)
        : Box(Axis::ROW)
    {
        this->setPadding(10.0f, 18.0f, 10.0f, 18.0f);
        this->setCornerRadius(18.0f);
        this->setBackgroundColor(active ? rgb(74, 144, 226) : rgb(45, 49, 56));
        this->setAlignItems(AlignItems::CENTER);

        Label* label = new Label();
        label->setText(text);
        label->setFontSize(15.0f);
        label->setSingleLine(true);
        label->setTextColor(active ? rgb(255, 255, 255) : rgb(190, 198, 210));
        this->addView(label);
    }

    bool isFocusable() const override { return true; }
};

// ---------------------------------------------------------------------------
// 场景
// ---------------------------------------------------------------------------

static View* buildScene()
{
    Box* root = new Box(Axis::COLUMN);
    root->setWidthPercentage(100.0f);
    root->setHeightPercentage(100.0f);
    root->setBackgroundColor(rgb(26, 28, 33));
    root->setPadding(20.0f);
    root->setGap(14.0f);

    // ---- 标题行 ----
    Box* header = new Box(Axis::ROW);
    header->setWidthPercentage(100.0f);
    header->setAlignItems(AlignItems::CENTER);
    header->setJustifyContent(JustifyContent::SPACE_BETWEEN);

    Label* title = new Label();
    title->setText("bklv：虚拟列表 + 滚动 + 图片 + 焦点导航");
    title->setFontSize(24.0f);
    title->setTextColor(rgb(255, 255, 255));
    header->addView(title);

    ProgressSpinner* spinner = new ProgressSpinner();
    spinner->setDimensions(26.0f, 26.0f);
    header->addView(spinner);

    root->addView(header);

    // ---- 顶部分类条（横向滚动） ----
    HScrollingFrame* categories = new HScrollingFrame();
    categories->setWidthPercentage(100.0f);
    categories->setHeight(48.0f);
    categories->setShowScrollingIndicator(false);

    Box* categoryContent = new Box(Axis::ROW);
    categoryContent->setHeight(48.0f);
    categoryContent->setGap(10.0f);

    const char* names[] = {"推荐", "热门", "追番", "影视", "直播", "动态", "音乐", "舞蹈", "游戏", "科技", "生活"};
    for (int i = 0; i < 11; ++i)
    {
        CategoryChip* chip = new CategoryChip(names[i], i == 0);
        chip->setHeight(38.0f);
        categoryContent->addView(chip);
    }
    // 内容宽度 = 各 chip 宽度 + 间距；这里给个足够大的值让横向滚动有空间
    categoryContent->setWidth(11 * 96.0f + 10 * 10.0f);
    categories->setContentView(categoryContent);
    root->addView(categories);

    // ---- 说明文字 ----
    Label* hint = new Label();
    hint->setText("方向键 / WASD 切换焦点，回车或鼠标点击触发；鼠标滚轮或拖拽滚动列表；F1 显示调试层。");
    hint->setFontSize(14.0f);
    hint->setTextColor(rgb(140, 150, 165));
    root->addView(hint);

    // ---- 主列表（虚拟滚动） ----
    RecyclerFrame* list = new RecyclerFrame();
    list->setWidthPercentage(100.0f);
    list->setGrow(1.0f);
    list->setItemHeight(80.0f);
    list->setPreFetchLine(1);
    list->setShowScrollingIndicator(true);
    list->registerCell(RecyclerDataSource::DEFAULT_CELL_ID, [] { return new VideoCardCell(); });

    // 数据源挂在堆上，由 list 的生命周期内一直有效
    static VideoDataSource source(1000);
    list->setDataSource(&source);

    root->addView(list);

    // 根节点自己也注册一个动作，让提示栏在没有焦点时也有内容
    root->registerAction(ControllerButton::B, "退出", [](View*) { }, true);

    return root;
}

