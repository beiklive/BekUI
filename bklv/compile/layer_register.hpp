#pragma once

// 图层类型的**注册模板**：把"加一种图层效果"要写的东西压到最少。
//
// 用法（一个文件搞定，库代码零改动）：
//
//     struct NoiseLayer
//     {
//         static constexpr const char* kName     = "noise";
//         static constexpr const char* kLabel    = "噪声";
//         static constexpr LayerOrder  kOrder    = LayerOrder::DECORATION;
//         static constexpr const char* kIdPrefix = "noise";   // 可省，默认用 kName
//         static constexpr const char* kHelp     = "颗粒噪声：strength 强度 / mono 单色";
//
//         static std::vector<LayerParam> params()
//         {
//             return { { "strength", LayerParamType::NUMBER, 0.5f, 0, false, {}, "强度" },
//                      { "mono",     LayerParamType::BOOL,   1.0f, 0, true,  {}, "单色" } };
//         }
//
//         static void draw(const LayerDrawContext& ctx);      // 必写
//         // static bool parse(const Json&, const ThemeData&, AppearanceLayer&,
//         //     std::string&, const std::string&);            // 可选：只用于结构参数
//     };
//
//     bklv::compile::registerLayer<NoiseLayer>();   // 一行注册
//
// 注册之后自动获得：
//   · JSON 里写 `"op": "noise"` 就能用（参数由引擎按 params() 解析，含类型校验与 uiScale）
//   · 设计器的 op 菜单、中文名、候选键、参数默认值全部跟着走（`keys` 由 params 派生）
//   · 参数名就是绘制钩子里的取法：`numberParam(ctx, "strength")` / `enumParam(ctx, "mode")`
//
// 取向与 `registerLayer(LayerType)` 一致：**无宏、无静态初始化魔法**（注册时机由调用方决定，
// 测试也能自己控制 —— 见 tests/layer_parse_test.cpp）。

#include <string>
#include <type_traits>
#include <vector>

#include "bklv/compile/json_util.hpp"    // Json
#include "bklv/compile/layer_registry.hpp"
#include "bklv/core/logger.hpp"

namespace bklv
{
namespace compile
{

namespace detail
{

// C++17 检测惯用法。要点：**不能用 if constexpr 直接写 T::kIdPrefix** ——
// 被丢弃的分支里的非依赖名字仍然会被检查，而 T 是具体类（不是模板）时
// clang 会直接报 "no member named kIdPrefix"。改成"偏特化 + 空 void_t"这套标准写法：
// 只有 T 真的声明了那个成员，那个偏特化才会被实例化。

// 必须有 static draw（检测惯用法，给出能读的编译期错误而不是一堆模板报错）
template <typename T, typename = void>
struct HasStaticDraw : std::false_type
{
};

template <typename T>
struct HasStaticDraw<T, std::void_t<decltype(T::draw(std::declval<const LayerDrawContext&>()))>>
    : std::true_type
{
};

// 可选成员 kIdPrefix（没写就用 kName）与 kHelp（没写就是空串）：用偏特化给默认值
template <typename T, typename = void>
struct IdPrefixOf
{
    static constexpr const char* value = T::kName;
};

template <typename T>
struct IdPrefixOf<T, std::void_t<decltype(T::kIdPrefix)>>
{
    static constexpr const char* value = T::kIdPrefix;
};

template <typename T, typename = void>
struct HelpOf
{
    static constexpr const char* value = "";
};

template <typename T>
struct HelpOf<T, std::void_t<decltype(T::kHelp)>>
{
    static constexpr const char* value = T::kHelp;
};

// 可选 parse 钩子：有就挂上，没有就留空（标量参数由引擎按模式解析，不需要钩子）
template <typename T, typename = void>
struct ParseOf
{
    static void apply(LayerType&) { }
};

template <typename T>
struct ParseOf<T, std::void_t<decltype(&T::parse)>>
{
    static void apply(LayerType& type) { type.parse = &T::parse; }
};

} // namespace detail

// 注册一个图层类型。返回是否成功（重名 / 参数模式非法时返回 false 并已记日志）。
template <typename T> bool registerLayer()
{
    static_assert(detail::HasStaticDraw<T>::value,
        "图层类型必须提供 static void draw(const LayerDrawContext&)");

    LayerType type;
    type.name     = T::kName;
    type.label    = T::kLabel;
    type.idPrefix = detail::IdPrefixOf<T>::value;
    type.order    = T::kOrder;
    type.help     = detail::HelpOf<T>::value;
    detail::ParseOf<T>::apply(type); // 可选：只用于结构参数
    type.draw = &T::draw;

    // 参数模式 + 由它派生的键表：设计器候选键、文档表格、未知键提示都用这一份，
    // 所以注册方**不用再手写一遍 keys**（手写会漂移）。
    type.params = T::params();
    for (const LayerParam& param : type.params)
        type.keys.push_back(param.name);

    std::string error;
    if (!registerLayer(std::move(type), error))
    {
        BKLV_LOG_ERROR("注册图层 %s 失败：%s", T::kName, error.c_str());
        return false;
    }
    return true;
}

} // namespace compile
} // namespace bklv
