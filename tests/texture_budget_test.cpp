// 无窗口测试：纹理预算（3.5 第一小步）—— 只记账不淘汰，默认不限。

#include <cstdio>

#include "bklv/core/graphics.hpp"

using namespace bklv;

#include "test_util.hpp"

int main()
{
    std::printf("== 1. 默认 0 = 不限 ==\n");
    {
        setTextureBudget(0);
        checkEqualInt((int)getTextureBudget(), 0, "默认不限");
        check(!isTextureBudgetExceeded(), "不限时永远不算超");
    }

    std::printf("\n== 2. 设置/读取往返 ==\n");
    {
        setTextureBudget(64);
        checkEqualInt((int)getTextureBudget(), 64, "预算 64");
        setTextureBudget(1);
        checkEqualInt((int)getTextureBudget(), 1, "预算 1");
    }

    std::printf("\n== 3. 没有后端时计数为 0（无窗口不崩）==\n");
    {
        checkEqualInt((int)getTextureCount(), 0, "未初始化仓库 -> 0 张");
        setTextureBudget(8);
        check(!isTextureBudgetExceeded(), "0 张 < 8 -> 不超");
    }

    std::printf("\n== 4. 边界：0 / 极大都不崩 ==\n");
    {
        setTextureBudget(0);
        checkEqualInt((int)getTextureCount(), 0, "0 预算下计数仍可读");
        setTextureBudget((size_t)-1);
        check(!isTextureBudgetExceeded(), "极大预算 -> 不超");
        setTextureBudget(0); // 复原，别影响别的测试
        checkEqualInt((int)getTextureBudget(), 0, "复原为不限");
    }

    std::printf("\n== 5. 缓存条目数可读（暂无后端 -> 0）==\n");
    {
        checkEqualInt((int)getTextureCacheCount(), 0, "未初始化时缓存为空");
        setTextureBudget(4);
        check(getTextureCacheCount() <= (size_t)getTextureBudget() || true, "读缓存数不崩");
        setTextureBudget(0);
    }

    std::printf("\n== 6. LRU 查询：空缓存返回空串，不崩 ==\n");
    {
        check(getLeastRecentlyUsedPath().empty(), "没有记录 -> 空串");
        setTextureBudget(0);
        check(getLeastRecentlyUsedPath().empty(), "预算变化不影响 LRU 查询");
    }

    std::printf("\n== 7. 引用计数：越界/未初始化一律安全 no-op ==\n");
    {
        checkEqualInt((int)getTextureRefCount(0), 0, "空仓库 -> 计数 0");
        checkEqualInt((int)getTextureRefCount(9999), 0, "越界 -> 计数 0");
        retainTexture(0);            // 不该崩
        retainTexture((size_t)-1);   // 越界
        releaseTexture(0);           // 不该崩
        releaseTexture(9999);
        checkEqualInt((int)getTextureRefCount(0), 0, "no-op 后计数仍是 0");
        checkEqualInt((int)getTextureCount(), 0, "存活纹理数不变");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
