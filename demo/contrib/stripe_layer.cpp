// 第三方扩展层的样例：**完全不改 bklv/ 任何文件**，只新增这一个文件 + 一行注册。
//
// 这也是"通用图层注册模板"的实物验收（`compile/layer_register.hpp`）：
//   · 参数在 `params()` 里声明一次，引擎按模式解析 —— 类型校验、720p 缩放、拼错提示都白送；
//   · `keys` 由参数派生，设计器的加层菜单 / 中文名 / 候选键自动跟上；
//   · 绘制钩子用 `numberParam(ctx, "名字")` 取参数，不用记下标。
//
// 对比以前那份写法（git 历史里有）：这一层当初只能把角度偷存进 `layer.feather`、
// 线宽偷存进 `layer.spread`（引擎字段），因为参数槽还不存在。现在不用借了。
//
// 附带修掉一个小 bug：`feather` 是**像素量**（引擎会乘 uiScale），所以以前角度也被缩放了
// —— 非 720p 画布上条纹角度会随窗口宽度变；现在 `angle` 声明成 NUMBER，不缩放。

#include <cmath>
#include <vector>

#include "bklv/3rd/imgui/imgui.h"

#include "bklv/compile/layer_register.hpp"

namespace
{

using bklv::AppearanceLayer;
using bklv::LayerParamType;
using bklv::compile::LayerDrawContext;
using bklv::compile::LayerOrder;
using bklv::compile::LayerParam;

struct StripeLayer
{
    static constexpr const char* kName     = "stripe";
    static constexpr const char* kLabel    = "斜条纹";
    static constexpr const char* kIdPrefix = "stripe";
    static constexpr LayerOrder kOrder     = LayerOrder::DECORATION;
    static constexpr const char* kHelp     = "斜条纹：angle 角度 / lineWidth 线宽 / gap 间距";

    // 参数模式。位置参数顺序：name / type / numberDefault / colorDefault / boolDefault /
    // enumValues / label / help —— 只写用得到的那几个。
    static std::vector<LayerParam> params()
    {
        return {
            LayerParam::number("angle", 45.0f, "角度"),
            LayerParam::pixels("lineWidth", 2.0f, "线宽"),
            LayerParam::pixels("gap", 16.0f, "间距"),
        };
    }

    static void draw(const LayerDrawContext& ctx)
    {
        ImDrawList* drawList         = static_cast<ImDrawList*>(ctx.drawList);
        const bklv::compile::LayerRect& rect = ctx.rect;
        const float width            = rect.maxX - rect.minX;
        const float height           = rect.maxY - rect.minY;
        if (drawList == nullptr || width <= 1.0f || height <= 1.0f)
            return;

        const float angle = bklv::compile::numberParam(ctx, "angle", 45.0f);
        const float gap   = bklv::compile::numberParam(ctx, "gap", 16.0f);
        const float line  = bklv::compile::numberParam(ctx, "lineWidth", 2.0f);
        if (gap <= 0.0f || line <= 0.0f)
            return;

        // 角度 < 90 用 ↘，否则 ↗（保持样例原来的语义）
        const bool falling = angle < 90.0f;

        // 染色仍用引擎字段 `fill`（主题色由引擎解析好了），透明度折进颜色
        // （引擎已经把控件/状态透明度算进 rect.alpha）
        const float alpha = rect.alpha * (float)((ctx.layer.fill >> 24) & 0xFFu) / 255.0f;
        if (alpha <= 0.001f)
            return;
        const ImU32 color = (ctx.layer.fill & 0x00FFFFFFu) | ((ImU32)(alpha * 255.0f + 0.5f) << 24);

        // 朴素但可靠的排布：沿 x 每一列画一条跨满矩形高度的斜线，两端各多留一个高度，
        // 超出矩形由 PushClipRect 裁掉 —— 不用"居中 + 沿垂直线排开"那套（踩过：线大量落到裁剪区外）
        drawList->PushClipRect(ImVec2(rect.minX, rect.minY), ImVec2(rect.maxX, rect.maxY), true);
        for (float x = rect.minX - height; x <= rect.maxX + height; x += gap)
        {
            if (falling)
                drawList->AddLine(ImVec2(x, rect.minY), ImVec2(x + height, rect.maxY), color, line);
            else
                drawList->AddLine(ImVec2(x, rect.maxY), ImVec2(x + height, rect.minY), color, line);
        }
        drawList->PopClipRect();
    }
};

} // namespace

namespace demo
{

// 在 demo 的初始化里调一次（第三方程序就是在自己 main 里调这一句）
void registerStripeLayer()
{
    bklv::compile::registerLayer<StripeLayer>();
}

} // namespace demo
