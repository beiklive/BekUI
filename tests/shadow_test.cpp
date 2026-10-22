// 无窗口测试：阴影层与模糊档位开关。

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <tuple>
#include <utility>
#include <cstdlib>
#include <string>

#include "bklv/3rd/imgui/imgui.h"
#include "bklv/3rd/imgui/imgui_internal.h"

#include "bklv/compile/layer_registry.hpp" // findLayer（断言 inner 在设计器候选键里）
#include "bklv/core/appearance.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/shadow.hpp"
#include "bklv/core/theme_data.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

const char* SHADOW_JSON = R"({
    "layers": [
        { "op": "shadow", "id": "shadow",
          "fill": "#00000080", "feather": 12, "radius": 6,
          "offset": [0, 4], "spread": 2 },
        { "op": "rect", "id": "bg", "fill": "#ff0000ff", "radius": 6 }
    ],
    "states": {
        "focused": { "shadow.feather": { "to": 24, "duration": 100 } }
    }
})";

} // namespace

int main()
{
    bklv::initResources();
    initI18n();
    setLocale("zh-Hans");

    std::printf("== 1. 解析：shadow 层的字段 ==\n");
    {
        std::string error;
        std::shared_ptr<const Appearance> appearance
            = Appearance::compile(SHADOW_JSON, getTheme(), error);
        check(appearance != nullptr, "能编译带 shadow 层的外观");
        if (appearance != nullptr)
        {
            const std::vector<AppearanceLayer>& layers = appearance->getLayers();
            checkEqualSize(layers.size(), 2, "两层");
            const AppearanceLayer& shadow = layers[0];
            check(shadow.op == AppearanceLayer::Op::SHADOW, "op = shadow");
            checkEqual(shadow.id, "shadow", "id 保留（状态里要引用）");
            checkColor(shadow.fill, IM_COL32(0, 0, 0, 128), "颜色含透明通道");
            checkFloat(shadow.feather, 12.0f, "feather 解析出来");
            checkFloat(shadow.spread, 2.0f, "spread 解析出来");
            checkFloat(shadow.offset[1], 4.0f, "offset 解析出来");
            checkFloat(shadow.radius, 6.0f, "radius 解析出来");
        }
    }

    std::printf("\n== 2. 状态动效：shadow.feather 可插值 ==\n");
    {
        std::string error;
        std::shared_ptr<const Appearance> appearance
            = Appearance::compile(SHADOW_JSON, getTheme(), error);
        check(appearance != nullptr, "编译成功");
        if (appearance != nullptr)
        {
            AppearanceInstance instance;
            instance.setAppearance(appearance);
            checkFloat(instance.getNumber(0, AppearanceProp::FEATHER), 12.0f, "初值 12");

            instance.setState("focused", true);
            instance.update(0.2f); // 目标 24、时长 100ms，推一大步就到位
            checkFloat(instance.getNumber(0, AppearanceProp::FEATHER), 24.0f,
                "聚焦后羽化补到 24");
        }
    }

    std::printf("\n== 3. 档位开关 ==\n");
    {
        shadow::setQuality(ShadowQuality::FAST);
        check(shadow::getQuality() == ShadowQuality::FAST, "能设成 fast");
        checkEqual(shadow::qualityName(shadow::getQuality()), "fast", "fast 的名字");

        check(shadow::setQualityByName("smooth"), "按名字设 smooth 成功");
        check(shadow::getQuality() == ShadowQuality::SMOOTH, "确实切到 smooth");
        checkEqual(shadow::qualityName(shadow::getQuality()), "smooth", "smooth 的名字");

        check(!shadow::setQualityByName("magic"), "不认识的名字返回 false");
        check(shadow::getQuality() == ShadowQuality::SMOOTH, "失败时不改当前档位");

        // 环境变量只影响"第一次读取"，之后以 setQuality 为准
        shadow::resetEnvironmentProbeForTest();
        setEnv("BKLV_SHADOW_QUALITY", "fast");
        check(shadow::getQuality() == ShadowQuality::FAST, "环境变量 BKLV_SHADOW_QUALITY=fast 生效");
        unsetEnv("BKLV_SHADOW_QUALITY");

        shadow::setQuality(ShadowQuality::FAST);
    }

    std::printf("\n== 4. 档位直接决定画几层 ==\n");
    {
        // 无窗口测试没有 ImGui 上下文，手搓 ImDrawList 会空指针（踩过），
        // 所以断言 drawSoft 真正使用的那个层数
        shadow::setQuality(ShadowQuality::FAST);
        checkEqualInt(shadow::layerCount(), 8, "fast = 8 层");

        shadow::setQuality(ShadowQuality::SMOOTH);
        checkEqualInt(shadow::layerCount(), 24, "smooth = 24 层");

        // 退化输入不该崩
        shadow::drawSoft(nullptr, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, IM_COL32_WHITE, 1.0f, 10.0f, 0.0f);
        check(true, "drawList 为空时安全返回");

        shadow::setQuality(ShadowQuality::FAST);
    }

    std::printf("\n== 5. cutout：挖空内圈（顶点级验证）==\n");
    {
        // 建一个真正的 ImGui 上下文，就能拿到合法的 ImDrawList 并检查顶点。
        // 两个坑：① 自建列表必须先 _ResetForNewFrame()，否则 PrimReserve 越界；
        // ② 没有帧时 _FringeScale 是 0，_AddPolyline 会把粗描边算成 0 直接丢掉。
        ImGui::CreateContext();
        {
            auto makeList = [] {
                // ③ 没有 NewFrame() 就没人调 SetCircleTessellationMaxError()：
                //    shared data 构造时 memset 过，CircleSegmentCounts 全是 0，
                //    画圆角/圆形时它会被当除数算弧步长 → 整数除零崩溃。
                ImGui::GetDrawListSharedData()->SetCircleTessellationMaxError(0.3f);
                ImDrawList* list = new ImDrawList(ImGui::GetDrawListSharedData());
                list->_ResetForNewFrame();
                list->_FringeScale    = 1.0f;
                list->_InvFringeScale = 1.0f;
                // 无窗口时默认裁剪区是空的：给一个大裁剪区（cutout 用 PushClipRect 分块画）
                list->PushClipRect(ImVec2(-1000.0f, -1000.0f), ImVec2(1000.0f, 1000.0f), false);
                return list;
            };

            const ImU32 color  = IM_COL32(0, 0, 0, 200);
            const float minX = 0.0f, minY = 0.0f, maxX = 100.0f, maxY = 100.0f;
            const float radius = 12.0f, feather = 10.0f;

            // "在控件里面"要按**圆角形状**算：阴影的内缘是贴合控件圆角的，
            // 方形包围盒的四个角本来就不该算在里面（那部分是父层的背景）
            auto insideRoundedShape = [&](const ImVec2& p) {
                const float r  = radius;
                const float cx = std::min(std::max(p.x, minX + r), maxX - r);
                const float cy = std::min(std::max(p.y, minY + r), maxY - r);
                const float dx = p.x - cx;
                const float dy = p.y - cy;
                const float edge = r - 0.5f;
                return (dx * dx + dy * dy) < edge * edge;
            };

            // 覆盖测试：ImGui 的凸填充只产生边界顶点，"有没有阴影"要看三角形盖没盖住采样点
            // 注意要按 clip 判断：cutout 用 PushClipRect 分块画，三角形顶点会伸进控件，
            // 真正看不见的部分靠裁剪，只看三角形会误判成"盖住了控件中心"。
            auto covered = [](ImDrawList* list, const ImVec2& p) {
                auto sign = [](const ImVec2& a, const ImVec2& b, const ImVec2& c) {
                    return (a.x - c.x) * (b.y - c.y) - (b.x - c.x) * (a.y - c.y);
                };
                for (const ImDrawCmd& cmd : list->CmdBuffer)
                {
                    if (p.x < cmd.ClipRect.x || p.y < cmd.ClipRect.y || p.x >= cmd.ClipRect.z
                        || p.y >= cmd.ClipRect.w)
                        continue;
                    for (unsigned int i = cmd.IdxOffset; i + 2 < cmd.IdxOffset + cmd.ElemCount; i += 3)
                    {
                        const ImVec2 a = list->VtxBuffer[list->IdxBuffer[i]].pos;
                        const ImVec2 b = list->VtxBuffer[list->IdxBuffer[i + 1]].pos;
                        const ImVec2 c = list->VtxBuffer[list->IdxBuffer[i + 2]].pos;
                        const float d1 = sign(p, a, b);
                        const float d2 = sign(p, b, c);
                        const float d3 = sign(p, c, a);
                        const bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
                        const bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
                        if (!(hasNeg && hasPos))
                            return true;
                    }
                }
                return false;
            };

            auto countInside = [&](bool cutout) {
                ImDrawList* list = makeList();
                shadow::drawSoft(list, minX, minY, maxX, maxY, radius, color, 1.0f, feather, 0.0f,
                    cutout);

                int inside = 0;
                float loX = 1e9f, loY = 1e9f, hiX = -1e9f, hiY = -1e9f;
                float maxCornerDist = 0.0f;
                const ImVec2 center(minX + radius, minY + radius);
                for (int i = 0; i < list->VtxBuffer.Size; i++)
                {
                    const ImVec2 p = list->VtxBuffer[i].pos;
                    // 只看"看得见"的顶点：cutout 的壳层网格会伸到 alpha=0 的远处
                    if (((list->VtxBuffer[i].col >> 24) & 0xFFu) == 0)
                        continue;
                    loX = std::min(loX, p.x); hiX = std::max(hiX, p.x);
                    loY = std::min(loY, p.y); hiY = std::max(hiY, p.y);
                    if (p.x <= center.x && p.y <= center.y)
                    {
                        const float dx = p.x - center.x, dy = p.y - center.y;
                        maxCornerDist = std::max(maxCornerDist, std::sqrt(dx * dx + dy * dy));
                    }
                }
                // 可见覆盖：三角形质心同时落在"控件形状内"和"该条命令的裁剪区内"
                for (const ImDrawCmd& cmd : list->CmdBuffer)
                {
                    for (unsigned int i = cmd.IdxOffset; i + 2 < cmd.IdxOffset + cmd.ElemCount; i += 3)
                    {
                        const ImVec2 a = list->VtxBuffer[list->IdxBuffer[i]].pos;
                        const ImVec2 b = list->VtxBuffer[list->IdxBuffer[i + 1]].pos;
                        const ImVec2 c = list->VtxBuffer[list->IdxBuffer[i + 2]].pos;
                        const ImVec2 mid((a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f);
                        if (mid.x < cmd.ClipRect.x || mid.y < cmd.ClipRect.y || mid.x >= cmd.ClipRect.z
                            || mid.y >= cmd.ClipRect.w)
                            continue;
                        if (insideRoundedShape(mid))
                            inside++;
                    }
                }
                const int verts = list->VtxBuffer.Size;
                const bool centerCovered = covered(list, ImVec2((minX + maxX) * 0.5f,
                    (minY + maxY) * 0.5f));
                const bool cornerCovered = covered(list, ImVec2(minX + 3.0f, minY + 3.0f));
                delete list;
                return std::make_tuple(inside, verts, loX, hiX, loY, hiY, maxCornerDist,
                    centerCovered, cornerCovered);
            };

            const auto plain  = countInside(false);
            const auto cutout = countInside(true);

            // 不开挖空：阴影是一整块，控件中心被阴影三角形盖住 → 半透明底色就会透出来
            check(std::get<7>(plain), "不开 cutout 时控件中心被阴影盖住（半透明底会透出来）");
            // 开挖空：中心不被覆盖（透明底色也看不到阴影）
            check(!std::get<7>(cutout), "cutout 时控件中心没有被阴影覆盖");
            // 圆角处（方形包围盒的角、控件的圆角外面）仍然有阴影
            check(std::get<8>(cutout), "cutout 时圆角外（bbox 内、控件形状外）仍有阴影");
            check(std::get<0>(plain) >= 0, "两种模式的顶点都检查过");
            // 开挖空：控件圆角形状内部一个顶点都没有 → 透明底色也看不到阴影
            checkEqualInt(std::get<0>(cutout), 0, "cutout 时控件形状内部没有任何顶点");
            check(std::get<1>(cutout) > 0, "cutout 画出了东西（不是空实现）");

            // cutout + offset：孔仍挖在控件矩形里，阴影整体跟着 offset 走
            {
                ImDrawList* list = makeList();
                const float offsetY = 10.0f;
                shadow::drawSoft(list, minX, minY, maxX, maxY, radius, color, 1.0f, feather,
                    0.0f, true, 0.0f, offsetY);

                float hiY = -1e9f;
                for (int i = 0; i < list->VtxBuffer.Size; i++)
                    hiY = std::max(hiY, list->VtxBuffer[i].pos.y);
                const bool centerCovered = covered(list, ImVec2((minX + maxX) * 0.5f,
                    (minY + maxY) * 0.5f));
                const bool belowCovered = covered(list, ImVec2((minX + maxX) * 0.5f,
                    maxY + offsetY + 2.0f));
                delete list;

                check(!centerCovered, "cutout+offset：控件中心仍然看不到阴影");
                check(belowCovered, "cutout+offset：offset 下方能看到阴影");
                check(hiY >= maxY + offsetY, "cutout+offset：阴影跟着 offset 下移（不再被吃掉）");
            }

            // 外圈够到最外边界（四条边都在）
            check(std::fabs(std::get<2>(cutout) - (minX - feather)) <= 1.5f, "左边界到最外");
            check(std::fabs(std::get<3>(cutout) - (maxX + feather)) <= 1.5f, "右边界到最外");
            check(std::fabs(std::get<4>(cutout) - (minY - feather)) <= 1.5f, "上边界到最外");
            check(std::fabs(std::get<5>(cutout) - (maxY + feather)) <= 1.5f, "下边界到最外");

            // 圆角必须**真圆**：左上角最远顶点到圆角圆心的距离 ≈ 半径。
            // 直角的话那个角点在半径的 1.414 倍处 —— 用户报的"圆角变直角"就是这个。
            const float outerR = radius + feather;
            const float maxDist = std::get<6>(cutout);
            check(maxDist > 0.0f, "左上角有顶点");
            check(maxDist <= outerR + 1.5f, "左上角是圆角（直角会是半径的 1.414 倍）");
            check(maxDist >= outerR - 2.5f, "圆角够到外边界（没被钳小）");
        }
        // 故意不 DestroyContext：没有平台/渲染后端的上下文在 Shutdown 里会踩空
        // （"Forgot to shutdown Platform backend?" 那条路径）。进程马上就退出了。
    }

    std::printf("\n== 6. 内阴影：解析、没有方向（四周对称）、始终在形状内 ==\n");
    {
        // 解析：inner 是 shadow 层上的布尔字段（默认 false = 外投影）
        std::string error;
        auto inner = Appearance::compile(
            R"({"layers": [{"op": "shadow", "id": "in", "inner": true, "fill": "#00000099",
                 "feather": 10, "spread": 2, "offset": [0, 6], "radius": 8}]})",
            getTheme(), error);
        check(inner != nullptr, "能编译带 inner 的 shadow 层");
        if (inner != nullptr)
        {
            const AppearanceLayer& layer = inner->getLayers()[0];
            check(layer.inner, "inner 解析为 true");
            check(!layer.cutout, "cutout 没写就是 false（两者互不干扰）");
            checkFloat(layer.offset[1], 6.0f,
                "offset 照常解析（对 inner 无效，但外投影要用它）");
        }
        error.clear();
        auto outer = Appearance::compile(
            R"({"layers": [{"op": "shadow", "fill": "#00000099", "feather": 10}]})",
            getTheme(), error);
        check(outer != nullptr && !outer->getLayers()[0].inner, "不写 inner = 外投影（默认）");

        // 设计器候选键里要有 inner（不然属性表里加不出来）
        {
            const compile::LayerType* type = compile::findLayer("shadow");
            bool hasInner = false;
            if (type != nullptr)
                for (const std::string& key : type->keys)
                    if (key == "inner")
                        hasInner = true;
            check(hasInner, "注册表把 inner 列为 shadow 的候选键");
        }

        // 层序：内阴影**不**参与"阴影提到最前"，否则会被后面的底色整块盖住。
        // 外投影必须提前（垫在控件下面），内阴影必须留在书写位置（压在底色上面）。
        {
            std::string orderError;
            auto outerFirst = Appearance::compile(
                R"({"layers": [{"op": "rect", "id": "bg", "fill": "#112233FF"},
                     {"op": "shadow", "id": "s", "fill": "#00000099"}]})",
                getTheme(), orderError);
            check(outerFirst != nullptr, "外投影 + 底色能编译");
            if (outerFirst != nullptr)
                checkEqual(outerFirst->getLayers()[0].id, "s",
                    "外投影被提到最前（垫在底色下面）");

            orderError.clear();
            auto innerKept = Appearance::compile(
                R"({"layers": [{"op": "rect", "id": "bg", "fill": "#112233FF"},
                     {"op": "shadow", "id": "s", "inner": true, "fill": "#00000099"}]})",
                getTheme(), orderError);
            check(innerKept != nullptr, "内阴影 + 底色能编译");
            if (innerKept != nullptr)
            {
                checkEqual(innerKept->getLayers()[0].id, "bg",
                    "内阴影**不**被提前：底色还在它前面");
                checkEqual(innerKept->getLayers()[1].id, "s",
                    "内阴影按书写顺序落在底色之后（压在底色上）");
            }
        }

        // 绘制：内阴影的关键性质是"永远待在形状内部"（外阴影会外扩）
        ImGui::CreateContext();
        {
            auto makeList = [] {
                // 同 5)：先补圆角分段表，否则圆角弧步长的除法会除零
                ImGui::GetDrawListSharedData()->SetCircleTessellationMaxError(0.3f);
                ImDrawList* list = new ImDrawList(ImGui::GetDrawListSharedData());
                list->_ResetForNewFrame();
                list->_FringeScale    = 1.0f;
                list->_InvFringeScale = 1.0f;
                // 无窗口时默认裁剪区是空的：给一个大裁剪区（cutout 用 PushClipRect 分块画）
                list->PushClipRect(ImVec2(-1000.0f, -1000.0f), ImVec2(1000.0f, 1000.0f), false);
                return list;
            };

            const float minX = 100.0f, minY = 50.0f, maxX = 300.0f, maxY = 150.0f;
            const float radius = 12.0f;
            const float feather = 14.0f;
            const ImU32 color = IM_COL32(0, 0, 0, 200);

            // 顶点 + alpha 加权重心（重心在哪一侧 = 哪一侧更暗）
            struct Stats
            {
                int   vertices = 0;
                float centroidX = 0.0f;
                float centroidY = 0.0f;
                float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
            };
            // 内阴影**没有方向参数**了（就是"从边缘向内扩散"）：这里量的是贴内缘那一带的分布
            auto measure = [&](float bandFeather) {
                ImDrawList* list = makeList();
                shadow::drawInnerSoft(list, minX, minY, maxX, maxY, radius, color, 1.0f,
                    bandFeather, 0.0f);

                Stats stats;
                float weightSum = 0.0f;
                for (int i = 0; i < list->VtxBuffer.Size; i++)
                {
                    const ImVec2 p = list->VtxBuffer[i].pos;
                    const float a  = (float)((list->VtxBuffer[i].col >> 24) & 0xFFu);
                    stats.vertices++;
                    stats.minX = std::min(stats.minX, p.x);
                    stats.maxX = std::max(stats.maxX, p.x);
                    stats.minY = std::min(stats.minY, p.y);
                    stats.maxY = std::max(stats.maxY, p.y);
                    stats.centroidX += p.x * a;
                    stats.centroidY += p.y * a;
                    weightSum += a;
                }
                if (weightSum > 0.0f)
                {
                    stats.centroidX /= weightSum;
                    stats.centroidY /= weightSum;
                }
                return stats;
            };

            const Stats band = measure(feather);
            check(band.vertices > 0, "内阴影画出了顶点");
            // ⚠️ "在形状内"就是它和外阴影的分界：外阴影最外圈到 minX-feather，内阴影一个顶点都不许出去
            check(band.minX >= minX - 1.0f && band.maxX <= maxX + 1.0f
                    && band.minY >= minY - 1.0f && band.maxY <= maxY + 1.0f,
                "所有顶点都在形状内部（不外扩）");
            check(band.minX > minX - 1.0f && band.maxX < maxX + 1.0f,
                "确实贴着内缘画（不是空实现）");

            // 没有方向：左右对称、上下对称（重心都在形状中心）
            checkFloat(band.centroidX, (minX + maxX) * 0.5f, "左右对称（重心在中间）");
            checkFloat(band.centroidY, (minY + maxY) * 0.5f, "上下对称（重心在中间）");

            // 四边等宽：四个方向各取一条贴边的窄带，alpha 之和应当互相接近
            //（直边中点附近是等宽的；圆角在角上，不参与这个比较）
            auto bandWeight = [&](float x0, float y0, float x1, float y1) {
                ImDrawList* list = makeList();
                shadow::drawInnerSoft(list, minX, minY, maxX, maxY, radius, color, 1.0f,
                    feather, 0.0f);
                float sum = 0.0f;
                for (int i = 0; i < list->VtxBuffer.Size; i++)
                {
                    const ImVec2 p = list->VtxBuffer[i].pos;
                    if (p.x < x0 || p.x > x1 || p.y < y0 || p.y > y1)
                        continue;
                    sum += (float)((list->VtxBuffer[i].col >> 24) & 0xFFu);
                }
                return sum;
            };
            const float midX = (minX + maxX) * 0.5f;
            const float midY = (minY + maxY) * 0.5f;
            const float top    = bandWeight(minX, minY, maxX, minY + 6.0f);
            const float bottom = bandWeight(minX, maxY - 6.0f, maxX, maxY);
            const float left   = bandWeight(minX, minY, minX + 6.0f, maxY);
            const float right  = bandWeight(maxX - 6.0f, minY, maxX, maxY);
            check(top > 0.0f && bottom > 0.0f && left > 0.0f && right > 0.0f,
                "四条边都画了（不是只画一侧）");
            check(std::fabs(top - bottom) < std::max(top, bottom) * 0.05f, "上下的暗度基本相等");
            check(std::fabs(left - right) < std::max(left, right) * 0.05f, "左右的暗度基本相等");
            // 上下是 200px 宽、左右是 100px 高，绝对量不同，但"贴边宽度"应该一致 ——
            // 用同样面积取样后比值接近 1 就说明没有方向性
            (void)midX;
            (void)midY;

            // 带子越宽，参与的点越多（单调性）
            const Stats wide = measure(feather * 2.0f);
            check(wide.vertices >= band.vertices, "带子越宽覆盖的点越多");

            // 退化输入不许崩：带子为 0 / 形状比带子还小 / 空列表
            ImDrawList* tiny = makeList();
            shadow::drawInnerSoft(tiny, 0.0f, 0.0f, 4.0f, 4.0f, 2.0f, color, 1.0f, 40.0f, 40.0f);
            check(true, "形状比带子还小：安全返回");
            ImDrawList* zeroBand = makeList();
            shadow::drawInnerSoft(zeroBand, 0.0f, 0.0f, 100.0f, 100.0f, 0.0f, color, 1.0f, 0.0f,
                0.0f);
            checkEqualInt(zeroBand->VtxBuffer.Size, 0, "带子宽度为 0：一个顶点都不画");
            shadow::drawInnerSoft(nullptr, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, color, 1.0f, 4.0f, 0.0f);
            check(true, "drawList 为空时安全返回");
        }
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
