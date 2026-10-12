#pragma once

// JSON 视图描述里的属性系统。
//
// 设计要点（相对 borealis 的 XML 属性表）：
//   - **属性表按类静态**：borealis 给每个 View 实例挂 7 个 unordered_map
//     （float/percentage/color/bool/string/filePath/auto），回收列表里几百个 cell 时
//     全是纯开销。这里改成每个类一张静态表，handler 是普通函数指针、带 this。
//   - **值带类型**：JSON 里可以写原生数字/布尔（`"fontSize": 24`），
//     也可以写带单位的字符串（`"24px"`、`"50%"`），统一转成 ResolvedValue 再分派。
//   - **前缀在解析期处理**：`@i18n/` `@style/` `@theme/` `@res/` 与 `#RRGGBB`。
//   - **认识但值不对**（类型不符 / 枚举值非法）会报错并给出允许值，
//     而不是像 borealis 那样静默丢弃。

#include <initializer_list>
#include <string>
#include <vector>

#include "imgui.h" // ImU32

namespace bklv
{

class View;

// 属性值的原始形态。字符串一律保留原样（可能带前缀），转换在使用时进行。
class Value
{
  public:
    enum class Kind
    {
        STRING,
        NUMBER,
        BOOL,
    };

    Value() = default;
    Value(const char* text) : kind(Kind::STRING), text(text != nullptr ? text : "") { }
    Value(const std::string& value) : kind(Kind::STRING), text(value) { }
    Value(bool boolean) : kind(Kind::BOOL), boolean(boolean) { }

    // 整数/浮点都走这里；bool 由上面的重载精确匹配，避免二义。
    template <typename T,
        typename = typename std::enable_if<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value>::type>
    Value(T number) : kind(Kind::NUMBER), number(static_cast<double>(number))
    {
    }

    Kind getKind() const { return this->kind; }

    // 原样文本，仅用于错误信息（数字/布尔会转成字符串）。
    std::string raw() const;

    // 下面三个只在 getKind() 对应时才有效。
    const std::string& getString() const { return this->text; }
    double getNumber() const { return this->number; }
    bool getBool() const { return this->boolean; }

  private:
    Kind kind = Kind::STRING;
    std::string text;
    double number = 0.0;
    bool boolean = false;
};

// 属性声明的类型。决定值怎么转换、以及出错时的措辞。
enum class AttributeKind
{
    NUMBER,       // 12 / "12" / "12px" / "@style/brls/xxx"
    SIZE,         // AUTO / "auto" / 12 / "50%"
    BOOL,         // true / "true"
    COLOR,        // "#RRGGBB" / "#RRGGBBAA" / "@theme/xxx"
    TEXT,         // 字符串，`@i18n/key` 会被翻译
    STRING,       // 字符串，不做任何解析
    RESOURCE_PATH, // `@res/xxx` -> RESOURCE_PATH()
    ENUM,         // 取值必须落在 allowed 里（"row|column"）
};

// 转换好的值，交给 setter。按 kind 只填其中一部分。
struct ResolvedValue
{
    float number  = 0.0f; // Number / Size（AUTO 时是 NAN）
    bool percent  = false; // Size："50%" -> number=50, percent=true
    bool boolean  = false;
    ImU32 color   = 0;
    std::string text;
};

using AttributeSetter = void (*)(View* view, const ResolvedValue& value);

struct AttributeDef
{
    const char* name;
    AttributeKind kind;
    AttributeSetter set;
    const char* allowed; // 仅 Enum 用，"row|column" 这种
};

// 一个类的属性表。静态存续（函数内 static），不随实例分配。
// 枚举字符串的**宽容匹配**：忽略大小写与 `_` / `-` 分隔符。
// 于是 flexEnd / flex_end / FLEX_END / FlexEnd 都命中同一个值 ——
// 视图描述 JSON（camelCase）与控件外观 JSON（snake_case）的拼写差异就此抹平。
bool enumMatches(const std::string& text, const char* name);

// 用**字符串值**给视图套一个属性（按属性表的类型自己转：数字 / 颜色 / 布尔 / 文本 / 枚举）。
// 编辑器用来"给数据绑定填示例值""单独试一个属性值"；找不到属性返回 false。
bool applyViewAttribute(View* view, const std::string& name, const std::string& value);

// 枚举值不认识时打一条**能照着改**的警告（字段名 + 可用值），返回 false 让调用方决定兜底。
// 约定：不认识就**保持原值**不动，而不是悄悄改成别的值。
bool warnUnknownEnumValue(const char* field, const std::string& text, const char* options);

class AttributeTable
{
  public:
    AttributeTable(std::initializer_list<AttributeDef> defs) : defs(defs) { }

    const AttributeDef* find(const std::string& name) const;
    const std::vector<AttributeDef>& all() const { return this->defs; }
    bool isEmpty() const { return this->defs.empty(); }

    // 拼错属性名时给个候选（编辑距离最近的）。
    std::string suggest(const std::string& name) const;

  private:
    std::vector<AttributeDef> defs;
};

enum class ApplyResult
{
    NOT_HANDLED, // 这张表里没有这个属性
    APPLIED,    // 认了，也设好了
    FAILED,     // 认识这个属性，但值不合法（error 里是人话）
};

// 在表里查并应用。类型不符 / 枚举非法 -> Failed 并填 error。
ApplyResult applyFromTable(View* view, const AttributeTable& table, const std::string& name,
    const Value& value, std::string& error);

} // namespace bklv
