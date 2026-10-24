#include "bklv/media/font.hpp"

#include "bklv/media/icon_font.hpp"

#include <cstdio>
#include <string>

#include "imgui.h"

#include "bklv/core/logger.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/services.hpp"

#if defined(BKLV_SWITCH)
#include <switch.h>
#endif

namespace bklv
{

static ImFont* g_defaultFont         = nullptr;
static const float ICON_FONT_SIZE    = 22.0f; // 图标字形按这个字号栅格化
static unsigned int g_fontGeneration = 0;

namespace
{

// Switch 图标字体用的私有区（borealis 的字形都在 U+E000~U+F8FF 里）
static const ImWchar PUA_RANGES[] = { 0xE000, 0xF8FF, 0 };

// 扫描私有区时用的范围（和 PUA_RANGES 一致）
constexpr unsigned int PUA_FIRST = 0xE000;
constexpr unsigned int PUA_LAST  = 0xF8FF;

// switch_icons.ttf 里 U+E154 起全是占位字形（空框/缺字方块），不是真图标 ——
// "列出所有可用图标"的接口裁到这里为止（验收时逐屏看过确认的边界）。
constexpr unsigned int SWITCH_ICON_LAST_USABLE = 0xE153;

// 图标字体：hint 栏的按键字形（A/B/X/Y、方向键…）从这里取。
// Switch 上系统共享字体的 NintendoExt 就带这些字形，桌面则随包带 switch_icons.ttf。
ImFont* g_iconFont = nullptr;

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

std::vector<unsigned int> scanFontCodepoints(ImFont* font)
{
    std::vector<unsigned int> result;
    if (font == nullptr)
        return result;

    // 两道判断缺一不可：
    //   ① IsGlyphInFont：字体文件里**有没有这个码位**（内部走 stbtt_FindGlyphIndex，
    //      不是"码位在不在请求的范围里"）；
    //   ② 图集里**真的取得到字形**：有些字体给一堆码位留了空槽（cmap 有映射、轮廓是空的），
    //      只查 ① 会把这些画出来是"缺字方块"的码位也算成图标。
    ImFontBaked* baked = font->GetFontBaked(ICON_FONT_SIZE);
    if (baked == nullptr)
        return result;

    result.reserve(256);
    for (unsigned int cp = PUA_FIRST; cp <= PUA_LAST; cp++)
    {
        if (!font->IsGlyphInFont(static_cast<ImWchar>(cp)))
            continue;

        const ImFontGlyph* glyph = baked->FindGlyphNoFallback(static_cast<ImWchar>(cp));
        if (glyph == nullptr || !glyph->Visible || glyph->PackId < 0)
            continue;

        result.push_back(cp);
    }
    return result;
}

void setIconFont(ImFont* font)
{
    g_iconFont = font;
}

const std::vector<unsigned int>& getIconFontCodepoints()
{
    // 缓存：字体或字体版本变了才重扫（扫一遍是 6400 次查表，不值得每帧做）
    static std::vector<unsigned int> cached;
    static ImFont* cachedFont      = nullptr;
    static unsigned int cachedGen  = 0;

    if (cachedFont != g_iconFont || cachedGen != g_fontGeneration)
    {
        cached = scanFontCodepoints(g_iconFont);

        // 扫出来的码位是升序的，占位字形都在尾部 → 直接切尾
        while (!cached.empty() && cached.back() > SWITCH_ICON_LAST_USABLE)
            cached.pop_back();

        cachedFont = g_iconFont;
        cachedGen  = g_fontGeneration;
    }

    return cached;
}

ImFont* getIconFont()
{
    // 没单独加载图标字体时退回默认字体：Switch 的 NintendoExt 回退链能画出这些字形，
    // 桌面上如果没找到 switch_icons.ttf 也是同理（至少不会崩）。
    return g_iconFont != nullptr ? g_iconFont : g_defaultFont;
}

bool hasIconFont()
{
    return g_iconFont != nullptr;
}

void setDefaultFont(ImFont* font)
{
    // 同时告诉 ImGui：**ImGui 自己的界面文字**（调试窗口 / 日志窗口 / 设计器）
    // 要用哪一个字体。不设的话 ImGui 用 iofonts 里的第 0 个 —— 而那可能是
    // 先加载的**图标字体**（只有私用区几个字形），于是 ImGui::Text 什么都画不出来
    // （bklv 的文本没事，因为它自己传字体）。踩过。
    if (ImGui::GetCurrentContext() != nullptr)
        ImGui::GetIO().FontDefault = font;

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

// 图标字体：按键字形（switch_icons.ttf）与 Material 图标库。
// **两个平台都要装** —— 以前这段只在桌面的 #else 分支里，Switch 上加载完系统共享字体就
// 直接 return 了，于是真机上"所有图标字体都不显示"（踩过）。
// 必须在图集建好之前调用：建好之后再加字体 ImGui 会崩（设计器踩过）。
static void loadIconFonts(ImFontAtlas* atlas)
{
    const std::string iconPath = resourcePath("resources/font/switch_icons.ttf");
    if (fileExists(iconPath.c_str()))
    {
        ImFont* icon
            = atlas->AddFontFromFileTTF(iconPath.c_str(), ICON_FONT_SIZE, nullptr, PUA_RANGES);
        if (icon != nullptr)
        {
            setIconFont(icon);
            Logger::info("已加载图标字体：resources/font/switch_icons.ttf");
        }
        else
            Logger::warning("图标字体解析失败：resources/font/switch_icons.ttf");
    }
    else
        Logger::info("没找到图标字体 resources/font/switch_icons.ttf，hint 图标用文字代替");

    const std::string materialPath = resourcePath("resources/font/MaterialIcons-Regular.ttf");
    if (fileExists(materialPath.c_str()))
    {
        ImFontConfig config;
        config.FontDataOwnedByAtlas = true;
        ImFont* material = atlas->AddFontFromFileTTF(materialPath.c_str(), ICON_FONT_SIZE,
            &config, PUA_RANGES);
        if (material != nullptr)
        {
            bklv::icon::setFont(material);
            Logger::info("已加载图标库：resources/font/MaterialIcons-Regular.ttf");
        }
        else
            Logger::warning("图标字体解析失败：resources/font/MaterialIcons-Regular.ttf");
    }
    else
        Logger::info("没找到 resources/font/MaterialIcons-Regular.ttf，图标用文字代替");
}

void loadFonts()
{
    ImGuiIO& io = ImGui::GetIO();

    // 图标字体先装：两个平台都要，不能被下面各分支的 return 跳过
    loadIconFonts(io.Fonts);

    // 宿主字体：优先用 FontProvider 提供的字节/路径（嵌入模式）
    if (FontProvider* provider = getServices().fonts)
    {
        std::vector<unsigned char> bytes;
        std::string error;
        if (provider->readBodyFont(bytes, error) && !bytes.empty())
        {
            void* data = ImGui::MemAlloc(bytes.size());
            if (data != nullptr)
            {
                std::memcpy(data, bytes.data(), bytes.size());
                ImFontConfig config;
                config.OversampleH = 2;
                config.OversampleV = 1;
                config.FontDataOwnedByAtlas = true;
                ImFont* font = io.Fonts->AddFontFromMemoryTTF(data, (int)bytes.size(), FONT_SIZE,
                    &config, io.Fonts->GetGlyphRangesChineseFull());
                if (font != nullptr)
                {
                    setDefaultFont(font);
                    Logger::info("已加载宿主字体（FontProvider 字节）");
                    return;
                }
            }
        }

        const std::string path = provider->resolveBodyFont();
        if (!path.empty() && fileExists(path.c_str()))
        {
            ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), FONT_SIZE, nullptr,
                io.Fonts->GetGlyphRangesChineseFull());
            if (font != nullptr)
            {
                setDefaultFont(font);
                Logger::info(std::string("已加载宿主字体：") + path);
                return;
            }
        }
    }

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
