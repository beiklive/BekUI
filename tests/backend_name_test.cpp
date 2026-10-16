// 无窗口测试：图形后端名只能有一个事实来源。
//
// 名字有两处来源：
//   · `getGraphicsBackendName()`（core/graphics.cpp）—— **编译期**表。上下文还没建起来时
//     （App 构造期就会打一行"图形后端：xxx"）也要能回答，所以它不能转发给后端对象。
//   · 各后端的 `getName()` —— 运行时的自报名，纹理仓库与调试层用它。
// 两处各写一遍字面量，历史上就漂过：GL 构建下编译期表返回 "OpenGL 3.2 Core"、
// OpenGLBackend::getName() 返回 "OpenGL"，同一次运行里日志与调试层显示两个名字。
// 这条测试把两者钉在一起：以后谁改了其中一处，这里立刻失败。
//
// 注意：本测试**不**调用 initTextureStore()，只读名字，不碰任何图形状态。

#include <cstdio>
#include <string>

#include "bklv/core/graphics.hpp"
#include "test_util.hpp"

#if defined(BKLV_SWITCH)
#include "bklv/backends/gles3_backend.hpp"
#elif defined(BKLV_VULKAN)
#include "bklv/backends/vulkan_backend.hpp"
#else
#include "bklv/backends/opengl_backend.hpp"
#endif

using namespace bklv;

int main()
{
    std::printf("== 图形后端名一致性（编译期表 vs 后端自报名）\n");

    const std::string compileTime = getGraphicsBackendName();

    // 构造只为问一句名字：三个后端的构造函数都不碰图形上下文。
    const std::string fromBackend = [] {
#if defined(BKLV_SWITCH)
        GLES3Backend backend;
#elif defined(BKLV_VULKAN)
        VulkanBackend backend;
#else
        OpenGLBackend backend;
#endif
        return std::string(backend.getName());
    }();

    checkEqual(compileTime, fromBackend, "getGraphicsBackendName() 等于后端自己的 getName()");
    check(!compileTime.empty(), "后端名非空");
    check(compileTime != "none", "编译期表不返回 none（那是纹理仓库没有后端时的兜底）");

    // 纹理仓库在后端就位前也用同一个兜底字符串，这里顺带钉住它没被改名。
    checkEqual(std::string(getTextureBackendName()), std::string("none"),
        "还没 initTextureStore 时纹理仓库报 none");
    check(!isTextureStoreReady(), "还没 initTextureStore 时仓库未就绪");

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
