#include "bklv/core/font.hpp"

#include <cstdio>
#include <string>

#include "imgui.h"

#include "bklv/core/logger.hpp"

#if defined(BKLV_SWITCH)
#include <switch.h>
#endif

namespace bklv
{

static ImFont* g_defaultFont         = nullptr;
static unsigned int g_fontGeneration = 0;

namespace
{

// 界面基准字号。桌面与 Switch 用同一个值（布局按百分比算，改它会牵动所有文本排版）。
constexpr float FONT_SIZE = 20.0f;

bool fileExists(const char* path)
{
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr)
        return false;
    std::fclose(file);
    return true;
}

#if defined(BKLV_SWITCH)

// 用户自备字体：放在 SD 卡上就优先用它，对应 borealis 的 USER_FONT_PATH。
// 共享字体覆盖不到的字符（特殊符号、自定义风格）可以靠它补。
// 只在 Switch 上有意义（sdmc: 是 Switch 的路径），所以放在这个分支里。
constexpr const char* USER_FONT_PATH = "sdmc:/bklv/font.ttf";

// Nintendo 专用字符在 Unicode 私用区（PUA），不在任何常规字符范围里。
// 对应 borealis 把 FONT_SWITCH_ICONS 挂进 fallback 链的做法。
const ImWchar NINTENDO_PUA_RANGES[] = { 0xE000, 0xF8FF, 0 };

// 从 Switch 的共享字体服务（pl）取一套系统字体挂进图集。
//
// 两个必须注意的点：
//   1. AddFontFromMemoryTTF 默认**接管内存所有权**，销毁图集时会 free 这块内存；
//      而它是 pl 服务的共享内存，free 会直接崩。必须 FontDataOwnedByAtlas = false。
//   2. 字体数据要活到图集销毁为止（ImGui 1.92 起的明确要求），
//      所以这里不调 plExit()，服务句柄留给进程退出时回收。
//
// merge = true 表示合并进上一个字体，等价于 borealis 的 nvgAddFallbackFontId：
// 主字体缺的字形去这些字体里找。
ImFont* addSharedFont(ImFontAtlas* atlas, PlSharedFontType type, const ImWchar* ranges, bool merge)
{
    PlFontData data;
    if (R_FAILED(plGetSharedFontByType(&data, type)))
        return nullptr;

    ImFontConfig config;
    config.FontDataOwnedByAtlas = false; // 见上面第 1 点
    config.OversampleH          = 2;
    config.OversampleV          = 1;
    config.MergeMode            = merge;

    return atlas->AddFontFromMemoryTTF(data.address, (int)data.size, FONT_SIZE, &config, ranges);
}

// Switch 上不读字体文件：系统共享字体里就有完整的中日韩字形。
bool loadSwitchFonts(ImFontAtlas* atlas)
{
    const Result rc = plInitialize(PlServiceType_User);
    if (R_FAILED(rc))
    {
        BKLV_LOG_ERROR("plInitialize 失败（0x%x），读不到系统共享字体", (unsigned)rc);
        return false;
    }

    // 主字体：简体中文。Switch 的中文字体自带完整 ASCII，日常界面够用。
    ImFont* main =
        addSharedFont(atlas, PlSharedFontType_ChineseSimplified, atlas->GetGlyphRangesChineseFull(), false);

    if (main == nullptr)
    {
        // 简中取不到就退到 Standard，至少拉丁字母能看。
        main = addSharedFont(atlas, PlSharedFontType_Standard, atlas->GetGlyphRangesDefault(), false);
        if (main != nullptr)
            Logger::warning("Switch 共享字体：简体中文不可用，仅使用 Standard");
    }

    if (main == nullptr)
    {
        Logger::error("Switch 共享字体一个都取不到");
        return false;
    }

    // fallback 链，顺序与 borealis 一致：
    // Standard -> 简中扩展 -> 繁中 -> 韩文 -> Nintendo 图标。
    addSharedFont(atlas, PlSharedFontType_Standard, atlas->GetGlyphRangesDefault(), true);
    addSharedFont(atlas, PlSharedFontType_ExtChineseSimplified, atlas->GetGlyphRangesChineseFull(), true);
    addSharedFont(atlas, PlSharedFontType_ChineseTraditional, atlas->GetGlyphRangesChineseFull(), true);
    addSharedFont(atlas, PlSharedFontType_KO, atlas->GetGlyphRangesKorean(), true);
    addSharedFont(atlas, PlSharedFontType_NintendoExt, NINTENDO_PUA_RANGES, true);

    setDefaultFont(main);
    Logger::info("已加载系统共享字体（简中 + Standard / 简中扩展 / 繁中 / 韩文 / Nintendo 图标回退）");
    return true;
}

#endif // BKLV_SWITCH

} // namespace

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

#if defined(BKLV_SWITCH)
    // 用户自备字体优先（对应 borealis 的自定义字体覆盖）。
    if (fileExists(USER_FONT_PATH))
    {
        ImFont* user = io.Fonts->AddFontFromFileTTF(USER_FONT_PATH, FONT_SIZE, nullptr,
            io.Fonts->GetGlyphRangesChineseFull());
        if (user != nullptr)
        {
            setDefaultFont(user);
            Logger::info(std::string("已加载自定义字体：") + USER_FONT_PATH);
            return;
        }
        Logger::warning(std::string("自定义字体解析失败，改用系统共享字体：") + USER_FONT_PATH);
    }

    if (loadSwitchFonts(io.Fonts))
        return;
#else
    // 桌面上没有"系统共享字体"这种机制，按优先级试常见的中文字体文件路径。
#if defined(_WIN32)
    const char* candidates[] = {
        "C:/Windows/Fonts/msyh.ttc",   // 微软雅黑（Vista 起的默认中文 UI 字体）
        "C:/Windows/Fonts/msyh.ttf",   // 老版本 Windows 上是 TTF
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
        if (!fileExists(path))
            continue;

        ImFontConfig config;
        config.OversampleH = 2;
        config.OversampleV = 1;

        ImFont* font =
            io.Fonts->AddFontFromFileTTF(path, FONT_SIZE, &config, io.Fonts->GetGlyphRangesChineseFull());
        if (font != nullptr)
        {
            setDefaultFont(font);
            Logger::info(std::string("已加载字体：") + path);
            return;
        }
    }
#endif // BKLV_SWITCH

    // 一个都没加载上时退回内置字体。注意 AddFontDefault 只在字体表为空时才建字体，
    // 所以不能直接取 Fonts[0]，否则前面残留的半成品条目会让这里取到空指针。
    ImFont* fallback = io.Fonts->AddFontDefault();
    if (fallback == nullptr && !io.Fonts->Fonts.empty())
        fallback = io.Fonts->Fonts[0];

    setDefaultFont(fallback);
    if (fallback == nullptr)
        Logger::error("内置字体也创建失败，界面文字将不可见");
    else
        Logger::warning("未找到系统中文字体，已退回 ImGui 内置字体（中文会显示成方块）");
}

} // namespace bklv
