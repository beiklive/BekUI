#include "bklv/foundation/json/json_policy.hpp"

#include <cstdlib>
#include <cstring>

#include "bklv/foundation/core/globals.hpp"
#include "bklv/foundation/core/logger.hpp"

namespace bklv
{
namespace compile
{

namespace
{

// 诊断表上限：外观/视图都有缓存，正常不会爆；这个上限是防"反复编译坏文件"把内存吃光
constexpr size_t kMaxDiagnostics = 500;

struct PolicyState
{
    JsonErrorPolicy             policy = JsonErrorPolicy::STRICT;
    bool                        envApplied = false;
    std::string                 source;
    std::vector<JsonDiagnostic> diagnostics;
};

PolicyState& state()
{
    static PolicyState instance;
    return instance;
}

void applyEnvOnce()
{
    PolicyState& s = state();
    if (s.envApplied)
        return;
    s.envApplied = true;

    const char* env = std::getenv("BKLV_JSON_POLICY");
    if (env == nullptr || env[0] == '\0')
        return;

    if (std::strcmp(env, "strict") == 0)
        s.policy = JsonErrorPolicy::STRICT;
    else if (std::strcmp(env, "skip") == 0)
        s.policy = JsonErrorPolicy::SKIP_FIELD;
    else if (std::strcmp(env, "fallback") == 0)
        s.policy = JsonErrorPolicy::FALLBACK_DEFAULT;
    else
        BKLV_LOG_WARNING("BKLV_JSON_POLICY=%s 不认识（可选 strict / skip / fallback），按 strict 处理",
            env);
}

void resetForTest()
{
    PolicyState& s = state();
    s.policy      = JsonErrorPolicy::STRICT;
    s.envApplied  = false;
    s.source.clear();
    s.diagnostics.clear();
}

} // namespace

void setJsonErrorPolicy(JsonErrorPolicy policy)
{
    state().policy = policy;
    state().envApplied = true; // 显式设置优先于环境变量
}

JsonErrorPolicy getJsonErrorPolicy()
{
    applyEnvOnce();
    return state().policy;
}

const char* jsonErrorPolicyName(JsonErrorPolicy policy)
{
    switch (policy)
    {
        case JsonErrorPolicy::STRICT: return "strict";
        case JsonErrorPolicy::SKIP_FIELD: return "skip";
        case JsonErrorPolicy::FALLBACK_DEFAULT: return "fallback";
    }
    return "strict";
}

const std::vector<JsonDiagnostic>& getJsonDiagnostics()
{
    return state().diagnostics;
}

void clearJsonDiagnostics()
{
    state().diagnostics.clear();
}

void pushJsonDiagnostic(const JsonDiagnostic& diagnostic)
{
    PolicyState& s = state();
    if (s.diagnostics.size() >= kMaxDiagnostics)
    {
        static bool warned = false;
        if (!warned)
        {
            warned = true;
            BKLV_LOG_WARNING("JSON 诊断已超过 %zu 条，后续不再记录（先修前面那些）", kMaxDiagnostics);
        }
        return;
    }
    s.diagnostics.push_back(diagnostic);
}

JsonSourceScope::JsonSourceScope(std::string source)
    : previous(state().source)
{
    state().source = std::move(source);
}

JsonSourceScope::~JsonSourceScope()
{
    state().source = previous;
}

FieldResult handleFieldFailure(const std::string& path, const std::string& message,
    std::string& error)
{
    JsonDiagnostic diagnostic;
    diagnostic.file    = state().source;
    diagnostic.path    = path;
    diagnostic.message = message;

    if (getJsonErrorPolicy() == JsonErrorPolicy::SKIP_FIELD)
    {
        diagnostic.recovered = true;
        pushJsonDiagnostic(diagnostic);
        BKLV_LOG_WARNING("JSON 字段已跳过（%s %s）：%s",
            diagnostic.file.empty() ? "内联" : diagnostic.file.c_str(), path.c_str(),
            message.c_str());
        return FieldResult::SKIPPED;
    }

    pushJsonDiagnostic(diagnostic);
    error = message;
    return FieldResult::FAILED;
}

FieldResult applyColorField(const Json& node, const ThemeData& theme, ImU32& target,
    std::string& error, const std::string& where)
{
    ImU32       value = target;
    std::string detail;
    if (resolveColor(node, theme, value, detail, where))
    {
        target = value;
        return FieldResult::OK;
    }
    return handleFieldFailure(where, detail, error);
}

FieldResult applyNumberField(const Json& node, const ThemeData& theme, float& target,
    std::string& error, const std::string& where)
{
    float       value = target;
    std::string detail;
    if (resolveNumber(node, theme, value, detail, where))
    {
        target = value;
        return FieldResult::OK;
    }
    return handleFieldFailure(where, detail, error);
}

namespace
{

// 测试用的重置钩子（globals 约定：静态状态要在换主题/测试之间清掉）
struct JsonPolicyResetHook
{
    JsonPolicyResetHook()
    {
        globals::getGlobalStore().addResetHook("jsonPolicy", &resetForTest);
    }
};
JsonPolicyResetHook g_jsonPolicyResetHook;

} // namespace

} // namespace compile
} // namespace bklv
