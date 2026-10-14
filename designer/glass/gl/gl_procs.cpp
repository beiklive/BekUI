#include "designer/glass/gl/gl_procs.hpp"

#include "bklv/core/logger.hpp"

#include <SDL3/SDL.h>

namespace designer::glass
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
#define BKLV_DESIGNER_LOAD(ret, name, args)                                          \
    procs.name = (ret(*) args)SDL_GL_GetProcAddress("gl" #name);                     \
    if (procs.name == nullptr)                                                       \
    {                                                                                \
        BKLV_LOG_WARNING("[玻璃] 取不到 GL 入口 gl%s", #name);                        \
        missing++;                                                                   \
    }
    BKLV_DESIGNER_GL_FUNCTIONS(BKLV_DESIGNER_LOAD)
#undef BKLV_DESIGNER_LOAD

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

} // namespace designer::glass
