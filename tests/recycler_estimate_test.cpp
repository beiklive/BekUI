// 无窗口测试：第 4 刀(下·1) —— 未知区域的高度估算（可配置、不虚报、不出现 0）。

#include <cstdio>
#include <string>

#include "bklv/views/recycler.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

class Source : public RecyclerDataSource
{
  public:
    Source(size_t loaded, size_t maxTotal, float height)
        : loaded(loaded), maxTotal(maxTotal), height(height)
    {
    }

    size_t getItemCount() override { return this->loaded; }
    float getItemHeight(size_t) override { return this->height; }
    bool hasMore() override { return this->loaded < this->maxTotal; }

  private:
    size_t loaded;
    size_t maxTotal;
    float height;
};

} // namespace

int main()
{
    std::printf("== 1. 没有更多 -> 估算 == 真实（不虚报）==\n");
    {
        Source source(3, 3, 70.0f);
        RecyclerFrame list;
        list.setDataSource(&source);
        list.notifyDataChanged();

        check(!source.hasMore(), "没有更多");
        checkFloat(list.getTotalHeight(), 210.0f, "真实高度 = 3 × 70");
        checkFloat(list.getEstimatedScrollHeight(), 210.0f, "估算 == 真实");
    }

    std::printf("\n== 2. 配置了 estimatedItemHeight -> 按公式加一段 ==\n");
    {
        Source source(2, 100, 70.0f);
        RecyclerFrame list;
        RecyclerLoadConfig config = list.getLoadConfig();
        config.estimatedItemHeight = 50.0f;
        config.pageSize = 20;
        list.setLoadConfig(config);
        list.setDataSource(&source);
        list.notifyDataChanged();

        check(source.hasMore(), "还有更多");
        checkFloat(list.getTotalHeight(), 140.0f, "真实 = 2 × 70");
        // 140 + 50×20 = 1140
        checkFloat(list.getEstimatedScrollHeight(), 1140.0f, "估算 = 真实 + 估算行高 × 估算行数");
    }

    std::printf("\n== 3. 没配置 -> 用已加载项的平均高；空列表不出现 0 ==\n");
    {
        Source source(4, 100, 60.0f);
        RecyclerFrame list;
        RecyclerLoadConfig config = list.getLoadConfig();
        config.pageSize = 10;
        list.setLoadConfig(config); // estimatedItemHeight 保持 0
        list.setDataSource(&source);
        list.notifyDataChanged();
        // 平均高 60（拿不到每项高度差异，这里每项都 60）-> 240 + 60×10 = 840
        checkFloat(list.getEstimatedScrollHeight(), 840.0f, "用平均高估算");
    }
    {
        Source source(0, 100, 0.0f); // 一条都没有，且数据源不给高度
        RecyclerFrame list;
        list.setDataSource(&source);
        list.notifyDataChanged();
        list.setItemHeight(64.0f);
        // 空 + 还有更多：应当用 itemHeight(64) × 1 行，而不是 0
        checkFloat(list.getEstimatedScrollHeight(), 64.0f, "空列表也有估算（不出现 0 高度）");
    }

    std::printf("\n== 4. 边界：估算高度 0 / 极大都不崩 ==\n");
    {
        Source source(1, 100, 70.0f);
        RecyclerFrame list;
        RecyclerLoadConfig config = list.getLoadConfig();
        config.estimatedItemHeight = 0.0f;
        config.pageSize = 0;
        list.setLoadConfig(config);
        list.setDataSource(&source);
        list.notifyDataChanged();
        check(list.getEstimatedScrollHeight() > list.getTotalHeight(), "配 0 => 回落到平均值，估算仍增长");

        config.estimatedItemHeight = 1.0e9f;
        config.pageSize = 1000000;
        list.setLoadConfig(config);
        check(list.getEstimatedScrollHeight() > 0.0f, "极大值也不崩（不溢出成 0/负）");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
