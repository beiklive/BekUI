// Material Icons 图标库的表检查（不需要窗口）
#include "bklv/core/icon_font.hpp"

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

    check(std::string(icon::glyphByName("widgets")) != "", "按名字也能取字形");
    check(std::string(icon::glyphByName("不存在的图标")) == "", "名字不对返回空");
    check(std::string(icon::glyph(icon::Id::NONE)).empty(), "NONE 返回空串");

    std::printf("%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
