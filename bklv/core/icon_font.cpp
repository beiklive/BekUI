#include "bklv/core/icon_font.hpp"

#include "bklv/core/font.hpp"

#include <map>

namespace bklv
{
namespace icon
{

namespace
{

ImFont* g_font = nullptr;

// UTF-8 编码（图标码位都在 BMP 私有区，3 字节够用）
std::string encodeUtf8(unsigned int codepoint)
{
    std::string out;
    if (codepoint < 0x80)
        out.push_back((char)codepoint);
    else if (codepoint < 0x800)
    {
        out.push_back((char)(0xC0 | (codepoint >> 6)));
        out.push_back((char)(0x80 | (codepoint & 0x3F)));
    }
    else if (codepoint < 0x10000)
    {
        out.push_back((char)(0xE0 | (codepoint >> 12)));
        out.push_back((char)(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (codepoint & 0x3F)));
    }
    else
    {
        out.push_back((char)(0xF0 | (codepoint >> 18)));
        out.push_back((char)(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back((char)(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (codepoint & 0x3F)));
    }
    return out;
}

// 码位表：name 用 Material Icons 官方名字，便于对照官方图标表核对
const Info kIcons[] = {
    { Id::NONE, "none", "无", 0x0000 },
    { Id::ADD, "add", "增加", 0xE145 },
    { Id::DELETE, "delete", "删除", 0xE872 },
    { Id::EDIT, "edit", "编辑", 0xE3C9 },
    { Id::SAVE, "save", "保存", 0xE161 },
    { Id::REFRESH, "refresh", "刷新", 0xE5D5 },
    { Id::SEARCH, "search", "搜索", 0xE8B6 },
    { Id::SETTINGS, "settings", "设置", 0xE8B8 },
    { Id::INFO, "info", "信息", 0xE88E },
    { Id::WARNING, "warning", "警告", 0xE002 },
    { Id::ERROR, "error", "错误", 0xE000 },
    { Id::HELP, "help", "帮助", 0xE887 },
    { Id::CLOSE, "close", "关闭", 0xE5CD },
    { Id::CHECK, "check", "完成", 0xE5CA },
    { Id::MENU, "menu", "菜单", 0xE5D2 },
    { Id::MORE_VERT, "more_vert", "更多", 0xE5D4 },
    { Id::HOME, "home", "主页", 0xE88A },
    { Id::FOLDER_OPEN, "folder_open", "打开文件夹", 0xE2C8 },
    { Id::CREATE_NEW_FOLDER, "create_new_folder", "新建文件夹", 0xE2CC },
    { Id::DESCRIPTION, "description", "页面/文档", 0xE873 },
    { Id::LIST, "list", "列表", 0xE896 },
    { Id::TABLE_CHART, "table_chart", "主题表", 0xE265 },
    { Id::VIEW_LIST, "view_list", "清单", 0xE8EF },
    { Id::CONTENT_COPY, "content_copy", "复制", 0xE14D },
    { Id::CONTENT_PASTE, "content_paste", "粘贴", 0xE14F },
    { Id::FILE_DOWNLOAD, "file_download", "下载", 0xE2C4 },
    { Id::FILE_UPLOAD, "file_upload", "上传", 0xE2C6 },
    { Id::UNDO, "undo", "撤销", 0xE166 },
    { Id::REDO, "redo", "重做", 0xE15A },
    { Id::DRAG_INDICATOR, "drag_indicator", "拖动", 0xE945 },
    { Id::FORMAT_ALIGN_LEFT, "format_align_left", "左对齐", 0xE236 },
    { Id::FORMAT_COLOR_FILL, "format_color_fill", "填充颜色", 0xE23A },
    { Id::TEXT_FIELDS, "text_fields", "文字", 0xE262 },
    { Id::WIDGETS, "widgets", "控件", 0xE1BD },
    { Id::LAYERS, "layers", "层", 0xE53B },
    { Id::LAYERS_CLEAR, "layers_clear", "隐藏层", 0xE53C },
    { Id::TUNE, "tune", "调整", 0xE429 },
    { Id::VISIBILITY, "visibility", "可见", 0xE8F4 },
    { Id::GRID_ON, "grid_on", "网格", 0xE3EC },
    { Id::REORDER, "reorder", "排序", 0xE8FE },
    { Id::DASHBOARD, "dashboard", "总览", 0xE871 },
    { Id::FULLSCREEN, "fullscreen", "最大化", 0xE5D0 },
    { Id::FULLSCREEN_EXIT, "fullscreen_exit", "退出最大化", 0xE5D1 },
    { Id::ZOOM_IN, "zoom_in", "放大", 0xE8FF },
    { Id::ZOOM_OUT, "zoom_out", "缩小", 0xE900 },
    { Id::CENTER_FOCUS_STRONG, "center_focus_strong", "居中", 0xE3B4 },
    { Id::CROP_SQUARE, "crop_square", "矩形", 0xE3C6 },
    { Id::IMAGE, "image", "图片", 0xE3F4 },
    { Id::PALETTE, "palette", "配色", 0xE40A },
    { Id::BRUSH, "brush", "画笔", 0xE3AE },
    { Id::STAR, "star", "星标", 0xE838 },
    { Id::FAVORITE, "favorite", "喜欢", 0xE87D },
    { Id::PLAY_ARROW, "play_arrow", "播放", 0xE037 },
    { Id::PAUSE, "pause", "暂停", 0xE034 },
    { Id::CHEVRON_LEFT, "chevron_left", "左", 0xE5CB },
    { Id::CHEVRON_RIGHT, "chevron_right", "右", 0xE5CC },
    { Id::EXPAND_MORE, "expand_more", "展开", 0xE5CF },
    { Id::EXPAND_LESS, "expand_less", "收起", 0xE5CE },
    { Id::UNFOLD_MORE, "unfold_more", "展开全部", 0xE5D7 },
    { Id::UNFOLD_LESS, "unfold_less", "全部收起", 0xE5D6 },
};

std::vector<Info> g_all(kIcons, kIcons + sizeof(kIcons) / sizeof(kIcons[0]));
std::map<std::string, std::string> g_glyphs; // id/name → UTF-8

const std::string kEmpty;

} // namespace

const std::vector<Info>& all()
{
    return g_all;
}

// 码位 -> UTF-8（图标都在私有区，3 字节；写成通用的以防以后加更大的码位）
std::string utf8Glyph(unsigned int cp)
{
    std::string out;
    if (cp < 0x80)
    {
        out += static_cast<char>(cp);
    }
    else if (cp < 0x800)
    {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else if (cp < 0x10000)
    {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else
    {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

const std::vector<Glyph>& allGlyphs()
{
    // 缓存：字体或字体版本变了才重扫（扫一遍是几千次字体查询，不值得每帧做）
    static std::vector<Glyph> cached;
    static ImFont* cachedFont     = nullptr;
    static unsigned int cachedGen = 0;

    const unsigned int generation = getFontGeneration();
    ImFont* current               = font();

    if (cachedFont != current || cachedGen != generation)
    {
        cached.clear();
        // 手写表里的图标也在字体里，不去重 —— 这里就是"字体有的全列出来"
        for (unsigned int cp : scanFontCodepoints(current))
        {
            Glyph glyph;
            glyph.codepoint = cp;
            glyph.utf8      = utf8Glyph(cp);
            cached.push_back(std::move(glyph));
        }

        cachedFont = current;
        cachedGen  = generation;
    }

    return cached;
}

const char* glyph(Id id)
{
    for (const Info& info : g_all)
        if (info.id == id)
            return glyphByName(info.name);
    return kEmpty.c_str();
}

const char* glyphByName(const char* name)
{
    if (name == nullptr)
        return kEmpty.c_str();

    for (const Info& info : g_all)
    {
        if (std::string(info.name) == name)
        {
            const auto found = g_glyphs.find(info.name);
            if (found != g_glyphs.end())
                return found->second.c_str();

            const std::string encoded = encodeUtf8(info.codepoint);
            return g_glyphs.emplace(info.name, encoded).first->second.c_str();
        }
    }
    return kEmpty.c_str();
}

ImFont* font() { return g_font; }
bool hasFont() { return g_font != nullptr; }
void setFont(ImFont* font) { g_font = font; }

} // namespace icon
} // namespace bklv
