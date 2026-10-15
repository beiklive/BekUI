// 第三方扩展层的样例：**完全不改 bklv/ 任何文件**，只新增这一个文件 + 一行注册。
//
// 用途：给"可扩展性"这条验收当实物证据 ——
//   · 解析侧：注册之后主题里写 `"op": "stripe"` 就能用（走 `Op::EXTENSION` + extensionType）
//   · 设计器：加层菜单/中文名/候选键全部从注册表读，所以设计器自动就有它
//   · 绘制侧：draw 钩子在这里备好，等 ③（绘制改读注册表）接上后生效
//
// 这就是"别人拿到这套代码，加自己的新层级"要写的全部东西。

#include <cmath>

#include "bklv/3rd/imgui/imgui.h"

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

    float width = 2.0f;
    if (node.contains("width") && !bklv::compile::resolveNumber(node["width"], theme, width, error,
                                      where + ".width"))
        return false;

    // 这一层暂时借用库里现成的两个通用字段：feather 存角度、spread 存线宽。
    // （等 `AppearanceLayer` 给扩展 op 提供自己的参数槽后换掉 —— 那样才彻底不借用。）
    out.feather = angle;
    out.spread  = width;
    return true;
}

// 绘制：斜条纹。引擎（appearance_view.cpp 的 EXTENSION 分支）会把这个钩子接上，
// 传进来的 void* 就是 ImDrawList*，rect 是这一层自己的矩形（含圆角与有效 alpha）。
void drawStripe(const bklv::AppearanceLayer& layer, void* drawListPtr,
    const bklv::compile::LayerRect& rect)
{
    ImDrawList* drawList = static_cast<ImDrawList*>(drawListPtr);
    if (drawList == nullptr || rect.maxX - rect.minX <= 1.0f || rect.maxY - rect.minY <= 1.0f)
        return;

    const float angle = layer.feather; // 解析钩子暂存在这里
    const float width = layer.spread > 0.0f ? layer.spread : 2.0f;
    const float gap   = 14.0f;

    // 有效 alpha 直接折进颜色（引擎已经算好 rect.alpha）
    const float alpha = rect.alpha * (float)((layer.fill >> 24) & 0xFFu) / 255.0f;
    const ImU32 color = (layer.fill & 0x00FFFFFFu)
        | ((ImU32)(alpha * 255.0f + 0.5f) << 24);

    const float radians = angle * 3.14159265f / 180.0f;
    const float dx      = std::cos(radians);
    const float dy      = std::sin(radians);
    const float length  = std::sqrt((rect.maxX - rect.minX) * (rect.maxX - rect.minX)
                              + (rect.maxY - rect.minY) * (rect.maxY - rect.minY));
    const float centerX = (rect.minX + rect.maxX) * 0.5f;
    const float centerY = (rect.minY + rect.maxY) * 0.5f;
    // 与条纹垂直的方向：沿线排开每 gap 像素一条
    const float stepX = -dy * gap;
    const float stepY = dx * gap;

    drawList->PushClipRect(ImVec2(rect.minX, rect.minY), ImVec2(rect.maxX, rect.maxY), true);
    const int count = (int)(length / gap) + 2;
    for (int i = -count; i <= count; i++)
    {
        const float offsetX = stepX * (float)i;
        const float offsetY = stepY * (float)i;
        drawList->AddLine(
            ImVec2(centerX + offsetX - dx * length * 0.5f, centerY + offsetY - dy * length * 0.5f),
            ImVec2(centerX + offsetX + dx * length * 0.5f, centerY + offsetY + dy * length * 0.5f),
            color, width);
    }
    drawList->PopClipRect();
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
