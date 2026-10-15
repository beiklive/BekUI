// 第三方扩展层的样例：**完全不改 bklv/ 任何文件**，只新增这一个文件 + 一行注册。
//
// 用途：给"可扩展性"这条验收当实物证据 ——
//   · 解析侧：注册之后主题里写 `"op": "stripe"` 就能用（走 `Op::EXTENSION` + extensionType）
//   · 设计器：加层菜单/中文名/候选键全部从注册表读，所以设计器自动就有它
//   · 绘制侧：draw 钩子在这里备好，等 ③（绘制改读注册表）接上后生效
//
// 这就是"别人拿到这套代码，加自己的新层级"要写的全部东西。

#include <cmath>

#include "bklv/compile/json_util.hpp"
#include "bklv/compile/layer_registry.hpp"
#include "bklv/core/logger.hpp"

namespace
{

// 解析：把这一层自己的键读进来（统一报错由 json_util 提供，错误串带 key 路径）
bool parseStripe(const nlohmann::json& node, const bklv::ThemeData& theme,
    bklv::AppearanceLayer& out, std::string& error, const std::string& where)
{
    float angle = 45.0f;
    if (!bklv::compile::resolveNumber(node.contains("angle") ? node["angle"] : nlohmann::json(45.0f),
            theme, angle, error, where + ".angle"))
        return false;

    // 复用库里的通用字段：填充色与圆角（外观看得见的还是这两样）
    if (node.contains("fill") && !bklv::compile::resolveColor(node["fill"], theme, out.fill, error,
                                   where + ".fill"))
        return false;

    out.feather = angle; // 这一层没有自己的存储字段，先用 feather 把角度带过去（③ 接钩子后换掉）
    return true;
}

// 绘制：斜条纹。③ 把它接到注册表的 draw 钩子上（签名里的 void* 是 ImDrawList*）
void drawStripe(const bklv::AppearanceLayer& layer, void* drawList, const bklv::compile::LayerRect& rect)
{
    (void)layer;
    (void)drawList;
    (void)rect;
    // 真正的绘制在 ③ 完成后再补（那时这里能拿到 ImDrawList 并落下实际的线）
}

} // namespace

namespace demo
{

// 在 demo 的初始化里调一次（第三方程序就是在自己 main 里调这一句）
void registerStripeLayer()
{
    std::string error;
    bklv::compile::LayerType stripe;
    stripe.name     = "stripe";
    stripe.label    = "斜条纹";
    stripe.idPrefix = "stripe";
    stripe.order    = bklv::compile::LayerOrder::DECORATION;
    stripe.help     = "斜条纹：angle 角度 / width 线宽 / gap 间距（第三方扩展层样例）";
    stripe.keys     = { "angle", "width", "gap", "fill", "radius" };
    stripe.parse    = parseStripe;
    stripe.draw     = drawStripe;

    if (!bklv::compile::registerLayer(std::move(stripe), error))
        BKLV_LOG_WARNING("stripe 扩展层注册失败（可能已注册过）：%s", error.c_str());
}

} // namespace demo
