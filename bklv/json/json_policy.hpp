#pragma once

// JSON 解析的**错误策略**与**诊断**。
//
// 背景：解析失败之后，除了"报错"，还应该能按配置选一种恢复动作。三种策略：
//
//   STRICT（默认）      字段级错误 → 整份对象编译失败（现有行为，"不静默降级"）
//   SKIP_FIELD          字段级错误 → 只丢出问题的那一个键、保留默认值，其余照常编译
//   FALLBACK_DEFAULT    文件级错误 → 整份退到 default 主题里的同名外观（见 appearance.cpp）
//
// 两条边界（写在这里，省得以后又混）：
//   ① 能"跳过"的只有**字段级**错误（颜色非法、数值类型不对、@theme key 不存在）；
//      文件级错误（JSON 语法坏、缺 layers）没法跳过，只能整份回退。
//   ② 无论哪种策略，**诊断都要留下**：`getJsonDiagnostics()` 里带 file / path / message，
//      recovered 标明这条是否已经按策略恢复。skip 不是"没发生"，只是"不致命"。
//
// 策略作用于"语法层面的错"，不改变既有约定：枚举拼错仍是**警告 + 保持原值**（它已经等于
// skip 的语义，若在 STRICT 下改成致命会破坏第三方主题）；视图的未知属性仍是警告。

#include <string>
#include <vector>

#include "bklv/json/json_util.hpp"

namespace bklv
{
namespace compile
{

enum class JsonErrorPolicy
{
    STRICT,
    SKIP_FIELD,
    FALLBACK_DEFAULT,
};

// 一条诊断。file 是"来源"（外观名 / 视图路径 / 空 = 内联文本），path 是 JSON 路径。
struct JsonDiagnostic
{
    std::string file;
    std::string path;
    std::string message;
    bool        recovered = false;
};

// 策略：全局一份（App 启动前设）。环境变量 `BKLV_JSON_POLICY=strict|skip|fallback` 也能设，
// 用于排查；显式调用 setJsonErrorPolicy 优先。
void            setJsonErrorPolicy(JsonErrorPolicy policy);
JsonErrorPolicy getJsonErrorPolicy();
const char*     jsonErrorPolicyName(JsonErrorPolicy policy);

// 诊断表：**只增不清**，由调用方在合适的时机 clear（例如"换主题前"或测试开头）。
const std::vector<JsonDiagnostic>& getJsonDiagnostics();
void                               clearJsonDiagnostics();
void                               pushJsonDiagnostic(const JsonDiagnostic& diagnostic);

// 作用域：设置"当前正在解析的来源"，只影响诊断里的 file 字段（RAII，可嵌套）。
class JsonSourceScope
{
  public:
    explicit JsonSourceScope(std::string source);
    ~JsonSourceScope();
    JsonSourceScope(const JsonSourceScope&)            = delete;
    JsonSourceScope& operator=(const JsonSourceScope&) = delete;

  private:
    std::string previous;
};

// 字段级解析的结果：OK 已应用 / SKIPPED 按策略跳过（目标值未改）/ FAILED 致命
enum class FieldResult
{
    OK,
    SKIPPED,
    FAILED,
};

// 解析一个颜色/数值字段，失败时按当前策略处理（并在诊断里留一条）
FieldResult applyColorField(const Json& node, const ThemeData& theme, ImU32& target,
    std::string& error, const std::string& where);
FieldResult applyNumberField(const Json& node, const ThemeData& theme, float& target,
    std::string& error, const std::string& where);

// 已经知道失败原因时用这个（例如自定义校验）；返回 FAILED 时 error 已被填好
FieldResult handleFieldFailure(const std::string& path, const std::string& message,
    std::string& error);

} // namespace compile
} // namespace bklv
