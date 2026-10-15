// 无窗口测试：JSON 描述回收列表 —— 走真实装载路径（主题 view JSON -> 类型 -> 属性）。
// 注意：Switch 带 -fno-rtti（不许 dynamic_cast），所以这里不"找类型"，分两半验证：
//   ① JSON 真能建出视图（类型已注册、属性认了）
//   ② 属性落地的效果直接构造 RecyclerView 来验

#include <cstdio>
#include <string>

#include "bklv/core/attributes.hpp"
#include "bklv/core/json_view.hpp"
#include "bklv/views/data_source.hpp"
#include "bklv/widgets/recycler_view.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

class MockSource : public RecyclerDataSource
{
  public:
    size_t getItemCount() override { return 5; }
    float getItemHeight(size_t) override { return 64.0f; }
};

} // namespace

int main()
{
    std::printf("== 1. JSON 能建出 Recycler（类型 + 属性都认）==\n");
    {
        data::clearSourcesForTest();
        std::string error;
        check(data::registerSource("recycler_demo",
                  [] { return (RecyclerDataSource*)new MockSource(); }, error),
            "注册数据源");

        // 类型必须先注册（app 里由 registerWidgetViewTypes() 做；测试里只注册被测的那个）
        registerRecyclerViewType();
        View* page = createViewFromTheme("recycler_json_demo");
        check(page != nullptr, "主题里的 recycler_json_demo 能建出视图");
    }

    std::printf("\n== 2. 属性落到 list 上（用库里 JSON 装载器走的同一个入口）==\n");
    {
        RecyclerView list;
        check(applyViewAttribute(&list, "data", "@data/recycler_demo"), "data 属性被认（@data/ 前缀剥掉）");
        check(list.getDataSourceName() == "recycler_demo", "名字正确");
        check(list.getDataSource() != nullptr, "数据源已 attach");
        if (list.getDataSource() != nullptr)
            checkEqualInt((int)list.getDataSource()->getItemCount(), 5, "取到真实数据");

        check(applyViewAttribute(&list, "minItemWidth", "180"), "minItemWidth 被认");
        checkFloat(list.getMinItemWidth(), 180.0f, "minItemWidth 落上");

        applyViewAttribute(&list, "columns", "0");
        checkEqualInt(list.getColumns(), 1, "columns < 1 被夹到 1");

        check(applyViewAttribute(&list, "onEnd", "loadMore"), "onEnd 被认");
        check(list.getOnEndName() == "loadMore", "onEnd 名字落上");

        check(applyViewAttribute(&list, "itemAppearance", "base/rectangle.card"), "itemAppearance 被认");
        check(!applyViewAttribute(&list, "不存在的属性", "x"), "未知属性 -> 返回 false（不静默）");
    }

    std::printf("\n== 3. 没注册过的数据源名 -> 不崩、报出来 ==\n");
    {
        RecyclerView list;
        applyViewAttribute(&list, "data", "@data/不存在的数据源");
        check(list.getDataSource() == nullptr, "没注册过 -> 不 attach");
        check(list.getDataSourceName() == "不存在的数据源", "名字仍记下来（便于排查）");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
