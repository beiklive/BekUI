// 无窗口测试：解码队列的纯调度（优先级 / 并发上限 / 每帧上传配额 / 去重 / 取消）。

#include <cstdio>
#include <string>
#include <vector>

#include "bklv/core/decode_queue.hpp"
#include "bklv/core/graphics.hpp"

using namespace bklv;

#include "test_util.hpp"

int main()
{
    std::printf("== 1. 按优先级出队（数字越小越优先）==\n");
    {
        clearDecodeQueueForTest();
        setDecodeConcurrency(0); // 不限
        std::vector<std::string> started;
        enqueueDecode("c", 5, [&] { started.push_back("c"); });
        enqueueDecode("a", 1, [&] { started.push_back("a"); });
        enqueueDecode("b", 3, [&] { started.push_back("b"); });
        checkEqualInt((int)getPendingDecodeCount(), 3, "3 个待启动");

        pumpDecodeQueue();
        checkEqualInt((int)started.size(), 3, "全都启动了");
        check(started.size() == 3 && started[0] == "a" && started[1] == "b" && started[2] == "c",
            "顺序是 a(1) -> b(3) -> c(5)");
    }

    std::printf("\n== 2. 并发上限：在飞的没完成就不启动下一个 ==\n");
    {
        clearDecodeQueueForTest();
        setDecodeConcurrency(1);
        std::vector<std::string> started;
        enqueueDecode("x", 1, [&] { started.push_back("x"); });
        enqueueDecode("y", 2, [&] { started.push_back("y"); });

        pumpDecodeQueue();
        checkEqualInt((int)started.size(), 1, "并发 1 -> 只启动 1 个");
        checkEqualInt((int)getInFlightDecodeCount(), 1, "1 个在飞");
        checkEqualInt((int)getPendingDecodeCount(), 1, "1 个还在排队");

        notifyDecodeFinished("x", [] {});
        pumpDecodeQueue();
        checkEqualInt((int)started.size(), 2, "腾出名额后启动下一个");
    }

    std::printf("\n== 3. 每帧上传配额 ==\n");
    {
        clearDecodeQueueForTest();
        setMaxUploadsPerFrame(1);
        int uploaded = 0;
        notifyDecodeFinished("a", [&] { ++uploaded; });
        notifyDecodeFinished("b", [&] { ++uploaded; });
        checkEqualInt((int)getPendingUploadCount(), 2, "2 个待上传");

        pumpUploads();
        checkEqualInt(uploaded, 1, "配额 1 -> 这一帧只上传 1 个");
        pumpUploads();
        checkEqualInt(uploaded, 2, "下一帧再传 1 个");
        checkEqualInt((int)getPendingUploadCount(), 0, "传完清空");
    }

    std::printf("\n== 4. 去重与取消 ==\n");
    {
        clearDecodeQueueForTest();
        setDecodeConcurrency(0);
        int startedCount = 0;
        enqueueDecode("dup", 1, [&] { ++startedCount; });
        enqueueDecode("dup", 1, [&] { ++startedCount; });
        checkEqualInt((int)getPendingDecodeCount(), 1, "同名只入队一次");

        cancelDecode("dup");
        pumpDecodeQueue();
        checkEqualInt(startedCount, 0, "取消后不启动");

        notifyDecodeFinished("up", [&] { ++startedCount; });
        cancelDecode("up");
        pumpUploads();
        checkEqualInt(startedCount, 0, "取消后不上传");
    }

    std::printf("\n== 5. 0 = 不限；边界不崩 ==\n");
    {
        clearDecodeQueueForTest();
        setDecodeConcurrency(0);
        setMaxUploadsPerFrame(0);
        int n = 0;
        for (int i = 0; i < 3; ++i)
            enqueueDecode("k" + std::to_string(i), 10 - i, [&] { ++n; });
        pumpDecodeQueue();
        checkEqualInt(n, 3, "并发不限 -> 全启动");
        setDecodeConcurrency((size_t)-1);
        setMaxUploadsPerFrame((size_t)-1);
        pumpDecodeQueue();
        pumpUploads();
        check(n == 3, "极大值不崩");

        setDecodeConcurrency(4);   // 复原默认
        setMaxUploadsPerFrame(2);
        clearDecodeQueueForTest();
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
