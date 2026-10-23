#include "bklv/core/json_view.hpp"

#include <algorithm>
#include <fstream>
#include <list>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "bklv/compile/json_vars.hpp"
#include "bklv/compile/json_policy.hpp"
#include "bklv/core/attributes.hpp"
#include "bklv/core/globals.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/view.hpp"
#include "bklv/views/h_scrolling_frame.hpp"
#include "bklv/views/image.hpp"
#include "bklv/views/label.hpp"
#include "bklv/views/padding.hpp"
#include "bklv/views/progress_spinner.hpp"
#include "bklv/views/scrolling_frame.hpp"

namespace bklv
{

namespace
{

using Json = nlohmann::json;

// 规模上限：畸形或恶意描述不该把 Switch 拖死。
constexpr int MAX_DEPTH     = 32;
constexpr int MAX_NODES     = 4096;
constexpr size_t MAX_CACHED = 32; // 模板缓存上限（borealis 的 XML 缓存无上限）

std::unordered_map<std::string, std::function<View*()>>& viewRegistry()
{
    static std::unordered_map<std::string, std::function<View*()>> registry;
    return registry;
}

ViewTypeMap* g_contextViewTypes = nullptr;

// 内置类型是否已经注册过。放**命名空间作用域**（不是函数内 static）：
// clearViewTypesForTest() / resetGlobals() 要把它一起翻回来 —— 只清表不翻标志的话，
// 重置之后 "Box" / "Label" 这些类型会静默失效（JSON 报"未知 type"，比崩更难查）。
bool g_builtinsRegistered = false;

// 内置类型注册。放在函数里而不是散在各个 .cpp 的静态初始化里：
// 静态初始化顺序不可控，而这里只在第一次用到时注册，顺序天然安全。
void registerBuiltinViewTypes()
{
    if (g_builtinsRegistered)
        return;
    g_builtinsRegistered = true;

    // 注册表的状态是自己的静态表（搬不进全局表），按 core/globals.hpp 的约定登记重置钩子
    globals::getGlobalStore().addResetHook("viewTypes", &clearViewTypesForTest);

    registerViewType("Box", [] { return (View*)new Box(Axis::COLUMN); });
    registerViewType("Row", [] { return (View*)new Box(Axis::ROW); });
    registerViewType("Label", [] { return (View*)new Label(); });
    registerViewType("Image", [] { return (View*)new Image(); });
    registerViewType("ScrollingFrame", [] { return (View*)new ScrollingFrame(); });
    registerViewType("HScrollingFrame", [] { return (View*)new HScrollingFrame(); });
    registerViewType("ProgressSpinner", [] { return (View*)new ProgressSpinner(); });
    // 占位挤压（对标 borealis 的 brls:Padding）：[左][Padding][右] 把右边顶到另一头
    registerViewType("Padding", [] { return (View*)new Padding(); });
    // RecyclerFrame 不注册：它必须先有数据源才能用，光有类型没有意义（做法见 P3 的 cell）。
}

// 模板缓存：key 是完整路径（**不是 hash**——borealis 用 std::hash 当 key，
// 碰撞会静默返回别人的文档），带 LRU 淘汰与失败负缓存。
struct CacheEntry
{
    std::shared_ptr<const ViewTemplate> tmpl; // 失败时为 nullptr（负缓存）
    bool failed = false;
};

std::unordered_map<std::string, CacheEntry>& templateCache()
{
    static std::unordered_map<std::string, CacheEntry> cache;
    return cache;
}

std::list<std::string>& templateCacheOrder()
{
    static std::list<std::string> order;
    return order;
}

int& templateParseCount()
{
    static int count = 0;
    return count;
}

// 上一次实例化的诊断信息（错误 + 未知属性警告都收在这里）
int& lastErrorCount()
{
    static int count = 0;
    return count;
}

std::string& lastErrorText()
{
    static std::string text;
    return text;
}

// 解析/读盘阶段的失败也走同一套诊断，否则调用方只能拿到一个 nullptr，
// 完全不知道是"文件没有""语法错误"还是"结构不合法"。
void recordLoadError(const std::string& message)
{
    lastErrorText() = message + "\n";
    lastErrorCount() = 1;
}

void touchCacheOrder(const std::string& key)
{
    std::list<std::string>& order = templateCacheOrder();
    order.remove(key);
    order.push_back(key);

    while (order.size() > MAX_CACHED)
    {
        templateCache().erase(order.front());
        order.pop_front();
    }
}

// 实例化过程中的现场：include 栈（循环检测）、节点计数、错误收集。
struct InstantiateContext
{
    std::vector<std::string> includeStack;
    std::vector<Binding> bindings;
    int nodeCount = 0;
    std::vector<std::string> errors;
    std::vector<InstantiatedView>* map = nullptr; // 非空时记录 pointer → View*
};

// 正在进行的实例化。**必须整棵树共享**：include 会调回 ViewTemplate::instantiate()，
// 如果每次调用都新建现场，include 栈就被重置了，a -> b -> a 这种循环检测不出来，
// 会一路递归到爆栈（测试抓过）。
//
// 不用线程局部：界面构建是单线程的；真要多线程建树，这里得换成每线程一份。
InstantiateContext* g_activeContext = nullptr;

void addError(InstantiateContext& ctx, const std::string& pointer, const std::string& message)
{
    const std::string full = pointer + ": " + message;
    ctx.errors.push_back(full);
    BKLV_LOG_ERROR("JSON 视图 %s", full.c_str());
}

// "#RRGGBB" 这类之外，json 的标量都能当属性值。
bool toValue(const Json& node, Value& out, std::string& error)
{
    switch (node.type())
    {
        case Json::value_t::string:
            out = Value(node.get<std::string>());
            return true;
        case Json::value_t::boolean:
            out = Value(node.get<bool>());
            return true;
        case Json::value_t::number_integer:
        case Json::value_t::number_unsigned:
            out = Value((long long)node.get<long long>());
            return true;
        case Json::value_t::number_float:
            out = Value(node.get<double>());
            return true;
        case Json::value_t::null:
            error = "属性值是 null";
            return false;
        default:
            error = "属性值只能是字符串/数字/布尔，不能是数组或对象";
            return false;
    }
}

// 各类型的"主属性"：`"bind": "title"` 默认写到哪个属性上。
// 写不清楚的（比如 Box）没默认值，必须显式用 "bindTo"。
const char* primaryBindAttribute(const std::string& type)
{
    if (type == "Label")
        return "text";
    if (type == "Image")
        return "src";
    return nullptr;
}

std::string stripPrefix(std::string type)
{
    // 允许写 "bklv:Box"，也允许直接写 "Box"
    if (type.rfind("bklv:", 0) == 0)
        type = type.substr(5);
    return type;
}

View* instantiateNode(const Json& node, const std::string& pointer, int depth, InstantiateContext& ctx);

// include：把另一个描述文件整棵接进来。
//
// 注意：被引入的模板从它自己的深度 0 开始算，所以单靠 MAX_DEPTH 拦不住
// 一长串 include（A include B include C ...）。这里用 include 栈长度再兜一层，
// 循环检测负责"A->B->A"这种，栈深负责"足够长但不重复"这种。
View* instantiateInclude(const Json& node, const std::string& pointer, InstantiateContext& ctx)
{
    if (ctx.includeStack.size() >= (size_t)MAX_DEPTH)
    {
        addError(ctx, pointer, "include 嵌套超过 " + std::to_string(MAX_DEPTH) + " 层");
        return nullptr;
    }

    const std::string raw = node.value("include", "");
    if (raw.empty() || !node["include"].is_string())
    {
        addError(ctx, pointer, "include 必须是非空字符串");
        return nullptr;
    }

    const std::string path = resourcePath(raw);

    // 循环检测：A include B include A 要报错，不能栈溢出
    if (std::find(ctx.includeStack.begin(), ctx.includeStack.end(), path) != ctx.includeStack.end())
    {
        std::string chain;
        for (const std::string& entry : ctx.includeStack)
            chain += entry + " -> ";
        chain += path;
        addError(ctx, pointer, "循环 include：" + chain);
        return nullptr;
    }

    const std::shared_ptr<const ViewTemplate> tmpl = ViewTemplate::load(raw);
    if (tmpl == nullptr)
    {
        addError(ctx, pointer, "include 的文件装载失败：" + raw);
        return nullptr;
    }

    ctx.includeStack.push_back(path);
    View* view = tmpl->instantiate();
    ctx.includeStack.pop_back();

    return view;
}

View* instantiateNode(const Json& node, const std::string& pointer, int depth, InstantiateContext& ctx)
{
    if (depth > MAX_DEPTH)
    {
        addError(ctx, pointer, "嵌套超过 " + std::to_string(MAX_DEPTH) + " 层");
        return nullptr;
    }
    if (++ctx.nodeCount > MAX_NODES)
    {
        addError(ctx, pointer, "节点数超过 " + std::to_string(MAX_NODES) + " 个");
        return nullptr;
    }
    if (!node.is_object())
    {
        addError(ctx, pointer, "节点必须是对象");
        return nullptr;
    }

    if (node.contains("include"))
        return instantiateInclude(node, pointer, ctx);

    if (!node.contains("type"))
    {
        addError(ctx, pointer, "缺少 \"type\"（或改用 \"include\"）");
        return nullptr;
    }

    const std::string type = stripPrefix(node.value("type", ""));
    registerBuiltinViewTypes();

    std::function<View*()> factory;
    if (g_contextViewTypes != nullptr)
    {
        const auto contextCreator = g_contextViewTypes->find(type);
        if (contextCreator != g_contextViewTypes->end())
            factory = contextCreator->second;
    }
    if (!factory)
    {
        const auto globalCreator = viewRegistry().find(type);
        if (globalCreator != viewRegistry().end())
            factory = globalCreator->second;
    }
    if (!factory)
    {
        addError(ctx, pointer, "未知类型 \"" + type + "\"，可用：" + getViewTypeNames());
        return nullptr;
    }

    // unique_ptr 持有：中途任何一步失败/抛出都不会漏掉子树
    std::unique_ptr<View> view(factory());
    if (view == nullptr)
    {
        addError(ctx, pointer, "类型 \"" + type + "\" 的构造函数返回了空指针");
        return nullptr;
    }

    // 记下"这个视图是 JSON 里的哪个节点"（编辑器做选中/高亮用）
    if (ctx.map != nullptr)
        ctx.map->push_back({ pointer, view.get() });

    // ---- 数据绑定 ----
    if (node.contains("bind"))
    {
        if (!node["bind"].is_string() || node["bind"].get<std::string>().empty())
        {
            addError(ctx, pointer + "/bind", "bind 必须是非空字符串");
            return nullptr;
        }

        Binding binding;
        binding.field = node["bind"].get<std::string>();
        binding.view  = view.get();

        if (node.contains("bindTo"))
        {
            if (!node["bindTo"].is_string())
            {
                addError(ctx, pointer + "/bindTo", "bindTo 必须是字符串");
                return nullptr;
            }
            binding.attribute = node["bindTo"].get<std::string>();
        }
        else if (const char* primary = primaryBindAttribute(type))
        {
            binding.attribute = primary;
        }
        else
        {
            addError(ctx, pointer,
                "类型 \"" + type + "\" 没有默认绑定属性，请用 \"bindTo\" 指定");
            return nullptr;
        }

        ctx.bindings.push_back(binding);
    }

    // ---- 属性 ----
    for (auto it = node.begin(); it != node.end(); ++it)
    {
        const std::string& key = it.key();
        // 特殊键：type / children / include / bind / bindTo
        if (key == "type" || key == "children" || key == "include" || key == "bind" || key == "bindTo")
            continue;

        Value value;
        std::string convertError;
        if (!toValue(it.value(), value, convertError))
        {
            // 值本身不是标量（数组/对象）：跳过字段时丢这一个属性、整棵树照建
            if (compile::getJsonErrorPolicy() == compile::JsonErrorPolicy::SKIP_FIELD)
            {
                compile::JsonDiagnostic diagnostic;
                diagnostic.path      = pointer + "/" + key;
                diagnostic.message   = convertError;
                diagnostic.recovered = true;
                compile::pushJsonDiagnostic(diagnostic);
                BKLV_LOG_WARNING("JSON 视图字段已跳过（%s）：%s", diagnostic.path.c_str(),
                    convertError.c_str());
                continue;
            }
            addError(ctx, pointer + "/" + key, convertError);
            return nullptr;
        }

        std::string error;
        const ApplyResult result = view->applyAttribute(key, value, error);

        if (result == ApplyResult::FAILED)
        {
            // 跳过字段：丢掉这一个属性、整棵树照建（诊断里标 recovered）
            if (compile::getJsonErrorPolicy() == compile::JsonErrorPolicy::SKIP_FIELD)
            {
                compile::JsonDiagnostic diagnostic;
                diagnostic.path      = pointer + "/" + key;
                diagnostic.message   = error;
                diagnostic.recovered = true;
                compile::pushJsonDiagnostic(diagnostic);
                BKLV_LOG_WARNING("JSON 视图字段已跳过（%s）：%s", diagnostic.path.c_str(),
                    error.c_str());
                continue;
            }
            addError(ctx, pointer + "/" + key, error);
            return nullptr;
        }
        if (result == ApplyResult::NOT_HANDLED)
        {
            // 未知属性只警告，不中断——多写一个键不该让整个界面建不出来
            std::string message = "未知属性 \"" + key + "\"";
            const std::string hint = view->attributes().suggest(key);
            if (!hint.empty())
                message += "，是否想写 \"" + hint + "\"？";
            message += "（" + view->getClassString() + "，公共属性也认）";
            ctx.errors.push_back(pointer + "/" + key + ": " + message);
            BKLV_LOG_WARNING("JSON 视图 %s", (pointer + "/" + key + ": " + message).c_str());

            compile::JsonDiagnostic diagnostic;
            diagnostic.path      = pointer + "/" + key;
            diagnostic.message   = message;
            diagnostic.recovered = true; // 未知属性一直是"警告 + 继续"，等于已经跳过
            compile::pushJsonDiagnostic(diagnostic);
        }
    }

    // ---- 子节点 ----
    if (node.contains("children"))
    {
        const Json& children = node["children"];
        if (!children.is_array())
        {
            addError(ctx, pointer + "/children", "children 必须是数组");
            return nullptr;
        }

        Box* box = view->asBox();
        if (!children.empty() && box == nullptr)
        {
            addError(ctx, pointer + "/children",
                "类型 \"" + type + "\" 不是容器，不能有 children");
            return nullptr;
        }

        for (size_t i = 0; i < children.size(); i++)
        {
            const std::string childPointer = pointer + "/children/" + std::to_string(i);
            std::unique_ptr<View> child(instantiateNode(children[i], childPointer, depth + 1, ctx));
            if (child == nullptr)
                return nullptr; // 整棵失败：上层 unique_ptr 会把已建好的部分一起释放

            box->addView(child.get());
            child.release(); // 所有权移交给父节点
        }
    }

    return view.release();
}

} // namespace

// ---------------------------------------------------------------------------
// 类型注册
// ---------------------------------------------------------------------------

void registerViewType(const std::string& name, std::function<View*()> creator)
{
    viewRegistry()[name] = std::move(creator);
}

void clearViewTypesForTest()
{
    viewRegistry().clear();
    // 一并翻回"没注册过"：下一次用到时会重新注册内置类型（否则内置类型会静默失效）
    g_builtinsRegistered = false;
}

void setContextViewTypes(ViewTypeMap* types)
{
    g_contextViewTypes = types;
}

ViewTypeMap* getContextViewTypes()
{
    return g_contextViewTypes;
}

bool isViewTypeRegistered(const std::string& name)
{
    registerBuiltinViewTypes();
    if (g_contextViewTypes != nullptr && g_contextViewTypes->find(name) != g_contextViewTypes->end())
        return true;
    return viewRegistry().find(name) != viewRegistry().end();
}

std::string getViewTypeNames()
{
    registerBuiltinViewTypes();

    std::vector<std::string> names;
    names.reserve(viewRegistry().size());
    for (const auto& entry : viewRegistry())
        names.push_back(entry.first);

    std::sort(names.begin(), names.end());

    std::string joined;
    for (size_t i = 0; i < names.size(); i++)
    {
        if (i > 0)
            joined += " / ";
        joined += names[i];
    }
    return joined;
}

// ---------------------------------------------------------------------------
// 模板
// ---------------------------------------------------------------------------

struct ViewTemplate::Impl
{
    Json document;
};

ViewTemplate::ViewTemplate(std::shared_ptr<const Impl> sharedImpl, std::string source)
    : impl(std::move(sharedImpl))
    , origin(std::move(source))
{
}

ViewTemplate::~ViewTemplate() = default;

std::shared_ptr<const ViewTemplate> ViewTemplate::load(const std::string& path)
{
    registerBuiltinViewTypes();

    const std::string resolved = resourcePath(path);

    auto& cache = templateCache();
    const auto cached = cache.find(resolved);
    if (cached != cache.end())
    {
        if (!cached->second.failed)
            touchCacheOrder(resolved);
        return cached->second.tmpl; // 失败时是 nullptr（负缓存，不重复读盘/刷日志）
    }

    std::ifstream file(resolved, std::ios::binary);
    if (!file.is_open())
    {
        BKLV_LOG_ERROR("JSON 视图文件打不开：%s", resolved.c_str());
        recordLoadError(resolved + ": 文件打不开");
        cache[resolved] = CacheEntry { nullptr, true };
        touchCacheOrder(resolved);
        return nullptr;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    std::shared_ptr<const ViewTemplate> tmpl = fromString(buffer.str(), resolved);
    cache[resolved] = CacheEntry { tmpl, tmpl == nullptr };
    touchCacheOrder(resolved);
    return tmpl;
}

std::shared_ptr<const ViewTemplate> ViewTemplate::fromString(const std::string& json,
    const std::string& source)
{
    registerBuiltinViewTypes();

    auto shared = std::make_shared<Impl>();

    try
    {
        shared->document = Json::parse(json);
        templateParseCount()++;
    }
    catch (const Json::parse_error& e)
    {
        // parse_error.byte 是字节偏移，折算成行号便于直接定位
        size_t line = 1;
        for (size_t i = 0; i < e.byte && i < json.size(); i++)
            if (json[i] == '\n')
                line++;

        BKLV_LOG_ERROR("JSON 视图语法错误：%s:%zu  %s", source.c_str(), line, e.what());
        recordLoadError(source + ":" + std::to_string(line) + ": " + e.what());
        return nullptr;
    }

    // 变量块（$color.x / $number.y）：模板缓存的是展开后的结果（vars 与语言无关）。
    // box 的 children 里也能直接用 $，因为它绑的是"文件级"作用域，不是块级。
    {
        std::string varError;
        if (!compile::expandJsonVars(shared->document, varError))
        {
            BKLV_LOG_ERROR("视图 JSON 的变量块有问题：%s  %s", source.c_str(), varError.c_str());
            recordLoadError(source + ": " + varError);
            return nullptr;
        }
    }

    return std::shared_ptr<const ViewTemplate>(new ViewTemplate(shared, source));
}

View* ViewTemplate::instantiate(std::vector<Binding>* bindings) const
{
    if (this->impl == nullptr)
        return nullptr;

    // 嵌套调用（被 include 引入）：复用外层现场，让 include 栈与节点计数覆盖整棵树。
    // 错误也由最外层统一上报。
    if (g_activeContext != nullptr)
        return instantiateNode(this->impl->document, this->origin, 0, *g_activeContext);

    InstantiateContext ctx;
    // 根模板自身入栈，这样 include 绕回自己也能被检测到（fromString 的 origin 是 "<string>"）
    if (this->origin != "<string>")
        ctx.includeStack.push_back(this->origin);

    g_activeContext = &ctx;
    const compile::JsonSourceScope sourceScope(this->origin);
    View* view     = instantiateNode(this->impl->document, this->origin, 0, ctx);
    g_activeContext = nullptr;

    // 成功也记：未知属性是警告，不中断构建，但要让调用方/测试看得见
    std::string joined;
    for (const std::string& error : ctx.errors)
        joined += error + "\n";
    lastErrorText()  = joined;
    lastErrorCount() = (int)ctx.errors.size();

    // 只在成功时导出绑定：建树失败的话这些指针都指向已经释放的子树
    if (bindings != nullptr && view != nullptr)
        *bindings = ctx.bindings;

    return view;
}

View* createFromJSONFile(const std::string& path)
{
    const std::shared_ptr<const ViewTemplate> tmpl = ViewTemplate::load(path);
    if (tmpl == nullptr)
        return nullptr;
    return tmpl->instantiate();
}

std::shared_ptr<const ViewTemplate> loadViewTemplateFromTheme(const std::string& name)
{
    // name 可以写 "page_json"，也可以写全 "view/page_json"
    const std::string key = name.rfind("view/", 0) == 0 ? name : "view/" + name;

    const std::string* json = getTheme().viewJson(key);
    if (json == nullptr)
    {
        BKLV_LOG_WARNING("主题 %s 里没有视图描述 \"%s\"（%s.json）", getTheme().getName().c_str(),
            key.c_str(), key.c_str());
        return nullptr;
    }

    // 不带路径缓存：视图描述随主题走，主题一换内容就不同，缓存键得带上主题名
    return ViewTemplate::fromString(*json, getTheme().getName() + ":" + key);
}

View* createViewFromTheme(const std::string& name)
{
    const std::shared_ptr<const ViewTemplate> tmpl = loadViewTemplateFromTheme(name);
    if (tmpl == nullptr)
        return nullptr;
    return tmpl->instantiate();
}

View* createFromJSONString(const std::string& json)
{
    return createFromJSONString(json, nullptr);
}

View* createFromJSONString(const std::string& json, std::vector<InstantiatedView>* map,
    std::vector<Binding>* bindings)
{
    const std::shared_ptr<const ViewTemplate> tmpl = ViewTemplate::fromString(json, "<string>");
    if (tmpl == nullptr)
        return nullptr;

    // 映射挂在现场上，include 进来的子树也会被记（它们走的是同一个 ctx）
    if (map != nullptr)
    {
        map->clear();
        InstantiateContext ctx;
        ctx.map = map;
        if (g_activeContext != nullptr)
            return tmpl->instantiate();
        g_activeContext = &ctx;
        // 注意：这里设了 g_activeContext，ViewTemplate::instantiate 会走嵌套分支直接返回，
        // 那条路径**不会**把 bindings 拷出来（映射走指针所以看不出来）。所以在这里自己拷。
        View* view      = tmpl->instantiate();
        g_activeContext = nullptr;
        if (bindings != nullptr && view != nullptr)
            *bindings = ctx.bindings;

        // pointer 里带着模板来源前缀（字符串模板是 "<string>"，文件是路径）。
        // 导出给编辑器之前把它剥掉，变成"相对这份文档"的 JSON Pointer。
        const std::string prefix = "<string>";
        for (InstantiatedView& entry : *map)
        {
            if (entry.pointer == prefix)
                entry.pointer.clear();
            else if (entry.pointer.rfind(prefix + "/", 0) == 0)
                entry.pointer.erase(0, prefix.size());
        }
        return view;
    }
    return tmpl->instantiate();
}

int getTemplateCacheSize()
{
    return (int)templateCache().size();
}

int getTemplateParseCount()
{
    return templateParseCount();
}

void clearTemplateCache()
{
    templateCache().clear();
    templateCacheOrder().clear();
}

int getLastLoadErrorCount()
{
    return lastErrorCount();
}

std::string getLastLoadErrors()
{
    return lastErrorText();
}

} // namespace bklv
