#include "bklv/core/appearance.hpp"
#include "bklv/core/theme_data.hpp"

#include <algorithm>

#include "bklv/core/ui_scale.hpp"

#include <cctype>
#include <set>
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

// 主题缺键只警告一次（同一个 key 反复查也不刷屏）
void warnMissingKeyOnce(const std::string& key, const char* kind)
{
    static std::set<std::string> warned;
    if (!warned.insert(key).second)
        return;

    // 现场信息要够定位：光说"缺 key"看不出是"主题还没装载"还是"调用方把 key 传错了"
    // （曾经靠这条日志排查了半天，见 AGENTS 的"缺键不能静默"）
    const ThemeData& theme = getTheme();
    BKLV_LOG_WARNING("主题%s缺失：%s（当前主题 %s；已装载 %d 色 / 默认 %d 色；"
                     "外观 JSON 里引用不存在的 key 会直接编译失败，C++ 里取值会退化成 fallback）",
        kind, key.c_str(), theme.getName().c_str(), theme.getColorCount(),
        (int)theme.getDefaultColorCount());
    // 临时诊断：把装载进来的同前缀 key 打出来，看清到底存的是什么名字
    {
        const std::string prefix = key.substr(0, key.find('.') + 1);
        std::string nearby;
        int shown = 0;
        for (const auto& entry : theme.getColorKeys())
            if (entry.rfind(prefix, 0) == 0 && shown++ < 6)
                nearby += entry + "  ";
        BKLV_LOG_WARNING("  ↑ 装载进来的 %s* 有：%s", prefix.c_str(), nearby.c_str());
    }
    if (theme.getColorCount() == 0 && theme.getDefaultColorCount() == 0)
        BKLV_LOG_WARNING("  ↑ 两个颜色表都是空的：说明这是**主题装载之前**的取值，"
                         "不是 key 拼错");
}

} // namespace

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

// 解析一段已经在内存里的 JSON 文本（origin 只用于日志定位）
bool parseJsonText(const std::string& text, const std::string& origin, Json& out)
{
    try
    {
        out = Json::parse(text);
        return true;
    }
    catch (const Json::parse_error& e)
    {
        BKLV_LOG_ERROR("主题文件解析失败：%s（第 %zu 字节）", origin.c_str(), (size_t)e.byte);
        return false;
    }
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
    return this->loadIntoDirectory(std::string(THEME_DIR) + themeName + "/", themeName, isDefault);
}

bool ThemeData::loadFromDirectory(const std::string& themeDir, const std::string& themeName)
{
    // default 主题永远来自程序自己的 resources（缺项回退用它），本体按传入目录装
    if (themeName != "default")
        this->loadInto("default", true);

    return this->loadIntoDirectory(themeDir.empty() ? std::string() : themeDir + "/", themeName, false);
}

bool ThemeData::loadIntoDirectory(const std::string& dir, const std::string& themeName,
    bool isDefault)
{

    Json meta;
    if (!parseFile(dir + "theme.json", meta))
        return false;

    // 元信息：设计器"打开项目"读它
    if (!isDefault)
    {
        auto readString = [&meta](const char* key) {
            return meta.contains(key) && meta[key].is_string() ? meta[key].get<std::string>()
                                                               : std::string();
        };
        this->author         = readString("author");
        this->description    = readString("description");
        this->descriptionKey = readString("descriptionKey");
    }

    // theme.json 里列出本主题包含哪些文件——**不扫目录**（romfs 上目录迭代未验证）
    std::vector<std::string> listed;
    if (meta.contains("files") && meta["files"].is_array())
    {
        for (const Json& entry : meta["files"])
            if (entry.is_string())
                listed.push_back(entry.get<std::string>());
    }
    else if (meta.contains("widgets") && meta["widgets"].is_array())
    {
        // 兼容旧布局（widgets/ 目录 + widgets[] 清单）：能读，但要明确让人去迁
        BKLV_LOG_WARNING("主题 %s 用的是旧布局 widgets[]：请改成 files[]，"
                         "并把外观挪进 base/ 或 widget/、色板数值挪进 tokens/",
            themeName.c_str());
        for (const Json& entry : meta["widgets"])
            if (entry.is_string())
                listed.push_back("widgets/" + entry.get<std::string>());
    }

    auto& targetColors = isDefault ? this->defaultColors : this->colors;
    auto& targetNumbers = isDefault ? this->defaultNumbers : this->numbers;
    auto& targetWidgets = isDefault ? this->defaultWidgetJson : this->widgetJson;
    auto& targetViews   = isDefault ? this->defaultViewJson : this->viewJsonMap;

    targetColors.clear();
    targetNumbers.clear();
    targetWidgets.clear();
    targetViews.clear();

    // 名字 = 相对主题根的路径去掉扩展名（base/button.json -> "base/button"）
    auto nameFromPath = [](const std::string& relative) {
        std::string name = relative;
        const size_t dot = name.find_last_of('.');
        if (dot != std::string::npos)
            name = name.substr(0, dot);
        return name;
    };

    bool colorsLoaded  = false;
    bool metricsLoaded = false;

    for (const std::string& relative : listed)
    {
        std::string text;
        if (!readText(dir + relative, text))
        {
            if (!isDefault)
                BKLV_LOG_WARNING("主题 %s 缺少清单里的 %s，该项回退到 default", themeName.c_str(),
                    relative.c_str());
            continue;
        }

        // tokens/*.json：色板与数值（老主题写在主题根下，两个名字都认）
        const bool isColors  = relative == "tokens/colors.json" || relative == "colors.json";
        const bool isMetrics = relative == "tokens/metrics.json" || relative == "metrics.json";
        if (isColors || isMetrics)
        {
            Json doc;
            if (!parseJsonText(text, relative, doc))
                continue;
            flatten(doc, isColors ? "color" : "metric", targetColors, targetNumbers);
            (isColors ? colorsLoaded : metricsLoaded) = true;
            continue;
        }

        // view/*.json：页面/视图描述（结构 + 外观），交给 core/json_view.* 实例化
        if (relative.rfind("view/", 0) == 0)
        {
            targetViews[nameFromPath(relative)] = std::move(text);
            continue;
        }

        // 其余（base/*、widget/*、以及自定义目录）都是控件外观
        targetWidgets[nameFromPath(relative)] = std::move(text);
    }

    // 清单里没写 tokens 的主题：按标准路径兜底读一次，尽量不让人白写一个主题
    for (const char* fallback : { "tokens/colors.json", "colors.json" })
    {
        if (colorsLoaded)
            break;
        Json doc;
        if (!parseFile(dir + fallback, doc))
            continue;
        flatten(doc, "color", targetColors, targetNumbers);
        colorsLoaded = true;
    }
    for (const char* fallback : { "tokens/metrics.json", "metrics.json" })
    {
        if (metricsLoaded)
            break;
        Json doc;
        if (!parseFile(dir + fallback, doc))
            continue;
        flatten(doc, "metric", targetColors, targetNumbers);
        metricsLoaded = true;
    }

    if (!isDefault && (!colorsLoaded || !metricsLoaded))
        BKLV_LOG_WARNING("主题 %s 缺少 %s，将回退到 default", themeName.c_str(),
            !colorsLoaded ? "colors.json" : "metrics.json");

    return true;
}

bool ThemeData::load(const std::string& requested)
{
    // 环境变量优先：桌面调试与验证用（BKLV_THEME=ocean ./dist/demo）
    std::string wanted = requested;
    if (wanted.empty())
    {
        if (const char* env = std::getenv("BKLV_THEME"); env != nullptr && *env != '\0')
            wanted = env;
    }

    // 1. 清单：可用主题与默认主题
    Json manifest;
    if (parseFile(std::string(THEME_DIR) + "themes.json", manifest))
    {
        this->available.clear();
        if (manifest.contains("available") && manifest["available"].is_array())
            for (const Json& entry : manifest["available"])
                if (entry.is_string())
                    this->available.push_back(entry.get<std::string>());

        if (wanted.empty() && manifest.contains("default") && manifest["default"].is_string())
            this->name = manifest["default"].get<std::string>();
        else
            this->name = wanted;
    }
    else
    {
        // 没有清单也能用：直接按名字找目录
        this->name = wanted.empty() ? "default" : wanted;
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
    return this->colors.find(key) != this->colors.end()
        || this->defaultColors.find(key) != this->defaultColors.end();
}

bool ThemeData::hasNumber(const std::string& key) const
{
    return this->numbers.find(key) != this->numbers.end()
        || this->defaultNumbers.find(key) != this->defaultNumbers.end();
}

ImU32 ThemeData::color(const std::string& key, ImU32 fallback) const
{
    const auto it = this->colors.find(key);
    if (it != this->colors.end())
        return it->second;

    const auto fallbackIt = this->defaultColors.find(key);
    if (fallbackIt != this->defaultColors.end())
        return fallbackIt->second;

    // 缺键不能静默：以前是 DEBUG 级，界面上就画成 fallback（常见是品红），
    // 排查半天。同一个 key 只报一次，避免每帧刷屏。
    warnMissingKeyOnce(key, "颜色");
    return fallback;
}

// 主题里的尺寸都是按 1280x720 写的，取值时乘当前 UI 缩放。
// 例外：metric.anim.*（毫秒）与 metric.font.lineHeight（行高系数）不是像素量。
static bool isScalableMetricKey(const std::string& key)
{
    if (key.rfind("metric.", 0) != 0)
        return false;
    if (key.rfind("metric.anim.", 0) == 0)
        return false;
    if (key == "metric.font.lineHeight")
        return false;

    return true;
}

float ThemeData::number(const std::string& key, float fallback) const
{
    const auto it = this->numbers.find(key);
    if (it != this->numbers.end())
        return isScalableMetricKey(key) ? it->second * getUiScale() : it->second;

    const auto fallbackIt = this->defaultNumbers.find(key);
    if (fallbackIt != this->defaultNumbers.end())
        return isScalableMetricKey(key) ? fallbackIt->second * getUiScale() : fallbackIt->second;

    // key 两处都没有：退化成调用方给的兜底值。
    //
    // 兜底值也要按同一套缩放规则处理 —— 调用方写的是"按 720p 想的那个数"
    // （`metric.x` 一律如此），而这条路径原来直接 `return fallback`、**不乘缩放**，
    // 于是"主题里没这个 key"和"主题里有"在非 720p 画布上表现不一致。
    // 非像素量的 key（metric.anim.* / font.lineHeight / 不带 metric. 前缀的）不缩放。
    warnMissingKeyOnce(key, "数值");
    return isScalableMetricKey(key) ? fallback * getUiScale() : fallback;
}

std::vector<std::string> ThemeData::getColorKeys() const
{
    std::vector<std::string> keys;
    keys.reserve(this->colors.size());
    for (const auto& entry : this->colors)
        keys.push_back(entry.first);
    std::sort(keys.begin(), keys.end());
    return keys;
}

std::vector<std::string> ThemeData::getNumberKeys() const
{
    std::vector<std::string> keys;
    keys.reserve(this->numbers.size());
    for (const auto& entry : this->numbers)
        keys.push_back(entry.first);
    std::sort(keys.begin(), keys.end());
    return keys;
}

std::vector<std::string> ThemeData::getWidgetNames() const
{
    // 同上：并上 default 主题的外观（皮肤只覆盖一部分是常态）
    std::vector<std::string> names;
    names.reserve(this->widgetJson.size() + this->defaultWidgetJson.size());
    for (const auto& entry : this->widgetJson)
        names.push_back(entry.first);
    for (const auto& entry : this->defaultWidgetJson)
        if (this->widgetJson.find(entry.first) == this->widgetJson.end())
            names.push_back(entry.first);
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

const std::string* ThemeData::viewJson(const std::string& name) const
{
    const auto it = this->viewJsonMap.find(name);
    if (it != this->viewJsonMap.end())
        return &it->second;

    const auto fallbackIt = this->defaultViewJson.find(name);
    if (fallbackIt != this->defaultViewJson.end())
        return &fallbackIt->second;

    return nullptr;
}

std::vector<std::string> ThemeData::getViewNames() const
{
    // 当前主题 + default 主题的并集：查 viewJson() 时也会回退到 default，
    // 只列当前主题的话，皮肤里没覆盖的视图就"查得到但列不出来"。
    std::vector<std::string> names;
    names.reserve(this->viewJsonMap.size() + this->defaultViewJson.size());
    for (const auto& entry : this->viewJsonMap)
        names.push_back(entry.first);
    for (const auto& entry : this->defaultViewJson)
        if (this->viewJsonMap.find(entry.first) == this->viewJsonMap.end())
            names.push_back(entry.first);
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
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
    clearAppearanceCache();
    return true;
}

bool loadThemeIntoGlobalFromDirectory(const std::string& themeDir, const std::string& themeName)
{
    ThemeData candidate;
    const bool ok = candidate.loadFromDirectory(themeDir, themeName);
    if (!ok && candidate.getName() != themeName)
        return false;

    mutableTheme() = candidate;
    themeLoaded()  = true;
    return true;
}

} // namespace bklv
