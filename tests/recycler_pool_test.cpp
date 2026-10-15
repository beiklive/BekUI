// 无窗口测试：第 3 刀 —— cell 池上限的入池判定与默认值（纯函数，不依赖布局）。

#include <cstdio>
#include <string>

#include "bklv/views/recycler.hpp"

using namespace bklv;

#include "test_util.hpp"

int main()
{
    std::printf("== 1. 入池判定：0 = 从不复用；上限内复用；超限销毁 ==\n");
    {
        check(!shouldPoolCell(0, 0), "limit=0 -> 从不入池（用完全销毁）");
        check(!shouldPoolCell(5, 0), "limit=0 且池里已有 -> 仍然不入池");
        check(shouldPoolCell(0, 1), "limit=1、池空 -> 入池");
        check(!shouldPoolCell(1, 1), "limit=1、池满 -> 销毁");
        check(shouldPoolCell(31, 32), "limit=32、池 31 -> 入池（最后一个名额）");
        check(!shouldPoolCell(32, 32), "limit=32、池满 -> 销毁");
        check(!shouldPoolCell(1000, 32), "滑过千行后池满 -> 一路销毁（不会无限涨）");
    }

    std::printf("\n== 2. 默认配置与配置往返 ==\n");
    {
        RecyclerFrame list;
        checkEqualInt((int)list.getLoadConfig().maxPoolPerIdentifier, 32, "默认上限 32");

        RecyclerLoadConfig config = list.getLoadConfig();
        config.maxPoolPerIdentifier = 4;
        list.setLoadConfig(config);
        checkEqualInt((int)list.getLoadConfig().maxPoolPerIdentifier, 4, "改成 4 生效");

        config.maxPoolPerIdentifier = 0;
        list.setLoadConfig(config);
        check(!shouldPoolCell(0, list.getLoadConfig().maxPoolPerIdentifier), "上限设 0 -> 完全不复用");
    }

    std::printf("\n== 3. 空池的诊断读数 ==\n");
    {
        RecyclerFrame list;
        checkEqualInt((int)list.getPooledCellCount(), 0, "新帧池是空的");
        checkEqualInt((int)list.getPooledCellCount("没有这个 identifier"), 0, "未出现过的 identifier 读 0");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
