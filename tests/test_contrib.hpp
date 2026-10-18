#pragma once

// 测试用的"第三方扩展层"注册（只为测试；demo 里的第三方层样例已随清理移除）。
//
// 为什么需要它：主题或内联外观里可以直接写扩展 op（`"op": "stripe"`），但"编译时能看到
// 第三方的注册"这件事在生产里靠调用方在 main 里注册；测试进程没有那个调用方，
// 所以凡是要**编译扩展 op**（含整主题校验：主题 files[] 里所有外观）的测试，都得先注册一次。
// 抽成一个共享头，免得每个测试各写一份、然后慢慢漂移。
//
// 默认主题自己**不带**扩展 op（曾留下的 `base/stripe_demo.json` 已撤，见评审 §4.2 P0④），
// 所以整主题校验类测试不再需要它；现在只有专门验扩展层的测试（layer_parse_test）要调。
//
// 这里用的是**通用图层注册模板**（compile/layer_register.hpp）：参数模式声明一次，
// 解析 / 缩放 / 拼错提示 / 设计器候选键都由引擎派生。真实样例照着 demo/contrib 那份写。

#include <string>
#include <vector>

#include "bklv/compile/layer_register.hpp"

namespace test_contrib
{

namespace
{

using bklv::LayerParamType;
using bklv::compile::LayerDrawContext;
using bklv::compile::LayerOrder;
using bklv::compile::LayerParam;

struct StripeTestLayer
{
    static constexpr const char* kName  = "stripe";
    static constexpr const char* kLabel = "斜条纹";
    static constexpr LayerOrder kOrder  = LayerOrder::DECORATION;
    // 故意不写 kIdPrefix / kHelp：模板要给它们兜底（idPrefix 默认用 name、help 可以空）

    static std::vector<LayerParam> params()
    {
        return {
            LayerParam::number("angle", 45.0f, "角度"),
            LayerParam::pixels("lineWidth", 2.0f, "线宽"),
            LayerParam::pixels("gap", 16.0f, "间距"),
        };
    }

    // 测试不真的绘制；条件允许时仍然走一遍真实钩子（注册表要求 draw 非空）
    static void draw(const LayerDrawContext&) { }
};

} // namespace

inline void registerStripe()
{
    bklv::compile::registerLayer<StripeTestLayer>();
}

} // namespace test_contrib
