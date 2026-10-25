#include "list_tab.hpp"

#include <cstddef>
#include <functional>
#include <string>

#include "bklv/system/media/icon_font.hpp"
#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/appearance/appearance_view.hpp"
#include "bklv/ui/widgets/basic/label.hpp"
#include "bklv/ui/widgets/container/h_scrolling_frame.hpp"
#include "bklv/ui/widgets/container/scroll_box.hpp"
#include "bklv/ui/widgets/list/recycler.hpp"
#include "demo/demo_app.hpp"
#include "demo/layout_kit.hpp"
#include "demo/style.hpp"

namespace demo
{

namespace
{

// 一个"有 count 条"的最简数据源。列表只关心条数与复用标识，
// 数据本身在 cell 里按 index 现算（示例够用）。
class CountSource : public bklv::RecyclerDataSource
{
  public:
    explicit CountSource(size_t count) : count(count) { }

    size_t getItemCount() override { return this->count; }

  private:
    size_t count = 0;
};

// 长列表的一行：整块是 AppearanceView（外观 demo/list_row），
// cell 本身只负责"摆位置 + 承接点击"。
class ListRowCell : public bklv::RecyclerCell
{
  public:
    ListRowCell() : bklv::RecyclerCell(true)
    {
        this->reuseIdentifier = "row";
        this->onPointerAction = true;
        this->registerAction(bklv::ControllerButton::A, "hints/ok", [this](bklv::View*) { this->invoke(); });

        auto* card = new bklv::AppearanceView("demo/list_row", false);
        card->setPositionType(bklv::PositionType::ABSOLUTE);
        card->setPositionTop(0.0f);
        card->setPositionRight(0.0f);
        card->setPositionBottom(0.0f);
        card->setPositionLeft(0.0f);
        card->enableContentMeasurement();
        this->card = card;
        this->addView(card);
    }

    // RecyclerFrame 绑段内条目时调的是 setSectionIndex（setIndex 只给段头用）
    void setSectionIndex(size_t section, size_t value) override
    {
        bklv::RecyclerCell::setSectionIndex(section, value);

        if (this->card == nullptr)
            return;

        this->card->setText("title", "第 " + std::to_string(value + 1) + " 条");
        this->card->setText("subtitle",
            (value % 5 == 0) ? "点这一条 → 新页面（内容层下钻，返回时滚动位置还在）"
                             : "滚动时这一行的 cell 会被复用，不会越建越多");

        // 复用池里的 cell 是"先 setIndex、后进树"的：换完内容要让它重排一次，
        // 否则文字层还按上一次（空串）量出来的宽度画。
    }

    // 点/按确认时回调（带上当前 index）
    std::function<void(size_t)> onActivate;

  private:
    void invoke()
    {
        if (this->onActivate)
            this->onActivate(this->getIndex());
    }

    bklv::AppearanceView* card = nullptr;
};

// 网格里的一格：图标 + 标题。
class GridTileCell : public bklv::RecyclerCell
{
  public:
    GridTileCell() : bklv::RecyclerCell(true)
    {
        this->reuseIdentifier = "tile";
        this->onPointerAction = true;
        this->registerAction(bklv::ControllerButton::A, "hints/ok", [this](bklv::View*) { this->invoke(); });

        auto* card = new bklv::AppearanceView("demo/grid_tile", false);
        card->setPositionType(bklv::PositionType::ABSOLUTE);
        card->setPositionTop(0.0f);
        card->setPositionRight(0.0f);
        card->setPositionBottom(0.0f);
        card->setPositionLeft(0.0f);
        card->enableContentMeasurement();
        this->card = card;
        this->addView(card);
    }

    // RecyclerFrame 绑段内条目时调的是 setSectionIndex（setIndex 只给段头用）
    void setSectionIndex(size_t section, size_t value) override
    {
        bklv::RecyclerCell::setSectionIndex(section, value);

        if (this->card == nullptr)
            return;

        const bklv::icon::Id icons[] = { bklv::icon::Id::STAR, bklv::icon::Id::SAVE, bklv::icon::Id::SETTINGS };
        this->card->setIcon("icon", bklv::icon::glyph(icons[value % 3]));
        this->card->setText("title", "格 " + std::to_string(value + 1));
    }

    std::function<void(size_t)> onActivate;

  private:
    void invoke()
    {
        if (this->onActivate)
            this->onActivate(this->getIndex());
    }

    bklv::AppearanceView* card = nullptr;
};

// 横向滚动里的一张卡。
bklv::AppearanceView* makeWideCard(const std::string& title, bklv::icon::Id icon)
{
    auto* card = new bklv::AppearanceView("demo/card_wide", false);
    card->setIcon("icon", bklv::icon::glyph(icon));
    card->setText("title", title);
    card->enableContentMeasurement();
    return card;
}

} // namespace

bklv::View* buildListTab(DemoApp& app)
{
    auto* scroll = new bklv::ScrollBox(bklv::Axis::COLUMN);
    scroll->setGrow(1.0f);

    auto* content = scroll->getContentBox();
    content->setGap(26.0f);
    content->setPadding(24.0f);

    // 1) 虚拟化长列表：2000 条，点一条 → 内容层下钻一层新页面
    {
        auto* recycler = new bklv::RecyclerFrame();
        recycler->setGrow(1.0f);
        recycler->setItemHeight(72.0f);
        recycler->setPreFetchLine(2);
        recycler->registerCell("row", [&app] {
            auto* cell  = new ListRowCell();
            cell->onActivate = [&app](size_t index) { app.openListDetail(index); };
            return cell;
        });
        recycler->setDataSource(app.getLongListSource());

        auto* holder = new bklv::Box(bklv::Axis::COLUMN);
        holder->setHeight(420.0f);
        holder->addView(recycler);

        content->addView(makeSection("虚拟化长列表", holder, "2000 条 · 行高 72 · 只建视口内的 cell"));
    }

    // 2) 虚拟化网格：3 列 400 格
    {
        auto* grid = new bklv::RecyclerFrame();
        grid->setColumns(3);
        grid->setGrow(1.0f);
        grid->setItemHeight(96.0f);
        grid->setPreFetchLine(1);
        grid->registerCell("tile", [&app] {
            auto* cell       = new GridTileCell();
            cell->onActivate = [&app](size_t index) { app.openGridDetail(index); };
            return cell;
        });
        grid->setDataSource(app.getGridSource());

        auto* holder = new bklv::Box(bklv::Axis::COLUMN);
        holder->setHeight(380.0f);
        holder->addView(grid);

        content->addView(makeSection("虚拟化网格（3 列）", holder, "400 格 · 每行 3 个 · 同一套复用池"));
    }

    // 3) 横向滚动：HScrollingFrame + 一行卡片
    {
        auto* hscroll = new bklv::HScrollingFrame();
        hscroll->setHeight(150.0f);

        auto* strip = new bklv::Box(bklv::Axis::ROW);
        strip->setGap(12.0f);
        strip->setPadding(4.0f);

        const bklv::icon::Id icons[] = { bklv::icon::Id::STAR,   bklv::icon::Id::SAVE,    bklv::icon::Id::SETTINGS,
                                         bklv::icon::Id::SEARCH, bklv::icon::Id::INFO,    bklv::icon::Id::WARNING };
        for (int i = 0; i < 12; ++i)
            strip->addView(makeWideCard("卡片 " + std::to_string(i + 1), icons[i % 6]));

        hscroll->setContentView(strip);
        content->addView(makeSection("横向滚动", hscroll, "HScrollingFrame · 12 张卡 · 带惯性"));
    }

    // 4) 说明：点击行为与返回
    content->addView(makeSection("点条目打开新页面",
        makeNote("点长列表里的任意一行：会往内容层压一层详情页（Shell::pushContentView），"
                 "入场是滑动淡入、返回时上一层恢复原来的滚动位置与焦点。"
                 "按 B / Esc 由 Shell 的 setOnBackRequested 处理，也可以点详情页里的按钮。"),
        "ContentStack"));

    return scroll;
}

} // namespace demo
