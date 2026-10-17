#include "designer/core/name_table.hpp"

#include <unordered_map>
#include <vector>

namespace designer
{

namespace
{

struct Entry
{
    const char* key;
    const char* name;
};

// 只收"新手看不懂"的词；`box` 这类直白的也一并译，保持一致
const Entry kSegments[] = {
    { "box", "外框" }, { "component", "组件" }, { "layout", "布局" },
    { "button", "按钮" }, { "bordered", "带边框" }, { "borderless", "无边框" },
    { "ghost", "幽灵按钮" }, { "highlight", "强调字按钮" }, { "primary", "主按钮" },
    { "text", "文字" }, { "icon", "图标" }, { "icon_text", "图标加文字" },
    { "info", "信息" }, { "switch", "开关" }, { "switch_text", "开关（文字）" },
    { "switch_icon", "开关（图标）" }, { "dropdown", "下拉" }, { "dropdown_item", "下拉选项" },
    { "dropdown_popup", "下拉面板" }, { "selector", "选择器" }, { "slider", "滑块" },
    { "cell", "列表项" }, { "bool", "开关" }, { "detail", "详情" }, { "input", "输入" },
    { "radio", "单选" }, { "selector_cell", "选择项" }, { "slider_cell", "滑块项" },
    { "header", "页头" }, { "header_icon", "页头图标" }, { "sidebar", "侧边栏" },
    { "tab", "标签" }, { "tab_cell", "标签项" }, { "hints", "提示" }, { "hint", "提示" },
    { "hint_item", "提示项" }, { "status", "状态" }, { "area", "区域" },
    { "bottom", "底部" }, { "bar", "栏" }, { "image", "图片" }, { "label", "文字" },
    { "progress", "进度" }, { "spinner", "转圈" }, { "padding", "占位" }, { "row", "横排" },
    { "glass", "玻璃" }, { "panel", "面板" }, { "surface", "表面" }, { "chrome", "浏览器栏" },
    { "dialog", "对话框" }, { "backdrop", "遮罩" }, { "divider", "分隔线" },
    { "line", "线" }, { "item", "条目" }, { "list", "列表" }, { "menu", "菜单" },
    { "tooltip", "气泡提示" }, { "toast", "轻提示" }, { "badge", "徽标" },
    { "card", "卡片" }, { "avatar", "头像" }, { "page_json", "演示页" },
    { "scrolling", "滚动" }, { "frame", "框" }, { "applet", "小程序页" },
    { "colors", "调色板" }, { "metrics", "尺寸与间距" },
    // 组成部分的 id（外观里的层名）
    { "bg", "底色" }, { "hlBg", "高亮底" }, { "hlGlow", "高亮光晕" },
    { "hlBorder", "高亮边框" }, { "knob", "滑块" }, { "track", "轨道" },
    { "fill", "填充" }, { "stroke", "描边" }, { "glow", "光晕" },
    { "shadow", "阴影" }, { "border", "边框" }, { "tint", "染色" },
};

const Entry kHints[] = {
    { "bordered", "只有边框、没有底色" }, { "borderless", "没有边框也没有底色" },
    { "primary", "页面里最主要的那个按钮" }, { "ghost", "更淡的按钮，放次要操作" },
    { "highlight", "灰底、强调色文字" }, { "icon", "只有图标的按钮" },
    { "icon_text", "图标加文字的按钮" }, { "glass", "半透明磨砂效果" },
    { "colors", "换了这里，所有用到它的地方一起变" },
    { "metrics", "尺寸与间距的公共数值" },
};

const Entry kOps[] = {
    { "rect", "底色块" }, { "text", "文字" }, { "image", "图片" },
    { "shadow", "阴影" }, { "glass", "玻璃" }, { "custom", "自定义绘制" },
};

const char* lookup(const Entry* table, size_t count, const std::string& key)
{
    for (size_t i = 0; i < count; i++)
        if (key == table[i].key)
            return table[i].name;
    return nullptr;
}

std::vector<std::string> split(const std::string& text, char separator)
{
    std::vector<std::string> parts;
    std::string              current;
    for (char c : text)
    {
        if (c == separator)
        {
            parts.push_back(current);
            current.clear();
        }
        else
            current.push_back(c);
    }
    parts.push_back(current);
    return parts;
}

} // namespace

std::string segmentName(const std::string& segment)
{
    const char* name = lookup(kSegments, sizeof(kSegments) / sizeof(kSegments[0]), segment);
    return name != nullptr ? name : std::string();
}

std::string fileTitle(const std::string& relPath)
{
    // 取最后一段（去掉 base/ widget/ 前缀），再按 `.` 逐段翻译
    std::string base = relPath;
    const size_t slash = base.rfind('/');
    if (slash != std::string::npos)
        base = base.substr(slash + 1);

    std::string out;
    std::string normalized = base;
    for (char& c : normalized)
        if (c == '_')
            c = '.'; // `cell_bool` 与 `button.bordered` 一样按段翻
    for (const std::string& part : split(normalized, '.'))
    {
        const std::string name = segmentName(part);
        if (!out.empty())
            out += " · ";
        out += name.empty() ? part : name;
    }
    return out;
}

std::string fileHint(const std::string& relPath)
{
    std::string base = relPath;
    const size_t slash = base.rfind('/');
    if (slash != std::string::npos)
        base = base.substr(slash + 1);

    // 优先用最后一段（变体名最有信息量），再退回整名
    std::string normalized = base;
    for (char& c : normalized)
        if (c == '_')
            c = '.';
    const std::vector<std::string> parts = split(normalized, '.');
    for (auto it = parts.rbegin(); it != parts.rend(); ++it)
    {
        const char* hint = lookup(kHints, sizeof(kHints) / sizeof(kHints[0]), *it);
        if (hint != nullptr)
            return hint;
    }
    return std::string();
}

std::string opTitle(const std::string& op)
{
    const char* name = lookup(kOps, sizeof(kOps) / sizeof(kOps[0]), op);
    return name != nullptr ? name : op;
}

const char* layerConceptHint()
{
    return "一个外观由这几个「组成部分」叠出来：先画底，再放内容，最后盖高亮。"
           "上下拖动可以调整叠放顺序。";
}

} // namespace designer
