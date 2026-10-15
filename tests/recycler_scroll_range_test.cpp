// 无窗口测试：第 4 刀(下·3) —— 估算进滚动范围。
//
// 注意：`getScrollRange()` 需要**布局之后**的尺寸（无窗口下帧高为 0，它恒为 0），
// 所以这里断言的是无窗口下能观测的部分：估算**不会**污染真实布局高度（安全性），
// 以及估算自身的数值行为；滚动条长度的视觉验证放在 5b 的 demo 截图里做。

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

    void load(size_t count)
    {
        this->loaded += count;
        if (this->loaded > this->maxTotal)
            this->loaded = this->maxTotal;
    }

  private:
    size_t loaded;
    size_t maxTotal;
    float height;
};

} // namespace

int main()
{
    std::printf("== 1. 估算绝不污染真实布局高度 ==\n");
    {
        Source source(3, 100, 70.0f);
        RecyclerFrame list;
        RecyclerLoadConfig config = list.getLoadConfig();
        config.estimatedItemHeight = 50.0f;
        config.pageSize = 20;
        list.setLoadConfig(config);
        list.setDataSource(&source);
        list.notifyDataChanged();
        list.update();

        checkFloat(list.getTotalHeight(), 210.0f, "真实高度仍是 3×70（估算没混进来）");
        checkFloat(list.getEstimatedScrollHeight(), 1210.0f, "估算高度 = 真实 + 50×20");
    }

    std::printf("\n== 2. 没有更多 -> 估算 == 真实；追加后真实变大、估算仍叠加 ==\n");
    {
        Source done(3, 3, 70.0f);
        RecyclerFrame list;
        list.setDataSource(&done);
        list.notifyDataChanged();
        checkFloat(list.getEstimatedScrollHeight(), 210.0f, "没有更多 -> 不虚报");
    }
    {
        Source source(3, 100, 70.0f);
        RecyclerFrame list;
        RecyclerLoadConfig config = list.getLoadConfig();
        config.estimatedItemHeight = 50.0f;
        config.pageSize = 20;
        list.setLoadConfig(config);
        list.setDataSource(&source);
        list.notifyDataChanged();

        source.load(10);
        list.notifyItemsAppended();
        checkFloat(list.getTotalHeight(), 13.0f * 70.0f, "真实高度 13×70");
        checkFloat(list.getEstimatedScrollHeight(), 13.0f * 70.0f + 50.0f * 20.0f, "估算叠加在后");
    }

    std::printf("\n== 3. 配置变化会反映到估算上（上线可调）==\n");
    {
        Source source(2, 100, 70.0f);
        RecyclerFrame list;
        RecyclerLoadConfig config = list.getLoadConfig();
        config.estimatedItemHeight = 50.0f;
        config.pageSize = 20;
        list.setLoadConfig(config);
        list.setDataSource(&source);
        list.notifyDataChanged();
        const float before = list.getEstimatedScrollHeight();

        config.pageSize = 40;
        list.setLoadConfig(config);
        check(list.getEstimatedScrollHeight() > before, "pageSize 变大 -> 估算段更长");

        config.estimatedItemHeight = 0.0f; // 回落到平均值(70)
        list.setLoadConfig(config);
        checkFloat(list.getEstimatedScrollHeight(), 140.0f + 70.0f * 40.0f, "估算高度 0 -> 用平均值");
    }

    std::printf("\n== 4. 边界：无窗口时滚动范围不为负 ==\n");
    {
        Source source(0, 100, 70.0f);
        RecyclerFrame list;
        list.setDataSource(&source);
        list.notifyDataChanged();
        list.update();
        check(list.getScrollRange() >= 0.0f, "滚动范围不出现负数");
        check(list.getEstimatedScrollHeight() >= 0.0f, "估算高度不为负");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
