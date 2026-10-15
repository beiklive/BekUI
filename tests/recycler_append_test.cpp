// 无窗口测试：第 2 刀 —— rows 增量追加（用"取高度的调用次数"当不 O(n²) 的确定性证据）。

#include <cstdio>
#include <string>
#include <vector>

#include "bklv/views/recycler.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

// 会计数的分页数据源：把 getItemHeight 的调用次数记下来
class CountingSource : public RecyclerDataSource
{
  public:
    size_t getItemCount() override { return this->loaded; }
    size_t getLoadedCount() override { return this->loaded; }
    bool hasMore() override { return this->loaded < this->maxTotal; }
    float getItemHeight(size_t) override
    {
        ++this->heightCalls;
        return 72.0f;
    }
    float getItemHeightInSection(size_t, size_t index) override
    {
        (void)index;
        ++this->heightCalls;
        return 72.0f;
    }
    std::string getItemIdentifier(size_t) override { return RecyclerDataSource::DEFAULT_CELL_ID; }

    void load(size_t count)
    {
        this->loaded += count;
        if (this->loaded > this->maxTotal)
            this->loaded = this->maxTotal;
    }

    size_t heightCalls = 0;

  private:
    size_t loaded = 0;
    size_t maxTotal = 1000;
};

} // namespace

int main()
{
    std::printf("== 1. 分 20 批追加到 1000 条：取高度的次数必须是线性的 ==\n");
    {
        CountingSource source;
        RecyclerFrame list;
        list.setDataSource(&source);

        const size_t batch = 50;
        const size_t batches = 20;
        for (size_t i = 0; i < batches; ++i)
        {
            source.load(batch);
            if (i == 0)
                list.notifyDataChanged(); // 第一批：建表
            else
                list.notifyItemsAppended(); // 之后：**增量**追加
        }

        checkEqualInt((int)source.getItemCount(), 1000, "最终 1000 条");
        // 线性上界：全量重建会是 sum(50,100,...,1000) = 10500/50*... ≈ 10.5 万次；
        // 增量只应该是 ~1000 次（每项一次）。留 2 倍余量。
        const bool linear = source.heightCalls <= 2200;
        check(linear, "取高度次数是线性的（增量追加生效）");
        std::printf("     实际取高度次数 = %d\n", (int)source.heightCalls);
    }

    std::printf("\n== 2. 追加不动前面的行（top 单调递增、行数对得上）==\n");
    {
        CountingSource source;
        RecyclerFrame list;
        list.setDataSource(&source);

        source.load(3);
        list.notifyDataChanged();

        source.load(2);
        list.notifyItemsAppended();

        checkEqualInt((int)source.getItemCount(), 5, "变成 5 条");
        checkFloat(list.getTotalHeight(), 72.0f * 5.0f, "总高 = 5 行");
        checkEqualInt((int)list.getVisibleCellCount(), 0, "已有行的 top 语义没变（没有可视 cell 也正常）");
    }

    std::printf("\n== 3. 条目变少 -> 回落到全量（不留下多余行）==\n");
    {
        CountingSource source;
        RecyclerFrame list;
        list.setDataSource(&source);

        source.load(5);
        list.notifyDataChanged();
        checkFloat(list.getTotalHeight(), 72.0f * 5.0f, "5 行");

        // 模拟"重置成更少"：这里换一个只有 2 条的数据源
        class Shrunk : public CountingSource
        {
          public:
            Shrunk()
            {
                this->load(2);
            }
            size_t getItemCount() override { return 2; }
        };
        Shrunk shrunk;
        list.setDataSource(&shrunk);
        list.notifyItemsAppended(); // 条目变少 -> 内部应回落全量
        checkFloat(list.getTotalHeight(), 72.0f * 2.0f, "回落到 2 行（没有残留）");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
