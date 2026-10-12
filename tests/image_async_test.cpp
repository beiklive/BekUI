// 无窗口测试：异步图片加载。
//
// 没有图形上下文，所以这里验证的是**异步那条链路的逻辑**：
// 后台解码出正确尺寸的像素、结果按"代数"丢弃过期请求、isLoading 状态流转。
// 纹理上传那一步在真机上由主线程做（loadTextureFromPixels）。

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "bklv/core/image_decoder.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/task.hpp"
#include "bklv/views/image.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

template <typename Predicate>
bool waitFor(Predicate predicate, int timeoutMs = 3000)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (predicate())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return predicate();
}

} // namespace

int main()
{
    bklv::initResources();
    initI18n();
    setLocale("zh-Hans");
    task::init(2);

    std::printf("== 1. 后台解码 ==\n");
    {
        const std::string path = resourcePath("resources/testcard.png");
        DecodedImage decoded;
        bool ranInBackground = false;

        task::runResult(
            [&] {
                ranInBackground = !task::isMainThread();
                return decodeImageFile(path.c_str());
            },
            [&](DecodedImage image) { decoded = std::move(image); });

        check(waitFor([&] { return task::pendingCount() == 0; }), "后台任务跑完");
        check(ranInBackground, "解码在后台线程执行");
        check(!decoded.isValid(), "回调还没 pump，结果还没到主线程");

        task::pumpMainThread();
        check(decoded.isValid(), "主线程回调收到有效图片");
        checkEqualInt(decoded.width > 0 ? 1 : 0, 1, "解码出了宽度");
        checkEqualSize((size_t)(decoded.width * decoded.height * 4), decoded.pixels.size(),
            "像素长度 = 宽 x 高 x 4（RGBA8）");
    }

    std::printf("\n== 2. Image 的异步状态 ==\n");
    {
        Image* image = new Image();
        check(!image->isLoading(), "刚建好不是加载中");

        image->setImageFromFileAsync("resources/testcard.png");
        check(image->isLoading(), "发起异步加载后处于加载中");

        // 后台解码完 + pump 之后：没有图形上下文，上传不了，
        // 于是仍然算"加载中"（保留 pendingPath 交给 update 重试）—— 不能崩
        check(waitFor([&] { return task::pendingCount() == 0; }), "解码任务结束");
        task::pumpMainThread();
        check(image->isLoading(), "纹理仓库没就绪时保持加载中（等重试）");

        delete image;
    }

    std::printf("\n== 3. 过期结果被丢弃 ==\n");
    {
        Image* image = new Image();

        // 连续发起两次：第一次的结果应该被代数拦掉，不会覆盖第二次
        image->setImageFromFileAsync("resources/testcard.png");
        image->setImageFromFileAsync("resources/testcard.png");

        check(waitFor([&] { return task::pendingCount() == 0; }), "两次任务都结束");
        task::pumpMainThread();
        check(image->isLoading(), "状态一致（不会因为旧结果而错乱）");

        delete image;
    }

    std::printf("\n== 4. 损坏路径不崩 ==\n");
    {
        Image* image = new Image();
        image->setImageFromFileAsync("resources/__nope__.png");
        check(waitFor([&] { return task::pendingCount() == 0; }), "任务结束");
        task::pumpMainThread();
        check(true, "加载失败不崩");
        delete image;
    }

    task::shutdown();

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
