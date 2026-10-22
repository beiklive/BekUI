#pragma once

// 图层类型注册表：**"一种 op 是什么"的唯一事实来源**。
//
// 为什么要它：在此之前，"图层类型"的知识散在四处 ——
//   · 解析：`appearance.cpp` 里的枚举 + 键处理
//   · 绘制：`appearance_view.cpp` 里的 op 分支
//   · 默认 id：`appearance.cpp` 里那张只认 rect/text/image 的映射表
//   · 工具/编辑器：用它枚举类型、中文名与参数模式（加层菜单、候选属性、层序校验）
// 结果是加一个 op 要在四处改，而且一定会漏（`glass` 就漏过）。
//
// 设计取向（按常见 C++ 规范，也跟本项目既有约定一致）：
//   · **无宏**：不用 `BKLV_REGISTER_LAYER(...)` 这种静态初始化魔法（顺序不可控、难调试）
//   · **显式注册**：内置层在 `registerBuiltinLayers()` 里逐个注册；第三方在自家初始化里
//     调一次 `registerLayer(...)` 即可，**不需要改库任何文件**
//   · 重复注册**报错**，不静默覆盖
//
// 这里全部是**数据**：解析期查表用，运行期不查（不影响帧时间）。

#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "bklv/core/appearance.hpp"

namespace bklv
{
namespace compile
{

// 层序档位。**注意它不是"内置层的绘制顺序"**（那由 JSON 书写顺序决定，见下面
// layerSortRank 的注释）—— 它是"一层自己声明的意图"，目前对**扩展层**生效：
// 声明 DECORATION / OVERLAY 的扩展层会画在所有内置层之上。
enum class LayerOrder
{
    SHADOW,
    GLASS,
    BACKGROUND,
    HIGHLIGHT_BG,
    CONTENT,
    DECORATION,
    OVERLAY,
};

// 绘制钩子拿到的矩形（和控件范围无关，是这一层自己的位置；alpha 已含控件/状态透明度）
struct LayerRect
{
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    float radius = 0.0f;
    float alpha = 1.0f;
};

struct LayerType; // LayerDrawContext 要带上"这一层是哪个类型"，供扩展层取参数

// 绘制钩子的上下文。`void*` 是为了让 compile/ 不依赖 widgets/ 与 ImGui：
//   drawList = ImDrawList*，view = AppearanceView*
struct LayerDrawContext
{
    const AppearanceLayer& layer;
    const AppearanceInstance& instance; // 取属性当前值（含状态补间结果）
    int layerIndex = 0;
    void* drawList  = nullptr;          // ImDrawList*
    LayerRect rect;
    void* view = nullptr;               // AppearanceView*

    // 内容槽的结果（由 AppearanceView::draw 通过虚函数算好后放进来；compile/ 不认识这些类型，
    // 所以一律用不透明指针/指针 —— 和 view 同样的处理方式）
    const std::string* text = nullptr;  // getLayerText(layer.source) 的结果（仅 TEXT 层填）
    void* font              = nullptr;  // ImFont*（仅 TEXT 层填）
    void* image             = nullptr;  // TextureRef::texture（仅 IMAGE 层填；nullptr = 没有图）
    void* customDraw        = nullptr;  // 保留：CUSTOM 层由视图自己画（见 drawCustomLayer）

    // 这一层的注册类型：扩展层靠它把 `layer.params[i]` 对应回名字与类型
    // （`ctx.type->params[i].name`）。内置层也有（拿到的是内置那条注册项）。
    const LayerType* type = nullptr;
};

// 一个图层参数的**模式**（注册时声明一次）：决定引擎怎么解析它、设计器给什么控件、
// 以及 states/transitions 能不能按名字引用它。
//
// 声明了模式的参数值存在 `AppearanceLayer::params`（下标 = 这里的下标），
// **引擎会自动解析**（NUMBER/PIXELS/COLOR/BOOL/ENUM/STRING 六种都不用写代码）。
// 只有数组/对象这种结构参数（比如内置 rect 的 gradient）才需要自己的 parse 钩子。
struct LayerParam
{
    std::string name;  // JSON 键名，也是 states/transitions 里引用的名字
    LayerParamType type = LayerParamType::NUMBER;

    // 默认值：JSON 里没写这个键时用它（**不是 0** —— 没写和写 0 是两件事）。
    // ENUM 时 numberDefault 是 enumValues 的下标（默认第几个候选）。
    float numberDefault = 0.0f;   // NUMBER / PIXELS / ENUM(下标)
    ImU32 colorDefault  = 0;      // COLOR
    bool boolDefault    = false;  // BOOL

    std::vector<std::string> enumValues; // ENUM 的候选（设计器下拉 + 拼错时提示）
    std::string label;                   // 中文名（设计器显示；空则用 name）
    std::string help;                    // 一句话说明（设计器提示）

    // 便捷构造：写在 params() 里可读得多，也不用凑齐所有字段
    // （聚合初始化漏掉尾部字段会触发 -Wmissing-field-initializers，项目要求零警告）。
    //
    //     { LayerParam::pixels("lineWidth", 2.0f, "线宽"),
    //       LayerParam::number("angle", 45.0f, "角度"),
    //       LayerParam::enumeration("mode", { "horizontal", "vertical" }, 0, "方向") }
    static LayerParam number(std::string name, float defaultValue, std::string label = {},
        std::string help = {});
    static LayerParam pixels(std::string name, float defaultValue, std::string label = {},
        std::string help = {});
    static LayerParam color(std::string name, ImU32 defaultValue, std::string label = {},
        std::string help = {});
    static LayerParam boolean(std::string name, bool defaultValue, std::string label = {},
        std::string help = {});
    static LayerParam enumeration(std::string name, std::vector<std::string> values,
        int defaultIndex = 0, std::string label = {}, std::string help = {});
};

using LayerDrawFn = std::function<void(const LayerDrawContext&)>;

struct LayerType
{
    std::string name;              // "rect"（JSON 里的 op 值）
    std::string label;             // 界面显示名（"矩形"），第三方可给自己的 op 一个中文名
    std::string idPrefix;          // 没写 id 时的默认前缀：rect -> rect0 / rect1
    LayerOrder order = LayerOrder::DECORATION;
    std::string help;              // 设计器与文档用的一句话
    std::vector<std::string> keys; // 该 op 可写的键（检查器候选、文档表格、键校验）
    std::vector<LayerParam> params; // 该 op 自己的参数模式（见 LayerParam）

    // 引擎保留键：这些是**层几何/通用属性**，引擎会先拿去算矩形与透明度，扩展层不能占用它们
    // （踩过：样例用 `width` 表示线宽 → 整层被做成 3px 宽的条，看着像"只剩左边缘几个毛刺"）。
    // 内置 6 种本来就该用这些键，所以它们置 true 豁免；第三方注册一律 false。
    static const std::vector<std::string>& reservedKeys();
    bool allowReservedKeys = false;
    // 钩子：①c-3 才接上（现在允许为空 —— 先让"数据"成为唯一事实来源）
    std::function<bool(const nlohmann::json& node, const ThemeData& theme, AppearanceLayer& out,
        std::string& error, const std::string& where)>
        parse;
    // 绘制钩子。上下文故意给全：内置层要拿实例值（含补间结果）与视图的内容槽
    // （getLayerText/getLayerFont/drawCustomLayer），第三方层多半只用 layer + rect。
    LayerDrawFn draw;
};

// 引擎自己会从 JSON 读的键（元信息 + 通用字段 + gradient）：任何 op 都能写，
// 值落在 `AppearanceLayer` 的具名成员上，绘制钩子直接读 `ctx.layer.fill` 这类。
//
// 两个用途：① 设计器的候选键 = 引擎键 ∪ 该类型的参数名（不用各写一份）；
// ② 注册时提醒"参数名撞了引擎键"（那样会有两份值，语义不清）。
const std::vector<std::string>& engineLayerKeys();

// 注册 / 查表。name 重复、name 为空、idPrefix 为空都返回 false 并写 error。
// 参数模式也会校验：参数名重复、超过 kMaxLayerParams、占用引擎保留键（除非
// allowReservedKeys）—— 都在这里一次报清楚，不等绘制期出怪现象。
bool registerLayer(LayerType type, std::string& error);

const LayerType* findLayer(const std::string& name);

// ---- 绘制钩子按名字取参数（省掉第三方自己记下标）----
//
// 参数个数 ≤ 8，名字比较的开销可以忽略（只有扩展层会调）。
// 参数名不存在 / 类型不对 / 下标越界都返回安全值，不抛不崩。

// 取 NUMBER / PIXELS / ENUM(下标) 参数的数值；没有这个参数时返回 fallback。
float numberParam(const LayerDrawContext& ctx, const char* name, float fallback = 0.0f);

// 取 COLOR 参数的颜色；没有这个参数时返回 fallback。
ImU32 colorParam(const LayerDrawContext& ctx, const char* name, ImU32 fallback = 0);

// 取 BOOL 参数；没有这个参数时返回 fallback。
bool boolParam(const LayerDrawContext& ctx, const char* name, bool fallback = false);

// 取 ENUM 参数当前值的字符串（按 enumValues 把下标翻回名字）；
// 没有这个参数 / 下标越界时返回空串。
const char* enumParam(const LayerDrawContext& ctx, const char* name);

// 内置 op 的类型：**数组下标查表**（不走字符串哈希）。
// 绘制分派每层每帧都要问一次，`findLayer("rect")` 每次都要构造 std::string 再做哈希查找 ——
// 这个函数把它降成一次数组索引。op 不是内置（EXTENSION）时返回 nullptr。
//
// 表里存的是**下标**而不是 LayerType*：注册表是 std::vector，扩展层再 registerLayer()
// 会让它 realloc，指针会失效（后果是层序随机错乱，见 .cpp 的 cacheBuiltinIndexes）。
const LayerType* builtinLayer(AppearanceLayer::Op op);

// 注册/替换绘制钩子。**由 widgets 侧在初始化时调用**（compile 只提供钩子类型，
// 不反向依赖 widgets）。name 不存在或钩子为空都返回 false。
bool setLayerDraw(const std::string& name, LayerDrawFn draw);

// op 枚举 -> 注册表里的名字（"一种 op 一个名字"的唯一来源，供绘制分派用）
const char* layerOpName(AppearanceLayer::Op op);

// 已注册的层（按注册顺序；设计器/文档遍历用）
const std::vector<LayerType>& registeredLayers();

// 清空（测试用）
void clearLayersForTest();

// 内置 6 种：rect / text / image / shadow / glass / custom。
// 显式列出，不做任何静态初始化。
void registerBuiltinLayers();

// 层序档位 -> 引擎排序用的 rank（升序 = 先画）。
//
// **为什么内置层不按完整阶梯排**：内置层之间的相对顺序目前由 JSON 书写顺序决定
// （`core/appearance.cpp` 的 sortLayers 只把阴影提到 0、玻璃提到 1，其余保持原序）；
// 实测 168 份随仓库发布的外观里有 **121 份**把 rect 层（hlGlow/hlBorder 这些聚焦描边）
// 写在 text 之后 —— 如果按"rect 永远属于 BACKGROUND"重排，这些描边会掉到文字下面。
// 所以这里的语义是：SHADOW=0、GLASS=1，其余统一为 2（**保持 JSON 顺序**），
// 只有声明了 DECORATION / OVERLAY 的层才往后排（扩展层画在所有内置层之上用这个）。
int layerSortRank(LayerOrder order);

} // namespace compile
} // namespace bklv
