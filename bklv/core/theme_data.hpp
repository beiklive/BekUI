#pragma once

// 主题数据：把 resources/theme/<name>/ 下的 JSON 编译成内存里的查表结构。
//
// 目录与分类（**不扫目录**：romfs 上的目录迭代真机未验证，清单必须提前写）：
//   resources/theme/themes.json          主题清单（有哪些主题、默认是哪个）
//   resources/theme/default/theme.json   元信息（作者/描述）+ 本主题的文件清单 files[]
//   .../tokens/colors.json               全局色板   -> key 前缀 "color."
//   .../tokens/metrics.json              全局数值   -> key 前缀 "metric."
//   .../base/*.json                      基础控件外观（单个容器/元素）
//   .../widget/*.json                    二级控件外观（引用 base 组合出来的）
//   .../view/*.json                      页面/视图描述（结构 + 外观）
//
// **名字 = 相对主题根的路径去掉扩展名**：`base/button/button.json` -> "base/button/button"、
// `widget/sidebar.json` -> "widget/sidebar"。所以挪文件 = 改名字，C++ 里的引用要一起改。
// 变体仍是后缀：`base/button/button.json` + variant "primary" -> "base/button/button.primary"。
//
// 换肤 = 复制 default 改名 + 改 JSON + 在 themes.json 里登记。
// 缺项的回退链：当前主题 -> default 主题 -> 调用方给的兜底值（只警告，不崩）。

#include <string>
#include <unordered_map>
#include <vector>

#include "imgui.h"

namespace bklv
{

class ThemeData
{
  public:
    // 装载一个主题。name 为空时用 themes.json 里的 default 字段。
    // 无论 name 是什么，都会先把 default 装载成回退表。
    bool load(const std::string& name);

    // 按**绝对目录**装载（设计器要编辑别的仓库里的主题，不能只认 resources/theme/<name>）
    bool loadFromDirectory(const std::string& themeDir, const std::string& themeName);

    const std::string& getName() const { return this->name; }

    // 取颜色 / 数值。找不到时先查 default 主题，再返回兜底值（并记一次 debug 日志）。
    ImU32 color(const std::string& key, ImU32 fallback = IM_COL32(255, 0, 255, 255)) const;
    float number(const std::string& key, float fallback = 0.0f) const;

    // "这个 key 能不能取到值"——包含 default 主题的回退。
    // 外观编译用它做校验：引用不存在的主题 key 要报错，而不是静默回退成洋红。
    bool hasColor(const std::string& key) const;
    bool hasNumber(const std::string& key) const;

    // 控件外观描述的**原始 JSON 文本**（未编译）。名字是"相对主题根的路径去掉扩展名"。
    // 当前主题没有这个文件时回退到 default 主题的；两边都没有返回 nullptr。
    // 编译交给 core/appearance.*（这样 theme 不依赖 appearance，避免循环依赖）。
    const std::string* widgetAppearanceJson(const std::string& widget) const;

    // **只取 default 主题**的那份原文（当前主题同名文件坏了时，错误策略会用它整份回退）。
    // 注意它和 widgetAppearanceJson 的区别：后者是"当前主题优先、缺了才回退"。
    const std::string* defaultWidgetAppearanceJson(const std::string& widget) const;

    // 页面/视图描述的原始 JSON 文本（view/ 目录，结构 + 外观）。
    // 同样带 default 主题回退；找不到返回 nullptr。实例化交给 core/json_view.*。
    const std::string* viewJson(const std::string& name) const;

    // 元信息（设计器"打开项目"用）
    const std::string& getAuthor() const { return this->author; }
    const std::string& getDescription() const { return this->description; }
    // 描述文案的 i18n key（可空；有就走 translate，没有就直接用 description）
    const std::string& getDescriptionKey() const { return this->descriptionKey; }

    // 可用的主题名（来自 themes.json，按清单顺序）。
    const std::vector<std::string>& getAvailableThemes() const { return this->available; }

    // 已装载的 key 数量（测试与诊断）。
    int getColorCount() const { return (int)this->colors.size(); }
    int getDefaultColorCount() const { return (int)this->defaultColors.size(); }

    // 键列表（排序后），给编辑器做下拉/只读表格用
    std::vector<std::string> getColorKeys() const;
    std::vector<std::string> getNumberKeys() const;

    // 该主题登记的外观名（路径名，如 "base/button/button.primary"），顺序与 theme.json 的 files[] 一致
    std::vector<std::string> getWidgetNames() const;
    // 该主题登记的视图名（"view/page_json"）
    std::vector<std::string> getViewNames() const;
    int getNumberCount() const { return (int)this->numbers.size(); }

  private:
    // 装载一个主题目录到给定的表里；isDefault 决定失败时是否记录警告。
    bool loadInto(const std::string& name, bool isDefault);
    // dir 以 / 结尾；default 那一份也用它（缺项回退）
    bool loadIntoDirectory(const std::string& dir, const std::string& themeName, bool isDefault);

    std::string name = "default";
    std::unordered_map<std::string, ImU32> colors;
    std::unordered_map<std::string, float> numbers;
    std::unordered_map<std::string, std::string> widgetJson;
    std::unordered_map<std::string, std::string> viewJsonMap;

    std::string author;
    std::string description;
    std::string descriptionKey;

    // default 主题的回退表
    std::unordered_map<std::string, ImU32> defaultColors;
    std::unordered_map<std::string, float> defaultNumbers;
    std::unordered_map<std::string, std::string> defaultWidgetJson;
    std::unordered_map<std::string, std::string> defaultViewJson;

    std::vector<std::string> available;
};

// 全局当前主题（App 构造时装载 default；换肤走 appearance.hpp 的 setTheme）。
const ThemeData& getTheme();

// 把全局主题换成 name。返回 false 表示没装载成功（全局主题会保持原样）。
// 一般不用直接调这个——换肤还要清外观缓存并让视图树重新套用外观，
// 那是 appearance.hpp 的 setTheme()。
bool loadThemeIntoGlobal(const std::string& name);

// 同上，但主题目录是绝对路径
bool loadThemeIntoGlobalFromDirectory(const std::string& themeDir, const std::string& themeName);

// 主题资源根目录（相对资源目录）。
constexpr const char* THEME_DIR = "resources/theme/";

} // namespace bklv
