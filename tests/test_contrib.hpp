#pragma once

// 测试用的"第三方扩展层"注册（**只为测试**，真实样例见 demo/contrib/stripe_layer.cpp）。
//
// 为什么需要它：主题里可以直接写扩展 op（`"op": "stripe"`），但"主题加载时能看到
// 第三方的注册"这件事在生产里靠调用方在 main 里注册；测试进程没有那个调用方，
// 所以凡是要**整主题校验**（编译主题 files[] 里所有外观）的测试，都得先注册一次。
// 抽成一个共享头，免得每个测试各写一份、然后慢慢漂移。

#include <string>

#include "bklv/compile/json_util.hpp"
#include "bklv/compile/layer_registry.hpp"

namespace test_contrib
{

inline void registerStripe()
{
    bklv::compile::LayerType stripe;
    stripe.name     = "stripe";
    stripe.label    = "斜条纹";
    stripe.idPrefix = "stripe";
    stripe.order    = bklv::compile::LayerOrder::DECORATION;
    stripe.help     = "测试用第三方扩展层（真实样例见 demo/contrib/stripe_layer.cpp）";
    stripe.keys     = { "angle", "width", "gap", "fill", "radius" };
    stripe.parse    = [](const nlohmann::json& node, const bklv::ThemeData& theme,
                          bklv::AppearanceLayer& out, std::string& error, const std::string& where) {
        if (node.contains("fill"))
            return bklv::compile::resolveColor(node["fill"], theme, out.fill, error, where + ".fill");
        return true;
    };

    std::string error;
    bklv::compile::registerLayer(std::move(stripe), error);
}

} // namespace test_contrib
