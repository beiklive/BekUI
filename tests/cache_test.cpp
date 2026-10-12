// 无窗口测试：LRU 缓存（命中/未命中、淘汰顺序、脏标记、钉住）。

#include <cstdio>
#include <string>

#include "bklv/core/cache.hpp"

using namespace bklv;

#include "test_util.hpp"

int main()
{
    std::printf("== 1. 命中与未命中 ==\n");
    {
        LruCache<std::string, int> cache(4);
        int loads = 0;
        auto loader = [&loads] { loads++; return 42; };

        checkEqualInt(cache.get("a", loader), 42, "第一次取到值");
        checkEqualInt(loads, 1, "未命中调了一次 loader");
        checkEqualInt(cache.get("a", loader), 42, "第二次命中");
        checkEqualInt(loads, 1, "命中不再调 loader");
        checkEqualSize(cache.getHitCount(), 1, "命中计数 1");
        checkEqualSize(cache.getMissCount(), 1, "未命中计数 1");
        check(cache.has("a"), "has 命中");
        check(!cache.has("b"), "没放进去的 key 返回 false");
    }

    std::printf("\n== 2. LRU 淘汰顺序 ==\n");
    {
        LruCache<std::string, int> cache(2);
        int value = 0;
        auto loader = [&value] { return ++value; };

        cache.get("a", loader); // a
        cache.get("b", loader); // b
        cache.get("a", loader); // 命中 a → a 变成最近使用
        cache.get("c", loader); // 淘汰最久未用的 b

        check(cache.has("a"), "a 还在");
        check(cache.has("c"), "c 进来了");
        check(!cache.has("b"), "b 被淘汰（它最久没用）");
        checkEqualSize(cache.size(), 2, "容量保持 2");
    }

    std::printf("\n== 3. 脏标记与失效 ==\n");
    {
        LruCache<std::string, int> cache(4);
        int value = 0;
        auto loader = [&value] { return ++value; };

        checkEqualInt(cache.get("k", loader), 1, "第一次生成 1");
        cache.invalidate("k");
        check(!cache.has("k"), "标脏后 has 认为是过期的");
        checkEqualInt(cache.get("k", loader), 2, "重新生成 2");
        checkEqualSize(cache.getMissCount(), 2, "两次未命中");

        cache.invalidateAll();
        checkEqualInt(cache.get("k", loader), 3, "invalidateAll 之后也重新生成");
    }

    std::printf("\n== 4. put 与钉住 ==\n");
    {
        LruCache<std::string, int> cache(2);
        cache.put("x", 10);
        checkEqualInt(cache.get("x", nullptr), 10, "put 进去的值直接命中");
        checkEqualSize(cache.getHitCount(), 1, "不调 loader");

        // 钉住 x，再塞两个新 key：x 不能被淘汰
        {
            auto pin = cache.pin("x");
            check(pin.isPinned(), "钉住句柄有效");
            cache.put("y", 20);
            cache.put("z", 30);
            check(cache.has("x"), "被钉住的条目不会被淘汰");
        } // 解除钉住

        cache.put("w", 40);
        check(!cache.has("x") || cache.size() <= 2, "解除钉住后容量约束重新生效");
        check(cache.size() <= 2, "容量始终不超过上限");
    }

    std::printf("\n== 5. 容量与统计 ==\n");
    {
        LruCache<std::string, int> cache(0); // 0 会被夹到 1，不能崩
        checkEqualSize(cache.getCapacity(), 1, "容量 0 夹到 1");

        int n = 0;
        cache.get("a", [&n] { return ++n; });
        cache.get("b", [&n] { return ++n; });
        checkEqualSize(cache.size(), 1, "容量 1 时只留一个");

        cache.setCapacity(3);
        checkEqualSize(cache.getCapacity(), 3, "可以调大容量");

        cache.clear();
        checkEqualSize(cache.size(), 0, "clear 清空");
        checkEqualSize(cache.getHitCount(), 0, "统计也清零");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
