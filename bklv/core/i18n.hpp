#pragma once

// 文案系统（i18n），对应 borealis 的 core/i18n.hpp。
//
// 与 borealis 的差异：
//   - 资源是 JSON，不是 XML（项目约定）；布局为 `resources/i18n/<locale>.json`。
//   - **不依赖目录枚举**：borealis 在 Switch 上要靠 libromfs 列目录，
//     而 romfs 上的目录迭代真机未验证；这里改成"按名字读文件"，
//     需要拆分时在文件内用 `$files` 清单显式列出，不做扫描。
//   - 查找结果带缓存（borealis 每次调用都重新切分 key 并逐层查树）。
//
// 语言名是**规范化**的（`zh-Hans` / `zh-Hant` / `en-US` / `ja-JP` / `ko-KR`），
// 平台探测结果（桌面 "zh-CN"、Switch "zh-Hans"）都经 canonicalLocale() 归一。

#include <string>
#include <utility>
#include <vector>

namespace bklv
{

// 初始化文案系统：探测系统语言并装载对应文案。
// App 构造时调用——视图树常在 App::run() 之前就构建好，文案必须在那之前就绪。
// 探测不依赖 Platform 实例（建树时平台还不存在），见 i18n.cpp。
void initI18n();

// 运行中切换语言并重新装载。
//
// 注意：**已建好的视图不会自动重译**——文案是在控件构造时解析成字符串的。
// 需要重新 setText()，或者重建视图树。新创建的视图用新语言。
bool setLocale(const std::string& locale);

// 当前语言；未装载成功时返回空串。默认语言是 en-US（缺 key 时的回退目标）。
const std::string& getLocale();
const std::string& getDefaultLocale();

// 取文案。key 用 "/" 分层：{"demo":{"welcome":"..."}} 对应 "demo/welcome"。
// 未命中顺序：当前语言 -> 默认语言 -> 返回 key 本身（并记一次 debug 日志）。
std::string getStr(const std::string& key);

// 把平台原始语言串规范成资源文件名。"zh-CN"/"zh-Hans" 都归到 "zh-Hans"。
std::string canonicalLocale(const std::string& raw);

// 占位符替换：`{}` 按出现顺序依次替换，多余参数忽略，参数不足留空。
//   formatStr("共 {} 项", {"12"}) -> "共 12 项"
std::string formatStr(const std::string& text, const std::vector<std::string>& args);

namespace detail
{

inline void collectArgs(std::vector<std::string>&) { }

// 参数转字符串：整数直接转，浮点用 %g（避免 std::to_string 的 "3.140000"）。
std::string toI18nArg(const std::string& value);
std::string toI18nArg(const char* value);
std::string toI18nArg(int value);
std::string toI18nArg(unsigned int value);
std::string toI18nArg(long value);
std::string toI18nArg(unsigned long value);
std::string toI18nArg(long long value);
std::string toI18nArg(unsigned long long value);
std::string toI18nArg(float value);
std::string toI18nArg(double value);
std::string toI18nArg(bool value);

template <typename T, typename... Rest>
void collectArgs(std::vector<std::string>& out, T&& first, Rest&&... rest)
{
    out.push_back(toI18nArg(std::forward<T>(first)));
    collectArgs(out, std::forward<Rest>(rest)...);
}

} // namespace detail

// 带参数的文案：getStr("list/count", 12)
template <typename... Args>
std::string getStr(const std::string& key, Args&&... args)
{
    std::vector<std::string> list;
    detail::collectArgs(list, std::forward<Args>(args)...);
    return formatStr(getStr(key), list);
}

// 测试与诊断用：已装载的 key 数量、查找缓存条目数。
int getI18nKeyCount();
int getI18nCacheSize();

} // namespace bklv
