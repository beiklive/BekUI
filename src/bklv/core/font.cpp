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

    // macOS 上常见的中文字体，按优先级尝试。
    const char* candidates[] = {
        "/System/Library/Fonts/Hiragino Sans GB.ttc",
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/STHeiti Medium.ttc",
    };

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

    io.Fonts->AddFontDefault();
    setDefaultFont(io.Fonts->Fonts[0]);
    Logger::warning("未找到系统中文字体，已退回 ImGui 内置字体");
}

} // namespace bklv
