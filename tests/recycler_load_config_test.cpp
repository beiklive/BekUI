// 无窗口测试：第 1 刀 —— 加载/回收旋钮的默认值与往返 + mock 分页源按接口工作。

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "bklv/views/recycler.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

// 一个分页数据源：每次 requestMore 追加 want 条，直到 maxTotal
class PagedSource : public RecyclerDataSource
{
  public:
    PagedSource(size_t pageSize, size_t maxTotal)
        : pageSize(pageSize), maxTotal(maxTotal), loaded(0)
    {
    }

    size_t getItemCount() override { return this->loaded; }
    size_t getLoadedCount() override { return this->loaded; }
    float getItemHeight(size_t) override { return 72.0f; }
    bool hasMore() override { return this->loaded < this->maxTotal; }

    void requestMore(size_t want, size_t from) override
    {
        this->requests.push_back({ want, from });
        if (want == 0)
            want = this->pageSize;
        this->loaded += want;
        if (this->loaded > this->maxTotal)
            this->loaded = this->maxTotal;
        if (this->listener)
            this->listener();
    }

    void setLoadListener(std::function<void()> listener) override { this->listener = listener; }

    struct Request
    {
        size_t want;
        size_t from;
    };
    std::vector<Request> requests;
    int notifyCount = 0;

  private:
    size_t pageSize;
    size_t maxTotal;
    size_t loaded;
    std::function<void()> listener;
};

} // namespace

int main()
{
    std::printf("== 1. 默认值（上线默认就该是安全的）==\n");
    {
        RecyclerLoadConfig config;
        checkEqualInt((int)config.pageSize, 0, "pageSize 默认 0（由数据源决定）");
        checkEqualInt((int)config.loadThreshold, 3, "loadThreshold 默认 3");
        checkEqualInt((int)config.prefetchLines, 1, "prefetchLines 默认 1");
        checkEqualInt((int)config.maxPoolPerIdentifier, 32, "池上限默认 32");
        checkFloat(config.estimatedItemHeight, 0.0f, "估算高度默认 0（用平均值）");
    }

    std::printf("\n== 2. 配置往返 + 边界值不崩 ==\n");
    {
        RecyclerFrame list;
        RecyclerLoadConfig config = list.getLoadConfig();
        config.pageSize = 20;
        config.loadThreshold = 5;
        config.maxPoolPerIdentifier = 8;
        config.estimatedItemHeight = 64.0f;
        list.setLoadConfig(config);
        checkEqualInt((int)list.getLoadConfig().pageSize, 20, "pageSize 往返");
        checkEqualInt((int)list.getLoadConfig().loadThreshold, 5, "loadThreshold 往返");
        checkEqualInt((int)list.getLoadConfig().maxPoolPerIdentifier, 8, "池上限往返");
        checkFloat(list.getLoadConfig().estimatedItemHeight, 64.0f, "估算高度往返");

        RecyclerLoadConfig zero;
        zero.pageSize = 0;
        zero.loadThreshold = 0;
        zero.maxPoolPerIdentifier = 0;
        list.setLoadConfig(zero);
        checkEqualInt((int)list.getLoadConfig().loadThreshold, 0, "0 值能设（边界不崩）");

        RecyclerLoadConfig huge;
        huge.maxPoolPerIdentifier = (size_t)-1;
        huge.loadThreshold = (size_t)-1;
        list.setLoadConfig(huge);
        check(list.getLoadConfig().maxPoolPerIdentifier == (size_t)-1, "极大值能设（不崩）");
    }

    std::printf("\n== 3. mock 分页源：requestMore 记账 + listener 回调 + 已加载数增长 ==\n");
    {
        PagedSource source(10, 30);
        checkEqualInt((int)source.getLoadedCount(), 0, "初始 0 条");
        check(source.hasMore(), "还有更多");

        int notified = 0;
        source.setLoadListener([&notified] { notified++; });

        source.requestMore(10, 0);
        checkEqualInt((int)source.getLoadedCount(), 10, "第一次拿到 10 条");
        checkEqualInt((int)source.requests.size(), 1, "记了一次请求");
        checkEqualInt((int)source.requests[0].want, 10, "请求 want=10");
        checkEqualInt((int)source.requests[0].from, 0, "请求 from=0");
        checkEqualInt(notified, 1, "回调被调了 1 次");

        source.requestMore(20, 10);
        checkEqualInt((int)source.getLoadedCount(), 30, "追加到 30 条");
        check(!source.hasMore(), "到顶后 hasMore 为假");
        source.requestMore(10, 30);
        checkEqualInt((int)source.getLoadedCount(), 30, "到顶后再要也不会超（夹取）");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
