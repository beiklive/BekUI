// 无窗口测试：item 绑定挂点注册表 + 它被 RecyclerItemCell 真正调用。

#include <cstdio>
#include <string>
#include <vector>

#include "bklv/views/item_binder.hpp"
#include "bklv/widgets/recycler_view.hpp"

using namespace bklv;

#include "test_util.hpp"

int main()
{
    std::printf("== 1. 注册表本身 ==\n");
    {
        binding::clearItemBindersForTest();
        std::string error;
        check(binding::registerItemBinder("mock", [](View&, size_t, size_t) {}, error), "注册成功");
        check(binding::hasItemBinder("mock"), "hasItemBinder 为真");
        check(binding::itemBinderNames().size() == 1, "名字表里有 1 个");
        check((bool)binding::findItemBinder("mock"), "能取到");

        error.clear();
        check(!binding::registerItemBinder("mock", [](View&, size_t, size_t) {}, error), "重复注册 -> 失败");
        error.clear();
        check(!binding::registerItemBinder("", [](View&, size_t, size_t) {}, error), "空名字 -> 失败");
        error.clear();
        check(!binding::registerItemBinder("empty", binding::ItemBinder(), error), "空函数 -> 失败");
        check(binding::itemBinderNames().size() == 1, "失败的注册没进表");
        check(!binding::findItemBinder("不存在"), "没注册过 -> 空 function");
    }

    std::printf("\n== 2. RecyclerItemCell 真的会调它，并把行号传进去 ==\n");
    {
        binding::clearItemBindersForTest();
        std::string error;
        std::vector<size_t> seen;
        check(binding::registerItemBinder("collect",
                  [&seen](View&, size_t, size_t index) { seen.push_back(index); }, error),
            "注册收集用的绑定函数");

        RecyclerItemCell cell;
        cell.setAppearanceName("base/rectangle.card"); // 先给模板（惰性创建内容）
        cell.setItemBinderName("collect");
        cell.setIndex(7);
        cell.setIndex(8); // 复用：同一个 cell 承载别的行

        checkEqualInt((int)seen.size(), 2, "绑定函数被调了两次");
        if (seen.size() == 2)
        {
            checkEqualInt((int)seen[0], 7, "第一次拿到行号 7");
            checkEqualInt((int)seen[1], 8, "复用后拿到行号 8");
        }
        check(cell.getContent() != nullptr, "内容已创建");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
