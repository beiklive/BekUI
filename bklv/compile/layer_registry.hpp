#pragma once

// 图层类型注册表：**"一种 op 是什么"的唯一事实来源**。
//
// 为什么要它：在此之前，"图层类型"的知识散在四处 ——
//   · 解析：`appearance.cpp` 里的枚举 + 键处理
//   · 绘制：`appearance_view.cpp` 里的 op 分支
//   · 默认 id：`appearance.cpp` 里那张只认 rect/text/image 的映射表
//   · 设计器：`designer_window.cpp` 里字符串版的一套（加层菜单/中文名/候选属性/层序校验）
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

// 层序档位：引擎按这个排序（不许各 op 自己决定先后）
//   SHADOW -> GLASS -> BACKGROUND -> HIGHLIGHT_BG -> CONTENT -> DECORATION -> OVERLAY
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

// 绘制钩子的上下文。`void*` 是为了让 compile/ 不依赖 widgets/ 与 ImGui：
//   drawList = ImDrawList*，view = AppearanceView*
struct LayerDrawContext
{
    const AppearanceLayer& layer;
    const AppearanceInstance& instance; // 取属性当前值（含状态补间结果）
    int layerIndex = 0;
    void* drawList  = nullptr;          // ImDrawList*
    LayerRect rect;
    void* view = nullptr;               // AppearanceView*（TEXT/IMAGE/CUSTOM 用内容槽）
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

// 注册 / 查表。name 重复、name 为空、idPrefix 为空都返回 false 并写 error。
bool registerLayer(LayerType type, std::string& error);

const LayerType* findLayer(const std::string& name);

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

// 层序档位 -> 与引擎排序一致的 rank（阴影 0、玻璃 1、其余按档位）
int layerOrderRank(LayerOrder order);

} // namespace compile
} // namespace bklv
