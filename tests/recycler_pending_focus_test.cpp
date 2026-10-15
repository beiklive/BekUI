// 无窗口测试：第 4 刀(下·2) —— 焦点落在未加载行：记下 -> 触发加载 -> 数据到了可消费。

#include <cstdio>
#include <functional>
#include <string>

#include "bklv/views/recycler.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

class PagedSource : public RecyclerDataSource
{
  public:
    PagedSource(size_t maxTotal)
        : maxTotal(maxTotal)
    {
    }

    size_t getItemCount() override { return this->loaded; }
    size_t getLoadedCount() override { return this->loaded; }
    float getItemHeight(size_t) override { return 72.0f; }
    bool hasMore() override { return this->loaded < this->maxTotal; }

    void requestMore(size_t want, size_t from) override
    {
        (void)from;
        ++this->requestCount;
        if (want == 0)
            want = 10;
        this->loaded += want;
        if (this->loaded > this->maxTotal)
            this->loaded = this->maxTotal;
        if (this->autoNotify && this->listener)
            this->listener();
    }

    void setLoadListener(std::function<void()> value) override { this->listener = value; }
    void deliver() const
    {
        if (this->listener)
            this->listener();
    }

    int requestCount = 0;
    bool autoNotify = false;

  private:
    size_t loaded = 0;
    size_t maxTotal;
    std::function<void()> listener;
};

} // namespace

int main()
{
    std::printf("== 1. 行已经在表里 -> 不需要等数据 ==\n");
    {
        PagedSource source(100);
        RecyclerFrame list;
        list.setDataSource(&source);
        source.requestMore(5, 0); // 先有 5 行
        list.notifyDataChanged();

        list.requestFocusIndex(3);
        check(!list.hasPendingFocus(), "已加载的行不记 pending");
        checkEqualInt((int)list.takePendingFocusIndex(), -1, "没有待办可取");
    }

    std::printf("\n== 2. 未加载的行 -> 记下 + 触发一次加载；数据到了才能取走 ==\n");
    {
        PagedSource source(100);
        RecyclerFrame list;
        RecyclerLoadConfig config = list.getLoadConfig();
        config.pageSize = 10;
        list.setLoadConfig(config);
        list.setDataSource(&source);
        source.requestMore(5, 0);
        list.notifyDataChanged();

        const int before = source.requestCount;
        list.requestFocusIndex(25);
        check(list.hasPendingFocus(), "记下 pending");
        checkEqualInt(source.requestCount, before + 1, "顺带触发了一次加载");

        checkEqualInt((int)list.takePendingFocusIndex(), -1, "数据还没到 -> 取不到（继续等）");
        check(list.hasPendingFocus(), "仍在等");

        // 关键：先让在飞的那次完成（单飞期间后面的请求本来就不该发 —— 这正是库的行为）
        source.autoNotify = true;
        source.deliver();
        check(!list.isLoadingMore(), "在飞的那次完成后解除");

        for (int i = 0; i < 20 && source.getLoadedCount() < 30; ++i)
            list.maybeRequestMore();

        check(source.getLoadedCount() >= 25, "已加载到 25 行以上");
        checkEqualInt((int)list.takePendingFocusIndex(), 25, "取到目标行号");
        check(!list.hasPendingFocus(), "取走后清空");
        checkEqualInt((int)list.takePendingFocusIndex(), -1, "只能取一次");
    }

    std::printf("\n== 3. 边界：极大行号不崩 ==\n");
    {
        PagedSource source(100);
        RecyclerFrame list;
        list.setDataSource(&source);
        list.requestFocusIndex((size_t)-2);
        check(list.hasPendingFocus(), "极大行号被记下（不崩）");
        checkEqualInt((int)list.takePendingFocusIndex(), -1, "取不到（行不存在）");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
