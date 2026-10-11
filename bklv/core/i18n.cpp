#include "bklv/core/i18n.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "bklv/core/logger.hpp"
#include "bklv/core/resource.hpp"

#if defined(BKLV_SWITCH)
#include <switch.h>
#else
// SDL_GetPreferredLocales 是纯查询，不需要先 SDL_Init；
// 这点很关键——文案要在视图树构建前就绪，那时平台实例还不存在。
#include <SDL3/SDL.h>
#endif

namespace bklv
{

namespace
{

constexpr const char* DEFAULT_LOCALE = "en-US";
constexpr const char* I18N_DIR       = "resources/i18n/";

nlohmann::json g_current; // 当前语言
nlohmann::json g_default; // 默认语言，缺 key 时回退到这里
std::string g_locale;
std::string g_defaultLocale = DEFAULT_LOCALE;

// 查找缓存：key -> 命中节点。命中失败也缓存（存 nullptr），
// 避免同一个缺失 key 每帧重复做字符串切分与树遍历。
// 存的是 g_current / g_default 内部的指针，setLocale() 换掉 json 后必须整体清空。
std::unordered_map<std::string, const nlohmann::json*> g_lookupCache;

// 平台原始语言串。不依赖 Platform 实例（建树时平台还不存在）。
std::string probeSystemLocale()
{
#if defined(BKLV_SWITCH)
    // 与 PlatformSwitch::getLocale() 同一套映射，但可以在平台创建前调用。
    if (R_FAILED(setInitialize()))
        return DEFAULT_LOCALE;

    u64 languageCode = 0;
    if (R_FAILED(setGetSystemLanguage(&languageCode)))
        return DEFAULT_LOCALE;

    SetLanguage language = SetLanguage_ENUS;
    if (R_FAILED(setMakeLanguage(languageCode, &language)))
        return DEFAULT_LOCALE;

    switch (language)
    {
        case SetLanguage_JA: return "ja-JP";
        case SetLanguage_ENGB: return "en-GB";
        case SetLanguage_FR: return "fr-FR";
        case SetLanguage_DE: return "de-DE";
        case SetLanguage_ES: return "es-ES";
        case SetLanguage_ZHCN: return "zh-Hans";
        case SetLanguage_ZHTW: return "zh-Hant";
        case SetLanguage_KO: return "ko-KR";
        default: return DEFAULT_LOCALE;
    }
#else
    int count = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&count);
    if (locales == nullptr)
        return DEFAULT_LOCALE;

    std::string raw = DEFAULT_LOCALE;
    if (count > 0 && locales[0] != nullptr && locales[0]->language != nullptr)
    {
        raw = locales[0]->language;
        if (locales[0]->country != nullptr)
        {
            raw += "-";
            raw += locales[0]->country;
        }
    }
    SDL_free(locales);
    return raw;
#endif
}

// 读一个文案文件；失败返回 discarded 的 json。
nlohmann::json readLocaleFile(const std::string& locale)
{
    const std::string path = resourcePath(std::string(I18N_DIR) + locale + ".json");

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        return nlohmann::json::parse("", nullptr, false); // discarded

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string content = buffer.str();

    try
    {
        return nlohmann::json::parse(content);
    }
    catch (const nlohmann::json::parse_error& e)
    {
        // parse_error.byte 是字节偏移，折算成行号便于直接定位文件里哪一行。
        size_t line = 1;
        for (size_t i = 0; i < e.byte && i < content.size(); i++)
            if (content[i] == '\n')
                line++;

        BKLV_LOG_ERROR("文案文件语法错误：%s:%zu  %s", path.c_str(), line, e.what());
        return nlohmann::json::parse("", nullptr, false);
    }
}

// 装载一个语言：主文件 + 可选 `$files` 清单里的补充文件（后者覆盖前者）。
nlohmann::json loadLocale(const std::string& locale)
{
    nlohmann::json merged = readLocaleFile(locale);
    if (merged.is_discarded())
        return merged;

    if (!merged.contains("$files") || !merged["$files"].is_array())
        return merged;

    for (const auto& entry : merged["$files"])
    {
        if (!entry.is_string())
            continue;

        nlohmann::json extra = readLocaleFile(entry.get<std::string>());
        if (extra.is_discarded() || !extra.is_object())
        {
            BKLV_LOG_WARNING("文案补充文件无效：%s", entry.get<std::string>().c_str());
            continue;
        }
        for (auto it = extra.begin(); it != extra.end(); ++it)
            merged[it.key()] = it.value();
    }
    return merged;
}

// 按 "/" 逐层查。返回命中的节点，未命中返回 nullptr。
const nlohmann::json* lookup(const nlohmann::json& root, const std::string& key)
{
    if (!root.is_object())
        return nullptr;

    const nlohmann::json* node = &root;
    size_t begin               = 0;

    while (begin <= key.size())
    {
        const size_t end  = key.find('/', begin);
        const std::string part = key.substr(begin, end == std::string::npos ? std::string::npos : end - begin);

        if (!node->is_object())
            return nullptr;

        const auto it = node->find(part);
        if (it == node->end())
            return nullptr;

        node  = &(*it);
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }

    return node->is_string() ? node : nullptr;
}

// 从某个语言表里取；同时把结果写进缓存。
const nlohmann::json* lookupCached(const std::string& key, const nlohmann::json& table)
{
    const auto cached = g_lookupCache.find(key);
    if (cached != g_lookupCache.end())
        return cached->second;

    const nlohmann::json* hit = lookup(table, key);
    g_lookupCache[key]        = hit;
    return hit;
}

bool g_defaultLoaded = false;

// 默认语言表的懒加载。任何公开入口（setLocale / getStr）都先确保它存在——
// 否则"直接调 setLocale()"这种用法在回退时会拿到空表（测试抓到过）。
void ensureDefaultLoaded();

int countLeaves(const nlohmann::json& node)
{
    if (node.is_string())
        return 1;

    if (!node.is_object())
        return 0;

    int total = 0;
    for (auto it = node.begin(); it != node.end(); ++it)
        if (it.key().empty() || it.key()[0] != '$') // 跳过 $files 这类元数据
            total += countLeaves(it.value());
    return total;
}

void ensureDefaultLoaded()
{
    if (g_defaultLoaded)
        return;

    g_defaultLoaded = true; // 先置位：loadLocale 内部不会再进来
    g_default       = loadLocale(DEFAULT_LOCALE);
    if (g_default.is_discarded() || !g_default.is_object())
    {
        BKLV_LOG_WARNING("默认文案 %s 未装载（缺失或语法错误），缺 key 时只能显示 key 本身",
            DEFAULT_LOCALE);
        g_default = nlohmann::json::object();
    }
}

} // namespace

std::string canonicalLocale(const std::string& raw)
{
    if (raw.empty())
        return DEFAULT_LOCALE;

    std::string lower = raw;
    for (char& c : lower)
        c = (char)std::tolower((unsigned char)c);

    // 中文要区分简繁：桌面 SDL 给的是国家（zh-CN / zh-TW），
    // Switch 给的是 SetLanguage（本函数同时兼容两者）。
    if (lower.rfind("zh", 0) == 0)
    {
        if (lower.find("hant") != std::string::npos || lower.find("tw") != std::string::npos
            || lower.find("hk") != std::string::npos || lower.find("mo") != std::string::npos)
            return "zh-Hant";
        return "zh-Hans";
    }

    if (lower.rfind("ja", 0) == 0)
        return "ja-JP";
    if (lower.rfind("ko", 0) == 0)
        return "ko-KR";
    if (lower.rfind("en", 0) == 0)
        return lower.find("gb") != std::string::npos ? "en-GB" : "en-US";

    return raw;
}

void initI18n()
{
    // 环境变量优先：桌面调试与测试都靠它，不用去改系统语言。
    const char* override_ = std::getenv("BKLV_LOCALE");
    const std::string wanted = canonicalLocale(override_ != nullptr ? override_ : probeSystemLocale());

    ensureDefaultLoaded();
    setLocale(wanted);
}

bool setLocale(const std::string& locale)
{
    ensureDefaultLoaded();

    const std::string wanted = canonicalLocale(locale);

    nlohmann::json loaded = loadLocale(wanted);
    if (loaded.is_discarded() || !loaded.is_object())
    {
        // 回退：完全没读到就退回默认语言，仍不可用则保持空表（getStr 返回 key）。
        BKLV_LOG_WARNING("语言 %s 的文案未装载，回退到 %s", wanted.c_str(), g_defaultLocale.c_str());
        g_current = g_default.is_object() ? g_default : nlohmann::json::object();
        g_locale  = g_defaultLocale;
        g_lookupCache.clear();
        return false;
    }

    g_current = std::move(loaded);
    g_locale  = wanted;
    g_lookupCache.clear(); // 换表了，旧指针全部失效

    BKLV_LOG_INFO("文案已装载：%s（%d 条）", g_locale.c_str(), countLeaves(g_current));
    return true;
}

const std::string& getLocale()
{
    return g_locale;
}

const std::string& getDefaultLocale()
{
    return g_defaultLocale;
}

std::string getStr(const std::string& key)
{
    if (key.empty())
        return key;

    ensureDefaultLoaded();

    // 当前语言 -> 默认语言 -> key 本身
    if (const nlohmann::json* hit = lookupCached(key, g_current))
        return hit->get<std::string>();

    if (const nlohmann::json* hit = lookup(g_default, key))
        return hit->get<std::string>();

    BKLV_LOG_DEBUG("文案缺失：%s", key.c_str());
    return key;
}

std::string formatStr(const std::string& text, const std::vector<std::string>& args)
{
    std::string out;
    out.reserve(text.size() + 16);

    size_t next = 0;
    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] == '{' && i + 1 < text.size() && text[i + 1] == '}')
        {
            if (next < args.size())
                out += args[next++];
            i++;
            continue;
        }
        out += text[i];
    }
    return out;
}

namespace detail
{

std::string toI18nArg(const std::string& value)
{
    return value;
}

std::string toI18nArg(const char* value)
{
    return value != nullptr ? std::string(value) : std::string();
}

std::string toI18nArg(int value)
{
    return std::to_string(value);
}

std::string toI18nArg(unsigned int value)
{
    return std::to_string(value);
}

std::string toI18nArg(long value)
{
    return std::to_string(value);
}

std::string toI18nArg(unsigned long value)
{
    return std::to_string(value);
}

std::string toI18nArg(long long value)
{
    return std::to_string(value);
}

std::string toI18nArg(unsigned long long value)
{
    return std::to_string(value);
}

std::string toI18nArg(float value)
{
    // %g 而不是 std::to_string：后者会给 "3.140000"。
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%g", (double)value);
    return buffer;
}

std::string toI18nArg(double value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%g", value);
    return buffer;
}

std::string toI18nArg(bool value)
{
    return value ? "true" : "false";
}

} // namespace detail

int getI18nKeyCount()
{
    return countLeaves(g_current);
}

int getI18nCacheSize()
{
    return (int)g_lookupCache.size();
}

} // namespace bklv
