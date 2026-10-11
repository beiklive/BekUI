#include "bklv/core/theme_data.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "bklv/core/logger.hpp"
#include "bklv/core/resource.hpp"

namespace bklv
{

namespace
{

using Json = nlohmann::json;

// 读一个主题下的文件（相对 resources/theme/）。失败返回空。
bool readText(const std::string& relativePath, std::string& out)
{
    const std::string full = resourcePath(relativePath);

    std::ifstream file(full, std::ios::binary);
    if (!file.is_open())
        return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

// 解析 JSON；失败时给出文件 + 行号（和 i18n / json_view 一样的定位方式）。
bool parseFile(const std::string& relativePath, Json& out)
{
    std::string text;
    if (!readText(relativePath, text))
    {
        BKLV_LOG_ERROR("主题文件打不开：%s", relativePath.c_str());
        return false;
    }

    try
    {
        out = Json::parse(text);
        return true;
    }
    catch (const Json::parse_error& e)
    {
        size_t line = 1;
        for (size_t i = 0; i < e.byte && i < text.size(); i++)
            if (text[i] == '\n')
                line++;

        BKLV_LOG_ERROR("主题文件语法错误：%s:%zu  %s", relativePath.c_str(), line, e.what());
        return false;
    }
}

// "#RRGGBB" / "#RRGGBBAA" -> ImU32
bool parseHex(const std::string& text, ImU32& out)
{
    if (text.size() != 7 && text.size() != 9)
        return false;
    if (text[0] != '#')
        return false;

    for (size_t i = 1; i < text.size(); i++)
        if (!std::isxdigit((unsigned char)text[i]))
            return false;

    const unsigned long value = std::strtoul(text.c_str() + 1, nullptr, 16);
    if (text.size() == 7)
        out = IM_COL32((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, 255);
    else
        out = IM_COL32((value >> 24) & 0xFF, (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
    return true;
}

// 把 colors.json / metrics.json 递归拍平成 "color.a.b" / "metric.a.b" 这样的点分层 key。
void flatten(const Json& node, const std::string& prefix, std::unordered_map<std::string, ImU32>& colors,
    std::unordered_map<std::string, float>& numbers)
{
    if (!node.is_object())
        return;

    for (auto it = node.begin(); it != node.end(); ++it)
    {
        // 以 $ 开头的是元信息（比如 $comment），跳过
        if (!it.key().empty() && it.key()[0] == '$')
            continue;

        const std::string key = prefix.empty() ? it.key() : prefix + "." + it.key();

        if (it.value().is_object())
        {
            flatten(it.value(), key, colors, numbers);
        }
        else if (it.value().is_string())
        {
            ImU32 color = 0;
            if (parseHex(it.value().get<std::string>(), color))
                colors[key] = color;
            else
                BKLV_LOG_WARNING("主题里的颜色不是 #RRGGBB[AA]：%s = \"%s\"", key.c_str(),
                    it.value().get<std::string>().c_str());
        }
        else if (it.value().is_number())
        {
            numbers[key] = it.value().get<float>();
        }
    }
}

} // namespace

bool ThemeData::loadInto(const std::string& themeName, bool isDefault)
{
    const std::string dir = std::string(THEME_DIR) + themeName + "/";

    Json meta;
    if (!parseFile(dir + "theme.json", meta))
        return false;

    // theme.json 里列出本主题包含哪些文件——**不扫目录**
    std::vector<std::string> widgetFiles;
    if (meta.contains("widgets") && meta["widgets"].is_array())
        for (const Json& entry : meta["widgets"])
            if (entry.is_string())
                widgetFiles.push_back(entry.get<std::string>());

    auto& targetColors = isDefault ? this->defaultColors : this->colors;
    auto& targetNumbers = isDefault ? this->defaultNumbers : this->numbers;
    auto& targetWidgets = isDefault ? this->defaultWidgetJson : this->widgetJson;

    targetColors.clear();
    targetNumbers.clear();
    targetWidgets.clear();

    for (const char* file : { "colors.json", "metrics.json" })
    {
        Json doc;
        const std::string path = dir + file;
        if (!parseFile(path, doc))
        {
            if (!isDefault)
                BKLV_LOG_WARNING("主题 %s 缺少 %s，将回退到 default", themeName.c_str(), file);
            continue;
        }
        const std::string prefix = (std::string(file) == "colors.json") ? "color" : "metric";
        flatten(doc, prefix, targetColors, targetNumbers);
    }

    for (const std::string& relative : widgetFiles)
    {
        std::string text;
        const std::string path = dir + relative;
        if (!readText(path, text))
        {
            if (!isDefault)
                BKLV_LOG_WARNING("主题 %s 缺少 %s，该控件外观将回退到 default", themeName.c_str(),
                    relative.c_str());
            continue;
        }

        // 记成 "button" 这样的短名（文件名去掉目录与扩展名）
        std::string name = relative;
        const size_t slash = name.find_last_of('/');
        if (slash != std::string::npos)
            name = name.substr(slash + 1);
        const size_t dot = name.find_last_of('.');
        if (dot != std::string::npos)
            name = name.substr(0, dot);

        targetWidgets[name] = std::move(text);
    }

    return true;
}

bool ThemeData::load(const std::string& requested)
{
    // 1. 清单：可用主题与默认主题
    Json manifest;
    if (parseFile(std::string(THEME_DIR) + "themes.json", manifest))
    {
        this->available.clear();
        if (manifest.contains("available") && manifest["available"].is_array())
            for (const Json& entry : manifest["available"])
                if (entry.is_string())
                    this->available.push_back(entry.get<std::string>());

        if (requested.empty() && manifest.contains("default") && manifest["default"].is_string())
            this->name = manifest["default"].get<std::string>();
        else
            this->name = requested;
    }
    else
    {
        // 没有清单也能用：直接按名字找目录
        this->name = requested.empty() ? "default" : requested;
    }

    if (this->name.empty())
        this->name = "default";

    // 2. default 永远先装载（它是回退目标）
    const bool defaultOk = this->loadInto("default", true);
    if (!defaultOk)
        BKLV_LOG_ERROR("default 主题装载失败（%sdefault/theme.json）", THEME_DIR);

    if (this->name == "default")
    {
        // 当前主题就是 default：直接指向同一份数据
        this->colors        = this->defaultColors;
        this->numbers       = this->defaultNumbers;
        this->widgetJson    = this->defaultWidgetJson;
        BKLV_LOG_INFO("主题已装载：default（%d 色 / %d 数值 / %d 控件外观）", this->getColorCount(),
            this->getNumberCount(), (int)this->widgetJson.size());
        return defaultOk;
    }

    if (!this->loadInto(this->name, false))
    {
        BKLV_LOG_WARNING("主题 %s 装载失败，回退到 default", this->name.c_str());
        this->name       = "default";
        this->colors     = this->defaultColors;
        this->numbers    = this->defaultNumbers;
        this->widgetJson = this->defaultWidgetJson;
        return false;
    }

    BKLV_LOG_INFO("主题已装载：%s（%d 色 / %d 数值 / %d 控件外观；缺项回退 default）", this->name.c_str(),
        this->getColorCount(), this->getNumberCount(), (int)this->widgetJson.size());
    return true;
}

bool ThemeData::hasColor(const std::string& key) const
{
    return this->colors.find(key) != this->colors.end();
}

bool ThemeData::hasNumber(const std::string& key) const
{
    return this->numbers.find(key) != this->numbers.end();
}

ImU32 ThemeData::color(const std::string& key, ImU32 fallback) const
{
    const auto it = this->colors.find(key);
    if (it != this->colors.end())
        return it->second;

    const auto fallbackIt = this->defaultColors.find(key);
    if (fallbackIt != this->defaultColors.end())
        return fallbackIt->second;

    BKLV_LOG_DEBUG("主题色缺失：%s", key.c_str());
    return fallback;
}

float ThemeData::number(const std::string& key, float fallback) const
{
    const auto it = this->numbers.find(key);
    if (it != this->numbers.end())
        return it->second;

    const auto fallbackIt = this->defaultNumbers.find(key);
    if (fallbackIt != this->defaultNumbers.end())
        return fallbackIt->second;

    BKLV_LOG_DEBUG("主题数值缺失：%s", key.c_str());
    return fallback;
}

const std::string* ThemeData::widgetAppearanceJson(const std::string& widget) const
{
    const auto it = this->widgetJson.find(widget);
    if (it != this->widgetJson.end())
        return &it->second;

    const auto fallbackIt = this->defaultWidgetJson.find(widget);
    if (fallbackIt != this->defaultWidgetJson.end())
        return &fallbackIt->second;

    return nullptr;
}

namespace
{

// 单例：App 构造时装 default，setTheme 换。
ThemeData& mutableTheme()
{
    static ThemeData theme;
    return theme;
}

bool& themeLoaded()
{
    static bool loaded = false;
    return loaded;
}

} // namespace

const ThemeData& getTheme()
{
    if (!themeLoaded())
    {
        themeLoaded() = true;
        mutableTheme().load("");
    }
    return mutableTheme();
}

bool loadThemeIntoGlobal(const std::string& name)
{
    ThemeData candidate;
    const bool ok = candidate.load(name);
    if (!ok && candidate.getName() != name)
        return false; // 目录都没有，保持原主题

    mutableTheme() = candidate;
    themeLoaded()  = true;
    return true;
}

} // namespace bklv
