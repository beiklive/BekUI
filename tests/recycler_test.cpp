// 无窗口测试：回收列表（单段老用法 + 分段新能力）。
//
// 分段对标 borealis 的 numberOfSections/getItemCount(section)/段标题，
// 但接口用 `*InSection` 后缀，避免 C++ 的名字隐藏（borealis 是重载，容易踩）。

#include <cstdio>
#include <string>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/appearance.hpp"
#include "bklv/core/theme_data.hpp"


#include "bklv/views/recycler.hpp"
#include "bklv/core/input.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

// 单段（老写法）：只重写 getItemCount
class FlatSource : public RecyclerDataSource
{
  public:
    explicit FlatSource(size_t count)
        : count(count)
    {
    }

    size_t getItemCount() override { return this->count; }

  private:
    size_t count;
};

// 分段：两段，第二段没有标题（不占表头行）
class SectionedSource : public RecyclerDataSource
{
  public:
    size_t getSectionCount() override { return 2; }

    size_t getItemCountInSection(size_t section) override { return section == 0 ? 3 : 2; }

    std::string getSectionTitle(size_t section) override
    {
        return section == 0 ? "第一段" : "";
    }

    std::string getItemIdentifierInSection(size_t section, size_t) override
    {
        return section == 0 ? "a" : "b";
    }

    float getItemHeightInSection(size_t section, size_t) override
    {
        return section == 0 ? 50.0f : 20.0f;
    }
};

// 记录数据的 cell
class ProbeCell : public RecyclerCell
{
  public:
    size_t seenSection = 999;
    size_t seenIndex   = 999;

    void setSectionIndex(size_t section, size_t index) override
    {
        RecyclerCell::setSectionIndex(section, index);
        this->seenSection = section;
        this->seenIndex   = index;
    }
};

// 记录标题的段头
class ProbeHeader : public RecyclerSectionHeader
{
  public:
    std::string seenTitle;

    void onSectionTitle(size_t, const std::string& title) override { this->seenTitle = title; }
};

RecyclerFrame* makeFrame(RecyclerDataSource* source, float height, float itemHeight)
{
    RecyclerFrame* frame = new RecyclerFrame();
    frame->setWidth(400.0f);
    frame->setHeight(height);
    frame->setItemHeight(itemHeight);
    frame->registerCell("a", [] { return new ProbeCell(); });
    frame->registerCell("b", [] { return new ProbeCell(); });
    frame->registerCell("section_header", [] { return new ProbeHeader(); });
    frame->setDataSource(source);
    YGNodeCalculateLayout(frame->getYGNode(), 400.0f, height, YGDirectionLTR);
    return frame;
}

} // namespace

int main()
{
    bklv::initResources();
    initI18n();
    setLocale("zh-Hans");
    setTheme("default");

    std::printf("== 1. 单段老用法一字不改 ==\n");
    {
        FlatSource source(5);
        RecyclerFrame* frame = makeFrame(&source, 200.0f, 60.0f);

        checkEqualSize(frame->getRowCount(), 5, "5 条 = 5 行（没有表头行）");
        checkFloat(frame->getTotalHeight(), 300.0f, "总高 = 5 x 60");
        check(frame->getRowKind(0) == RecyclerFrame::RowKind::ITEM, "第 0 行是条目");
        checkEqualSize(frame->getRowSection(0), 0, "单段段号恒为 0");
        checkFloat(frame->getRowTop(2), 120.0f, "第 2 行顶部 = 2 x 60");
        checkFloat(frame->getRowHeight(0), 60.0f, "行高用全局 itemHeight");

        delete frame;
    }

    std::printf("\n== 2. 分段展开成行（表头 + 条目）==\n");
    {
        SectionedSource source;
        RecyclerFrame* frame = makeFrame(&source, 200.0f, 60.0f);

        // 第 0 段有标题 → 1 表头 + 3 条；第 1 段没标题 → 2 条
        checkEqualSize(frame->getRowCount(), 6, "1 表头 + 3 条 + 2 条 = 6 行");
        check(frame->getRowKind(0) == RecyclerFrame::RowKind::HEADER, "第 0 行是表头");
        check(frame->getRowKind(1) == RecyclerFrame::RowKind::ITEM, "第 1 行是条目");
        checkEqualSize(frame->getRowSection(1), 0, "第 1 行属于第 0 段");
        checkEqualSize(frame->getRowItemIndex(1), 0, "第 1 行是该段第 0 条");
        checkEqualSize(frame->getRowSection(4), 1, "第 4 行属于第 1 段");
        checkEqualSize(frame->getRowItemIndex(4), 0, "第 4 行是第 1 段的第 0 条");

        const float headerHeight = getTheme().number("metric.list.sectionHeaderHeight", 36.0f);
        checkFloat(frame->getRowHeight(0), headerHeight, "表头行高来自主题");
        checkFloat(frame->getRowTop(1), headerHeight, "表头之后是第一条");
        checkFloat(frame->getTotalHeight(), headerHeight + 3 * 50.0f + 2 * 20.0f,
            "总高 = 表头 + 3x50 + 2x20（每段行高不同）");

        delete frame;
    }

    std::printf("\n== 3. 复用标识与段内下标 ==\n");
    {
        SectionedSource source;
        RecyclerFrame* frame = makeFrame(&source, 1000.0f, 60.0f); // 高一点，让所有行都进视口

        // 回收器是在 update() 里按可视区取数的，先跑几帧
        for (int i = 0; i < 5; i++)
            frame->update();

        int sectionACells = 0, sectionBCells = 0;
        for (const auto& entry : frame->getActiveCellsForTest())
        {
            const size_t row = entry.first;
            if (frame->getRowKind(row) != RecyclerFrame::RowKind::ITEM)
                continue; // 表头行不算条目

            ProbeCell* cell = static_cast<ProbeCell*>(entry.second);
            if (cell == nullptr)
                continue;

            const size_t section = frame->getRowSection(row);
            if (section == 0)
            {
                sectionACells++;
                check(cell->reuseIdentifier == "a", "第 0 段用 a 型 cell");
                checkEqualSize(cell->seenIndex, frame->getRowItemIndex(row), "段内下标对得上");
            }
            else
            {
                sectionBCells++;
                check(cell->reuseIdentifier == "b", "第 1 段用 b 型 cell");
            }
            checkEqualSize(cell->seenSection, section, "cell 知道自己在哪一段");
        }
        checkEqualInt(sectionACells, 3, "第 0 段的 3 条都建了");
        checkEqualInt(sectionBCells, 2, "第 1 段的 2 条都建了");

        // 段头收到了标题
        bool headerTitleSeen = false;
        for (const auto& entry : frame->getActiveCellsForTest())
        {
            if (frame->getRowKind(entry.first) != RecyclerFrame::RowKind::HEADER)
                continue;
            ProbeHeader* header = static_cast<ProbeHeader*>(entry.second);
            check(header->reuseIdentifier == "section_header", "段头用注册的表头 cell");
            if (header->seenTitle == "第一段")
                headerTitleSeen = true;
        }
        check(headerTitleSeen, "表头拿到了段标题");

        delete frame;
    }

    std::printf("\n== 4. 回收仍然有界 ==\n");
    {
        FlatSource source(500);
        RecyclerFrame* frame = makeFrame(&source, 200.0f, 60.0f);
        frame->setPreFetchLine(1);

        for (int i = 0; i < 60; i++)
            frame->update(); // 视口 200/60 ≈ 4 行 + 预取

        check(frame->getVisibleCellCount() <= 8, "可视 cell 数量有界");
        check(frame->getCreatedCellCount() <= 12, "累计新建 cell 数量有界（回收生效）");

        // 滚下去之后仍然有界
        frame->setContentOffsetY(3000.0f, false);
        for (int i = 0; i < 30; i++)
            frame->update();
        check(frame->getCreatedCellCount() <= 12, "滚动后累计新建仍然有界");

        delete frame;
    }

    std::printf("\n== 5. 数据变化后重算 ==\n");
    {
        FlatSource source(3);
        RecyclerFrame* frame = makeFrame(&source, 200.0f, 60.0f);
        checkFloat(frame->getTotalHeight(), 180.0f, "3 x 60");

        frame->setItemHeight(30.0f);
        checkFloat(frame->getTotalHeight(), 90.0f, "改行高后总高重算");

        frame->reloadData();
        checkEqualSize(frame->getRowCount(), 3, "reloadData 后行数不变");

        delete frame;
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
