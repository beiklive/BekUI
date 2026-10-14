#if defined(BKLV_OPENGL) || defined(BKLV_GLES3)

#if defined(BKLV_GLES3)
// Switch：GLES3 的头直接给声明，不需要运行时取地址（也没有 SDL 可用）
#include <GLES3/gl3.h>
#endif

#include "bklv/backends/gl_glass_procs.hpp"

#include "bklv/core/logger.hpp"

#if defined(BKLV_OPENGL)
#include <SDL3/SDL.h>
#endif

namespace bklv::glass
{

namespace
{

GlProcs procs;
bool loaded  = false;
bool ready   = false;

} // namespace

bool loadGlProcs()
{
    if (loaded)
        return ready;
    loaded = true;

    int missing = 0;
#if defined(BKLV_GLES3)
// GLES3：符号在链接期就解析好了，直接取地址；缺符号根本编不过，所以这里不会 missing。
#define BKLV_DESIGNER_LOAD(ret, name, args) procs.name = &gl##name;
    BKLV_DESIGNER_GL_FUNCTIONS(BKLV_DESIGNER_LOAD)
#undef BKLV_DESIGNER_LOAD
    (void)missing;
#else
#define BKLV_DESIGNER_LOAD(ret, name, args)                                          \
    procs.name = (ret(*) args)SDL_GL_GetProcAddress("gl" #name);                     \
    if (procs.name == nullptr)                                                       \
    {                                                                                \
        BKLV_LOG_WARNING("[玻璃] 取不到 GL 入口 gl%s", #name);                        \
        missing++;                                                                   \
    }
    BKLV_DESIGNER_GL_FUNCTIONS(BKLV_DESIGNER_LOAD)
#undef BKLV_DESIGNER_LOAD
#endif

    ready = missing == 0;
    if (!ready)
        BKLV_LOG_WARNING("[玻璃] 缺 %d 个 GL 入口，玻璃整体关闭（面板回退普通绘制）", missing);
    return ready;
}

GlProcs& glProcs()
{
    return procs;
}

bool isGlProcsReady()
{
    return ready;
}

} // namespace bklv::glass

#endif // BKLV_OPENGL || BKLV_GLES3
