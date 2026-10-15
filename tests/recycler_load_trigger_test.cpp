// 无窗口测试：第 4 刀(上) —— 加载触发（首屏要第一页、单飞去抖、到顶停、数据到了能继续）。

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

    // 真实场景里数据是**异步**回来的；测"在飞时不重复发"就必须让它延后通知
    void deliver() const
    {
        if (this->listener)
            this->listener();
    }
    bool autoNotify = true;

    void setLoadListener(std::function<void()> value) override { this->listener = value; }

    int requestCount = 0;

  private:
    size_t loaded = 0;
    size_t maxTotal;
    std::function<void()> listener;
};

} // namespace

int main()
{
    std::printf("== 1. 首屏就会要第一页；单飞不重复 ==\n");
    {
        PagedSource source(30);
        RecyclerFrame list;
        RecyclerLoadConfig config = list.getLoadConfig();
        config.pageSize = 10;
        config.loadThreshold = 2;
        list.setLoadConfig(config);
        list.setDataSource(&source);

        source.autoNotify = false; // 模拟"异步还没回来"
        list.maybeRequestMore();
        checkEqualInt(source.requestCount, 1, "首屏触发一次请求");
        checkEqualInt((int)source.getLoadedCount(), 10, "数据源已增长");
        check(list.isLoadingMore(), "请求在飞");

        list.maybeRequestMore();
        list.maybeRequestMore();
        checkEqualInt(source.requestCount, 1, "**单飞**：在飞期间连调两次也不重复发");

        source.deliver(); // 数据回来了
        check(!list.isLoadingMore(), "回调后解除在飞");
        list.maybeRequestMore();
        checkEqualInt(source.requestCount, 2, "数据到了以后能继续要下一页");
    }

    std::printf("\n== 2. 数据到了以后能继续要，直到到顶 ==\n");
    {
        PagedSource source(25);
        RecyclerFrame list;
        RecyclerLoadConfig config = list.getLoadConfig();
        config.pageSize = 10;
        config.loadThreshold = 2;
        list.setLoadConfig(config);
        list.setDataSource(&source);
        source.autoNotify = true;

        for (int i = 0; i < 10; ++i)
            list.maybeRequestMore();

        checkEqualInt((int)source.getLoadedCount(), 25, "累计拿到 25 条（到顶）");
        checkEqualInt(source.requestCount, 3, "只要了 3 次（10 + 10 + 5）");
        check(!source.hasMore(), "已到顶");
        check(!list.isLoadingMore(), "没有请求在飞");

        list.maybeRequestMore();
        checkEqualInt(source.requestCount, 3, "到顶后再调也不会发请求");
    }

    std::printf("\n== 3. 空数据源（hasMore 默认 false）不触发 ==\n");
    {
        PagedSource source(0); // maxTotal 0 -> 一开始就 hasMore()==false
        RecyclerFrame list;
        list.setDataSource(&source);
        list.maybeRequestMore();
        checkEqualInt(source.requestCount, 0, "没有更多 -> 一次都不发");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
