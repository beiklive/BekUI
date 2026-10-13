// Material Icons 图标库：手写表 + "扫字体得到全部字形"两部分（不需要窗口）
#include "bklv/core/icon_font.hpp"

#include "bklv/core/font.hpp"
#include "bklv/core/resource.hpp"
#include "tests/test_util.hpp"

#include <set>
#include <string>

namespace
{

// UTF-8 解码首个码位（表里的图标都在 BMP，3 字节）
unsigned int decodeFirstCodepoint(const std::string& text)
{
    if (text.empty())
        return 0;
    const unsigned char c0 = (unsigned char)text[0];
    if (c0 < 0x80)
        return c0;
    if ((c0 & 0xE0) == 0xC0 && text.size() >= 2)
        return ((unsigned int)(c0 & 0x1F) << 6) | ((unsigned char)text[1] & 0x3F);
    if ((c0 & 0xF0) == 0xE0 && text.size() >= 3)
        return ((unsigned int)(c0 & 0x0F) << 12) | (((unsigned char)text[1] & 0x3F) << 6)
            | ((unsigned char)text[2] & 0x3F);
    return 0;
}

} // namespace

int main()
{
    using namespace bklv;

    initResources();
    check(!icon::all().empty(), "图标表不为空");

    std::set<std::string> names;
    std::set<unsigned int> codepoints;
    int checked = 0;
    for (const icon::Info& info : icon::all())
    {
        if (info.id == icon::Id::NONE)
            continue;
        checked++;

        check(info.name != nullptr && info.name[0] != 0, "每个图标都有名字");
        check(info.chineseName != nullptr && info.chineseName[0] != 0, "每个图标都有中文说明");
        check(names.insert(info.name).second, "图标名字不重复");
        check(codepoints.insert(info.codepoint).second, "图标码位不重复");
        check(info.codepoint >= 0xE000 && info.codepoint <= 0xF8FF, "码位落在私有区");

        const std::string glyph = icon::glyph(info.id);
        check(!glyph.empty(), "取得到字形");
        check(decodeFirstCodepoint(glyph) == info.codepoint, "字形确实是这个码位");
    }
    check(checked >= 40, "图标数量够用（至少 40 个）");

    // ---- 扫字体：字体里**实际存在**的字形（手写表只是常用子集）----
    // 无窗口测试里没有字体，所以这里造一个 ImGui 上下文，把两份图标字体按私有区装进去。
    // 注意：**不要** DestroyContext（没接平台后端的上下文会在 Shutdown 里踩空）。
    std::printf("\n== 字体里实际存在的字形（扫私用区）==\n");
    {
        ImGui::CreateContext();

        const ImWchar pua[] = { 0xE000, 0xF8FF, 0 };

        ImFont* material = ImGui::GetIO().Fonts->AddFontFromFileTTF(
            bklv::resourcePath("resources/font/MaterialIcons-Regular.ttf").c_str(), 22.0f,
            nullptr, pua);
        ImFont* switchIcons = ImGui::GetIO().Fonts->AddFontFromFileTTF(
            bklv::resourcePath("resources/font/switch_icons.ttf").c_str(), 22.0f, nullptr, pua);
        check(material != nullptr, "测试里装上了 Material 字体");
        check(switchIcons != nullptr, "测试里装上了 Switch 图标字体");
        ImGui::GetIO().Fonts->Build();

        // 没设字体时：空表、不崩
        check(icon::allGlyphs().empty(), "没装图标字体时 allGlyphs() 是空表");

        icon::setFont(material);
        const std::vector<icon::Glyph>& materialGlyphs = icon::allGlyphs();
        check(materialGlyphs.size() > 1000, "Material 字体里有一千多个字形（远多于手写表）");
        check(materialGlyphs.size() > icon::all().size() * 10, "字体里的字形数远多于手写表");

        // 逐条检查几千个字形会把"检查项计数"灌到上万，没有额外价值 ——
        // 这里把每个不变量都累计成一个布尔，最后各出一条（覆盖一样，计数干净）。
        bool ascending   = true;
        bool unique      = true;
        bool inPua       = true;
        bool utf8Length  = true;
        bool utf8Head    = true;
        bool utf8Decodes = true;
        std::set<unsigned int> seenCodepoints;
        for (size_t i = 0; i < materialGlyphs.size(); i++)
        {
            const icon::Glyph& glyph = materialGlyphs[i];
            if (i > 0 && glyph.codepoint <= materialGlyphs[i - 1].codepoint)
                ascending = false;
            if (!seenCodepoints.insert(glyph.codepoint).second)
                unique = false;

            inPua = inPua && glyph.codepoint >= 0xE000 && glyph.codepoint <= 0xF8FF;
            utf8Length  = utf8Length && glyph.utf8.size() == 3;
            utf8Head    = utf8Head
                && (unsigned char)glyph.utf8[0] == 0xE0 + ((glyph.codepoint >> 12) & 0x0F);
            utf8Decodes = utf8Decodes && decodeFirstCodepoint(glyph.utf8) == glyph.codepoint;
        }
        check(ascending, "扫出来的码位按升序");
        check(unique, "扫出来的码位不重复");
        check(inPua, "每个字形的码位都在私有区");
        check(utf8Length, "私有区码位的 UTF-8 都是 3 字节");
        check(utf8Head, "UTF-8 首字节正确");
        check(utf8Decodes, "UTF-8 解回来是同一个码位");

        // 手写表里的每个图标都必须在字体里
        bool tableCovered = true;
        for (const icon::Info& info : icon::all())
        {
            if (info.id == icon::Id::NONE)
                continue;
            if (seenCodepoints.count(info.codepoint) == 0)
                tableCovered = false;
        }
        check(tableCovered, "手写表里的图标字体里都有（表是字体的子集）");

        // 重复调用走缓存（返回同一个向量）
        check(&icon::allGlyphs() == &materialGlyphs, "allGlyphs() 结果被缓存");

        // Switch 图标字体：同一个扫描器（总览页的另一个 header 用它）
        check(getIconFontCodepoints().empty(), "没设图标字体时 getIconFontCodepoints() 是空表");
        const std::vector<unsigned int> scanned = scanFontCodepoints(switchIcons);
        check(scanned.size() > 100, "Switch 图标字体里有一百多个字形");
        check(scanned.front() >= 0xE000 && scanned.back() <= 0xF8FF, "扫出来的都在私有区");

        // U+E154 起是这份字体自带的占位字形（空框）→ "可用图标"列表要裁掉
        setIconFont(switchIcons);
        const std::vector<unsigned int>& usable = getIconFontCodepoints();
        check(!usable.empty(), "设了图标字体后能列出可用图标");
        check(usable.back() <= 0xE153, "U+E154 起的占位字形被裁掉（最后一个不超过 E153）");
        check(usable.size() < scanned.size(), "裁掉占位后比字体里的码位少");
        check(&getIconFontCodepoints() == &usable, "结果被缓存（重复取是同一个对象）");
    }

    check(std::string(icon::glyphByName("widgets")) != "", "按名字也能取字形");
    check(std::string(icon::glyphByName("不存在的图标")) == "", "名字不对返回空");
    check(std::string(icon::glyph(icon::Id::NONE)).empty(), "NONE 返回空串");

    std::printf("%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
