#pragma once

// bklv 自带的图标库（Material Icons）。
//
// 为什么放进库：图标是界面公共资源，不该由每个上层程序各自实现一遍。
// 字体文件：resources/font/MaterialIcons-Regular.ttf（随资源目录分发）。
//
// 用法：
//   bklv::icon::glyph(bklv::icon::Id::ADD)   // 取 UTF-8 字形，直接画/当文本用
//   bklv::icon::font()                       // 图标字体（没装字体时返回 nullptr）
//
// 注意：图标字体必须在**图集建立之前**装载（同 core/font.cpp 里的其它字体），
// 之后再 AddFontFromFileTTF 会崩 —— 这是设计器里踩过的坑。

#include <string>
#include <vector>

struct ImFont;

namespace bklv
{
namespace icon
{

// 图标 id。命名用 UPPER_SNAKE，与项目其它枚举一致。
enum class Id
{
    NONE,
    // 通用
    ADD, DELETE, EDIT, SAVE, REFRESH, SEARCH, SETTINGS, INFO, WARNING, ERROR, HELP,
    CLOSE, CHECK, MENU, MORE_VERT, HOME,
    // 文件与资源
    FOLDER_OPEN, CREATE_NEW_FOLDER, DESCRIPTION, LIST, TABLE_CHART, VIEW_LIST,
    CONTENT_COPY, CONTENT_PASTE, FILE_DOWNLOAD, FILE_UPLOAD,
    // 编辑
    UNDO, REDO, DRAG_INDICATOR, FORMAT_ALIGN_LEFT, FORMAT_COLOR_FILL, TEXT_FIELDS,
    // 视图与画布
    WIDGETS, LAYERS, LAYERS_CLEAR, TUNE, VISIBILITY, GRID_ON, REORDER, DASHBOARD,
    FULLSCREEN, FULLSCREEN_EXIT, ZOOM_IN, ZOOM_OUT, CENTER_FOCUS_STRONG, CROP_SQUARE,
    // 展示与媒体
    IMAGE, PALETTE, BRUSH, STAR, FAVORITE, PLAY_ARROW, PAUSE,
    // 导航
    CHEVRON_LEFT, CHEVRON_RIGHT, EXPAND_MORE, EXPAND_LESS, UNFOLD_MORE, UNFOLD_LESS,
};

struct Info
{
    Id id;                    // 图标 id
    const char* name;         // Material 里的名字（英文，便于对照官方图标表）
    const char* chineseName;  // 中文说明（界面里给用户看的）
    unsigned int codepoint;   // 私有区码位
};

// 全部图标（**手写的常用子集**，几十个；按名字/中文说明索引）
const std::vector<Info>& all();

// 字体里**实际存在**的一个字形：码位 + UTF-8 编码
struct Glyph
{
    unsigned int codepoint;
    std::string utf8;
};

// Material Icons 字体里实际存在的**全部**码位（按码位升序）。
// 上面那张表只是常用子集，字体里其实有一整套（上千个）—— "图标总览"这类页面要用这个。
// 需要字体已装载（`hasFont()`）；没装载返回空表。结果缓存，字体换了自动失效。
const std::vector<Glyph>& allGlyphs();

// 取 UTF-8 字形；id 不存在返回空串
const char* glyph(Id id);

// 码位 -> UTF-8 字形（Switch 图标那套只给码位，字形要自己拼）
std::string utf8Glyph(unsigned int codepoint);

// 按 Material 名字取字形（找不到返回空串）
const char* glyphByName(const char* name);

// 图标字体；没装载返回 nullptr
ImFont* font();
bool hasFont();

// 内部：由 core/font.cpp 在装载字体时调用（不要在上层调用）
void setFont(ImFont* font);

} // namespace icon
} // namespace bklv
