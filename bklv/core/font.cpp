#include "bklv/core/font.hpp"

#include <cstdio>
#include <string>

#include "imgui.h"

#include "bklv/core/logger.hpp"

namespace bklv
{

static ImFont* g_defaultFont          = nullptr;
static unsigned int g_fontGeneration  = 0;

ImFont* getDefaultFont()
{
    return g_defaultFont;
}

void setDefaultFont(ImFont* font)
{
    if (g_defaultFont == font)
        return;

    g_defaultFont = font;
    // 字体变了：之前基于"无字体"算出来的文本尺寸全部作废。
    g_fontGeneration++;
}

unsigned int getFontGeneration()
{
    return g_fontGeneration;
}

void loadFonts()
{
    ImGuiIO& io = ImGui::GetIO();

    // 各平台常见的中文字体，按优先级尝试。第一个能打开且能解析的胜出。
#if defined(_WIN32)
    const char* candidates[] = {
        "C:/Windows/Fonts/msyh.ttc",  // 微软雅黑（Vista 起的默认中文 UI 字体）
        "C:/Windows/Fonts/msyh.ttf",  // 老版本 Windows 上是 TTF
        "C:/Windows/Fonts/simhei.ttf", // 黑体，覆盖最老的系统
        "C:/Windows/Fonts/simsun.ttc", // 宋体，最后的兜底
    };
#elif defined(__APPLE__)
    const char* candidates[] = {
        "/System/Library/Fonts/Hiragino Sans GB.ttc",
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/STHeiti Medium.ttc",
    };
#else
    const char* candidates[] = {
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    };
#endif

    for (const char* path : candidates)
    {
        // 先判断文件是否存在，避免 AddFontFromFileTTF 打印一堆错误日志。
        std::FILE* file = std::fopen(path, "rb");
        if (file == nullptr)
            continue;
        std::fclose(file);

        ImFontConfig config;
        config.OversampleH = 2;
        config.OversampleV = 1;

        ImFont* font = io.Fonts->AddFontFromFileTTF(path, 20.0f, &config, io.Fonts->GetGlyphRangesChineseFull());
        if (font != nullptr)
        {
            setDefaultFont(font);
            Logger::info(std::string("已加载字体：") + path);
            return;
        }
    }

    // 一个都没加载上时退回内置字体。注意 AddFontDefault 只在字体表为空时才建字体，
    // 所以不能直接取 Fonts[0]，否则前面残留的半成品条目会让这里取到空指针。
    ImFont* fallback = io.Fonts->AddFontDefault();
    if (fallback == nullptr && !io.Fonts->Fonts.empty())
        fallback = io.Fonts->Fonts[0];

    setDefaultFont(fallback);
    if (fallback == nullptr)
        Logger::error("内置字体也创建失败，界面文字将不可见");
    else
        Logger::warning("未找到系统中文字体，已退回 ImGui 内置字体");
}

} // namespace bklv
