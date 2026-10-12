#pragma once

// JSON 视图描述：把 JSON 文件/字符串变成视图树。
//
// 对应 borealis 的 `View::createFromXMLFile` 那一套（它用 tinyxml2），
// 这里用 JSON，但**两段式**是必须的：
//
//   ViewTemplate（解析一次、可缓存、与语言/主题无关）
//         ↓ instantiate()
//   视图树（每次实例化新建，@i18n/ @style/ @theme/ 在这一步才解析）
//
// 为什么不能只做一段：模板缓存的存在意义就是"同一份描述实例化很多次"
// （列表里每个 cell 都是同一份模板）。如果解析期就把 `@i18n/` 替换掉，
// 缓存里就烤死了当时的语言，切语言或第二次复用时全是旧文案。
//
// 结构（属性直接平铺在节点上，特殊键只有四个）：
//   {
//     "type": "Box",              // 视图类型，见 registerViewType()
//     "axis": "column",           // 其余键都是属性
//     "children": [ {...} ],      // 子节点，保序（布局依赖顺序）
//     "include": "views/x.json"   // 与 type 互斥：引入另一个描述文件
//   }
//
// 错误处理：结构错误返回 nullptr 并记日志（borealis 是直接 fatal 崩进程）；
// 未知属性只警告，并给出最接近的名字。

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <vector>

namespace bklv
{

class View;

// 一条数据绑定：JSON 节点上写 `"bind": "字段名"`（可用 `"bindTo"` 指定属性，
// 默认取该类型的主属性——Label 是 text、Image 是 src）。
// 实例化时会把"哪个视图绑了哪个字段"收集出来，交给列表项在 setIndex 时填值。
struct Binding
{
    std::string field;     // 数据字段名，由 binder 解读
    std::string attribute; // 要设置的属性名
    View* view = nullptr;  // 目标视图（实例化出来的树里的节点）
};

// 注册一个视图类型，供 JSON 的 "type" 使用。
void registerViewType(const std::string& name, std::function<View*()> creator);
bool isViewTypeRegistered(const std::string& name);

// 已注册的类型名（错误提示与文档用）。
std::string getViewTypeNames();

class ViewTemplate
{
  public:
    struct Impl; // 真正的 json 文档藏在这里，公开头文件不引 nlohmann

    // 按资源路径装载（内部带缓存；同一个路径只解析一次）。
    // 路径走 resourcePath()，**要写出 resources/ 这一层**（和图片一样）：
    //   load("resources/views/card.json")
    // 桌面解析成 <exe目录>/resources/views/card.json，Switch 是 romfs:/resources/...
    // 失败返回 nullptr（文件缺失 / 语法错误都算失败，且会被负缓存）。
    static std::shared_ptr<const ViewTemplate> load(const std::string& path);

    // 从内存里的 JSON 文本装载。origin 只用于日志与错误信息。
    static std::shared_ptr<const ViewTemplate> fromString(const std::string& json, const std::string& origin);

    ~ViewTemplate();
    ViewTemplate(const ViewTemplate&) = delete;
    ViewTemplate& operator=(const ViewTemplate&) = delete;

    // 实例化一棵视图树。失败返回 nullptr（错误已进日志）。
    // bindings 非空时，收集模板里所有 `"bind"` 节点（含 include 进来的）。
    View* instantiate(std::vector<Binding>* bindings = nullptr) const;

    const std::string& getOrigin() const { return this->origin; }

  private:
    ViewTemplate(std::shared_ptr<const Impl> impl, std::string origin);

    std::shared_ptr<const Impl> impl;
    std::string origin;
};

// 便捷入口：装载 + 实例化。只用一次时用这两个就够了。
View* createFromJSONFile(const std::string& path);
View* createFromJSONString(const std::string& json);

// 实例化时"哪个视图是 JSON 里的哪个节点"。
// pointer 是 JSON Pointer（"" = 根，"/children/0" = 第一个子节点）。
// 编辑器要靠它做"选中节点 → 在预览里框出来"和"点在预览上 → 反选节点"。
struct InstantiatedView
{
    std::string pointer;
    View* view = nullptr;
};

// 同 createFromJSONString，但额外导出映射（按创建顺序，含 include 进来的子树）
// bindings 非空时也导出数据绑定（哪个视图绑了哪个字段）
View* createFromJSONString(const std::string& json, std::vector<InstantiatedView>* map,
    std::vector<Binding>* bindings = nullptr);

// ---- 测试与诊断 ----

// 模板缓存条目数 / 实际解析次数。解析次数的意义：
// 同一份模板实例化 N 次，解析次数必须仍是 1。
int getTemplateCacheSize();
int getTemplateParseCount();
void clearTemplateCache();

// 上一次实例化遇到的错误条数（结构错误会把整次实例化判为失败）。
int getLastLoadErrorCount();
std::string getLastLoadErrors();

} // namespace bklv
