#include "bklv/core/attributes.hpp"

#include "bklv/core/icon_font.hpp"
#include "bklv/core/ui_scale.hpp"

#include <set>

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "bklv/core/i18n.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/view.hpp"

namespace bklv
{

namespace
{

// 归一：只留字母数字并转小写，`_` 与 `-` 直接丢掉
std::string normalizeEnumToken(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (char c : text)
    {
        if (c == '_' || c == '-' || c == ' ')
            continue;
        out.push_back((char)std::tolower((unsigned char)c));
    }
    return out;
}

} // namespace

bool applyViewAttribute(View* view, const std::string& name, const std::string& value)
{
    if (view == nullptr)
        return false;

    // 直接复用实例化 JSON 时那条路：类型转换、@theme / @i18n 解析、枚举宽容都在里面
    std::string error;
    const ApplyResult result = applyFromTable(view, view->attributes(), name, Value(value), error);
    return result == ApplyResult::APPLIED; // 未知属性是 UNKNOWN（只警告），这里要算失败
}

bool enumMatches(const std::string& text, const char* name)
{
    return normalizeEnumToken(text) == normalizeEnumToken(name);
}

bool warnUnknownEnumValue(const char* field, const std::string& text, const char* options)
{
    // 同一个「字段=值」只报一次：以前每次构造补间都刷一条，一屏日志全被它淹没
    // （实测：默认的 easing="linear" 曾因为匹配链漏了 linear 而狂刷几百条）
    static std::set<std::string> warned;
    if (!warned.insert(std::string(field) + "=" + text).second)
        return false;

    BKLV_LOG_WARNING("%s 不认识 \"%s\"（可用：%s；大小写与 _ 都不敏感，flexEnd == flex_end）",
        field, text.c_str(), options);
    return false;
}

namespace
{

// 视图 JSON 里的主题引用：`@theme/...` 取颜色、`@style/...` 取数值，
// 都落在 JSON 主题上（core/theme_data.*）：`@theme/color.a.b` / `@style/metric.a.b`。
// 省略前缀也行（`@theme/text.primary` -> `color.text.primary`）。
// 写法沿用外观那一套：斜杠会被容错成点分并警告；**key 不存在则报错**（不静默回退）。
std::string normalizeViewThemeKey(const std::string& key, const char* group, const std::string& where)
{
    std::string normalized = key;
    for (char& c : normalized)
        if (c == '/')
            c = '.';

    if (normalized.rfind("color.", 0) != 0 && normalized.rfind("metric.", 0) != 0)
        normalized = std::string(group) + normalized;

    if (normalized != key)
        BKLV_LOG_WARNING("视图 JSON 的主题引用写法：%s -> %s（%s）", key.c_str(), normalized.c_str(),
            where.c_str());

    return normalized;
}

} // namespace


namespace
{

bool startsWith(const std::string& value, const char* prefix)
{
    const size_t length = std::strlen(prefix);
    return value.size() >= length && value.compare(0, length, prefix) == 0;
}

std::string trim(const std::string& value)
{
    size_t begin = 0;
    size_t end   = value.size();
    while (begin < end && (value[begin] == ' ' || value[begin] == '\t'))
        begin++;
    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t'))
        end--;
    return value.substr(begin, end - begin);
}

// "12" / "12px" / "-3.5" -> 数字
bool parseNumber(const std::string& text, float& out)
{
    const std::string body = trim(text);
    if (body.empty())
        return false;

    char* end = nullptr;
    const double parsed = std::strtod(body.c_str(), &end);
    if (end == body.c_str() || end == nullptr)
        return false;

    // 只允许尾部还有空白（"12 abc" 这种要报错，不能当 12 用）
    while (*end == ' ' || *end == '\t')
        end++;
    if (*end != '\0')
        return false;

    out = (float)parsed;
    return true;
}

// 去掉尾部单位 "px"："12px" 与 12 等价（XML 那边就是这么写的）。
std::string stripPx(std::string text)
{
    text = trim(text);
    if (text.size() > 2 && text.compare(text.size() - 2, 2, "px") == 0)
        text = trim(text.substr(0, text.size() - 2));
    return text;
}

// 把 "#RRGGBB" / "#RRGGBBAA" 解析成 ImU32。
bool parseHexColor(const std::string& text, ImU32& out)
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

std::string kindName(AttributeKind kind)
{
    switch (kind)
    {
        case AttributeKind::NUMBER: return "数字";
        case AttributeKind::SIZE: return "尺寸（数字 / \"auto\" / \"50%\"）";
        case AttributeKind::BOOL: return "布尔";
        case AttributeKind::COLOR: return "颜色（\"#RRGGBB\" / \"@theme/xxx\"）";
        case AttributeKind::TEXT: return "字符串（支持 @i18n/）";
        case AttributeKind::STRING: return "字符串";
        case AttributeKind::RESOURCE_PATH: return "资源路径（\"@res/xxx\"）";
        case AttributeKind::ENUM: return "枚举";
    }
    return "值";
}

// 按 kind 把原始 Value 转成 ResolvedValue。失败填 error。
bool resolve(AttributeKind kind, const Value& value, const char* allowed, ResolvedValue& out,
    std::string& error)
{
    const std::string raw = value.raw();

    switch (kind)
    {
        case AttributeKind::NUMBER:
        {
            if (value.getKind() == Value::Kind::NUMBER)
            {
                out.number = (float)value.getNumber();
                return true;
            }
            if (value.getKind() == Value::Kind::BOOL)
                break;

            const std::string text = stripPx(raw);
            if (parseNumber(text, out.number))
                return true;

            // "@style/metric.a.b" -> 主题里的数值
            if (startsWith(text, "@style/"))
            {
                const std::string key = normalizeViewThemeKey(text.substr(7), "metric.", "@style/");
                if (!getTheme().hasNumber(key))
                {
                    error = "引用了不存在的主题数值：@style/" + key
                        + "（写法是 @style/metric.a.b）";
                    return false;
                }
                out.number    = getTheme().number(key);
                out.fromTheme = true; // ThemeData::number 已乘过 uiScale，别再乘一次
                return true;
            }
            break;
        }

        case AttributeKind::SIZE:
        {
            if (value.getKind() == Value::Kind::NUMBER)
            {
                out.number = (float)value.getNumber();
                return true;
            }
            if (value.getKind() == Value::Kind::BOOL)
                break;

            const std::string text = stripPx(raw);
            if (text == "auto")
            {
                out.number = AUTO;
                return true;
            }
            if (!text.empty() && text[text.size() - 1] == '%')
            {
                float percent = 0.0f;
                if (!parseNumber(text.substr(0, text.size() - 1), percent))
                    break;
                if (percent < 0.0f || percent > 100.0f)
                {
                    error = "百分比要在 0~100 之间，得到 " + text;
                    return false;
                }
                out.number  = percent;
                out.percent = true;
                return true;
            }
            if (startsWith(text, "@style/"))
            {
                const std::string key = normalizeViewThemeKey(text.substr(7), "metric.", "@style/");
                if (!getTheme().hasNumber(key))
                {
                    error = "引用了不存在的主题数值：@style/" + key
                        + "（写法是 @style/metric.a.b）";
                    return false;
                }
                out.number    = getTheme().number(key);
                out.fromTheme = true; // ThemeData::number 已乘过 uiScale，别再乘一次
                return true;
            }
            if (parseNumber(text, out.number))
                return true;
            break;
        }

        case AttributeKind::BOOL:
        {
            if (value.getKind() == Value::Kind::BOOL)
            {
                out.boolean = value.getBool();
                return true;
            }
            const std::string text = trim(raw);
            if (text == "true")
            {
                out.boolean = true;
                return true;
            }
            if (text == "false")
            {
                out.boolean = false;
                return true;
            }
            break;
        }

        case AttributeKind::COLOR:
        {
            const std::string text = trim(raw);
            if (text.empty())
                break;
            if (text[0] == '#')
            {
                if (parseHexColor(text, out.color))
                    return true;
                break;
            }
            if (startsWith(text, "@theme/"))
            {
                const std::string key = normalizeViewThemeKey(text.substr(7), "color.", "@theme/");
                if (!getTheme().hasColor(key))
                {
                    error = "引用了不存在的主题颜色：@theme/" + key
                        + "（写法是 @theme/color.a.b）";
                    return false;
                }
                out.color = getTheme().color(key);
                return true;
            }
            break;
        }

        case AttributeKind::TEXT:
        {
            out.text = raw;
            if (startsWith(raw, "@i18n/"))
            {
                out.text = getStr(raw.substr(6));
                return true;
            }
            // `@icon/名字` 与 `@icon/U+E000`：图标控件要的是**字形**，字形只有 C++ 侧能查
            // （名字表在 core/icon_font.cpp），所以在这里替使用者查一次。
            // 查不到就报错 —— 静默给空串的话，界面上少一个图标很难查出来。
            if (startsWith(raw, "@icon/"))
            {
                const std::string glyphRef = raw.substr(6);
                std::string glyph;
                if (glyphRef.rfind("U+", 0) == 0 || glyphRef.rfind("u+", 0) == 0)
                {
                    const unsigned long codepoint = std::strtoul(glyphRef.c_str() + 2, nullptr, 16);
                    glyph = icon::utf8Glyph(static_cast<unsigned int>(codepoint));
                }
                else
                {
                    glyph = icon::glyphByName(glyphRef.c_str());
                }
                if (glyph.empty())
                {
                    error = "未知图标 \"" + glyphRef
                        + "\"（按名字时用 @icon/star 这种写法，名字表见 core/icon_font.cpp；"
                          "不确定就写码位 @icon/U+E000）";
                    return false;
                }
                out.text = glyph;
            }
            return true;
        }

        case AttributeKind::STRING:
        {
            out.text = raw;
            return true;
        }

        case AttributeKind::RESOURCE_PATH:
        {
            // `@res/x` 是**相对 resources 目录**的（与 borealis 的 BRLS_RESOURCES 一致），
            // 所以 "resources/" 前缀在这里补上，调用点只写文件名。
            std::string path = raw;
            if (startsWith(path, "@res/"))
                path = "resources/" + path.substr(5);
            out.text = resourcePath(path);
            return true;
        }

        case AttributeKind::ENUM:
        {
            const std::string text = trim(raw);
            if (text.empty())
                break;

            // allowed 是 "a|b|c"，逐段宽容比：大小写与 `_`/`-` 都不敏感
            // （于是视图描述 JSON 写 flexEnd、控件外观 JSON 写 flex_end 都能过）。
            // 命中后回传表里登记的标准写法，后续 setter 只认一种拼写。
            const std::string list = allowed != nullptr ? allowed : "";
            size_t begin           = 0;
            while (begin <= list.size())
            {
                const size_t end = list.find('|', begin);
                const std::string option
                    = list.substr(begin, end == std::string::npos ? std::string::npos : end - begin);

                if (!option.empty() && enumMatches(text, option.c_str()))
                {
                    out.text = option; // 标准写法
                    return true;
                }
                if (end == std::string::npos)
                    break;
                begin = end + 1;
            }
            break;
        }
    }

    error = "期望 " + kindName(kind);
    if (kind == AttributeKind::ENUM && allowed != nullptr)
        error += std::string("，可选值：") + allowed;
    error += "，得到 \"" + raw + "\"";
    return false;
}

// 编辑距离，用于属性名拼错的提示。
int editDistance(const std::string& a, const std::string& b)
{
    std::vector<int> previous(b.size() + 1);
    std::vector<int> current(b.size() + 1);

    for (size_t j = 0; j <= b.size(); j++)
        previous[j] = (int)j;

    for (size_t i = 1; i <= a.size(); i++)
    {
        current[0] = (int)i;
        for (size_t j = 1; j <= b.size(); j++)
        {
            const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            int best       = previous[j] + 1;
            if (current[j - 1] + 1 < best)
                best = current[j - 1] + 1;
            if (previous[j - 1] + cost < best)
                best = previous[j - 1] + cost;
            current[j] = best;
        }
        previous = current;
    }
    return previous[b.size()];
}

} // namespace

std::string Value::raw() const
{
    switch (this->kind)
    {
        case Kind::STRING: return this->text;
        case Kind::BOOL: return this->boolean ? "true" : "false";
        case Kind::NUMBER:
        {
            char buffer[32];
            if (this->number == (double)(long long)this->number)
                std::snprintf(buffer, sizeof(buffer), "%lld", (long long)this->number);
            else
                std::snprintf(buffer, sizeof(buffer), "%g", this->number);
            return buffer;
        }
    }
    return {};
}

const AttributeDef* AttributeTable::find(const std::string& name) const
{
    for (const AttributeDef& def : this->defs)
        if (name == def.name)
            return &def;
    return nullptr;
}

std::string AttributeTable::suggest(const std::string& name) const
{
    const AttributeDef* best      = nullptr;
    int bestDistance              = 0;
    for (const AttributeDef& def : this->defs)
    {
        const int distance = editDistance(name, def.name);
        if (best == nullptr || distance < bestDistance)
        {
            best         = &def;
            bestDistance = distance;
        }
    }

    // 差太多就别猜了（防止把无关属性名当建议）
    const size_t threshold = name.size() / 2 + 1;
    if (best == nullptr || (size_t)bestDistance > threshold)
        return {};
    return best->name;
}

ApplyResult applyFromTable(View* view, const AttributeTable& table, const std::string& name,
    const Value& value, std::string& error)
{
    const AttributeDef* def = table.find(name);
    if (def == nullptr)
        return ApplyResult::NOT_HANDLED;

    ResolvedValue resolved;
    if (!resolve(def->kind, value, def->allowed, resolved, error))
        return ApplyResult::FAILED;

    // 像素量统一乘 uiScale：视图描述 JSON 里写的长度/坐标/偏移与主题 metric、外观 JSON
    // 用**同一个基准**（都是 1280x720 上量出来的），换分辨率不会错位。
    // 不乘的三种：百分比（"50%"）、auto（NAN）、来自主题的值（ThemeData::number 已乘过）。
    if (def->pixels && !resolved.fromTheme && !resolved.percent && std::isfinite(resolved.number))
        resolved.number *= getUiScale();

    if (def->set != nullptr)
        def->set(view, resolved);
    return ApplyResult::APPLIED;
}

} // namespace bklv
