// 无窗口测试：数据源注册表（纯数据层）。这是"JSON 只写数据源名字"的第一块砖。

#include <cstdio>
#include <memory>
#include <string>

#include "bklv/views/data_source.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

// 一个最小的数据源：3 项、第 1 项高 20、其余 10
class MockSource : public RecyclerDataSource
{
  public:
    size_t getItemCount() override { return 3; }
    float getItemHeight(size_t index) override { return index == 0 ? 20.0f : 10.0f; }
    std::string getItemIdentifier(size_t index) override
    {
        return index == 0 ? "first" : RecyclerDataSource::DEFAULT_CELL_ID;
    }
};

} // namespace

int main()
{
    std::printf("== 1. 注册与创建 ==\n");
    {
        data::clearSourcesForTest();
        std::string error;
        check(data::registerSource("mock", [] { return (RecyclerDataSource*)new MockSource(); }, error),
            "注册成功");
        check(data::hasSource("mock"), "hasSource 为真");
        check(data::sourceNames().size() == 1, "名字表里有 1 个");

        std::unique_ptr<RecyclerDataSource> source(data::createSource("mock"));
        check(source != nullptr, "能建出实例");
        if (source != nullptr)
        {
            checkEqualInt((int)source->getItemCount(), 3, "总数");
            checkFloat(source->getItemHeight(0), 20.0f, "第 1 项不等高（20）");
            checkFloat(source->getItemHeight(1), 10.0f, "第 2 项（10）");
            check(source->getItemIdentifier(0) == "first", "复用身份");
        }
    }

    std::printf("\n== 2. 没注册过要报出来、不静默 ==\n");
    {
        check(data::createSource("不存在") == nullptr, "没注册过 -> nullptr");
        check(!data::hasSource("不存在"), "hasSource 为假");
    }

    std::printf("\n== 3. 重名/空名/空工厂都要失败 ==\n");
    {
        std::string error;
        check(!data::registerSource("mock", [] { return (RecyclerDataSource*)new MockSource(); }, error),
            "重复注册 -> 失败");
        check(!error.empty(), "有错误串");

        error.clear();
        check(!data::registerSource("", [] { return (RecyclerDataSource*)new MockSource(); }, error),
            "空名字 -> 失败");

        error.clear();
        check(!data::registerSource("empty", data::SourceFactory(), error), "空工厂 -> 失败");

        check(data::sourceNames().size() == 1, "失败的注册没有进表");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
