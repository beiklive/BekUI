// 无窗口测试：多列时内容高度按视觉行累加（滚动范围不能偏大）。

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
    Source(size_t count, float height)
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

} // namespace

int main()
{
    std::printf("== 1. 单列：内容高度 = 条目数 x 行高（行为不变）==\n");
    {
        Source source(5, 100.0f);
        RecyclerFrame list;
        list.setDataSource(&source);
        list.notifyDataChanged();
        checkFloat(list.getTotalHeight(), 500.0f, "单列 5x100 = 500");
    }

    std::printf("\n== 2. 三列：内容高度 = 视觉行数 x 行高（不是条目数）==\n");
    {
        Source source(8, 100.0f);
        RecyclerFrame list;
        list.setColumns(3);
        list.setDataSource(&source);
        list.notifyDataChanged();
        // 8 条目 / 3 列 = 3 个视觉行（3+3+2）-> 300，而不是 800
        checkFloat(list.getTotalHeight(), 300.0f, "三列 8 条目 = 3 行 x 100 = 300");
        checkFloat(list.visualTopOf(3), 100.0f, "第 2 行从 100 开始");
        checkFloat(list.visualTopOf(6), 200.0f, "第 3 行从 200 开始");
    }

    std::printf("\n== 3. 切换到多列/回到单列，高度都跟着更新 ==\n");
    {
        Source source(8, 100.0f);
        RecyclerFrame list;
        list.setDataSource(&source);
        list.notifyDataChanged();
        checkFloat(list.getTotalHeight(), 800.0f, "先单列 800");

        list.setColumns(2);
        list.setDataSource(&source);
        list.notifyDataChanged();
        checkFloat(list.getTotalHeight(), 400.0f, "改两列 -> 4 行 x 100 = 400");

        list.setColumns(1);
        list.setDataSource(&source);
        list.notifyDataChanged();
        checkFloat(list.getTotalHeight(), 800.0f, "回单列 -> 800");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
