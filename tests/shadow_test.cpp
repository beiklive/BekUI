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
        setenv("BKLV_SHADOW_QUALITY", "fast", 1);
        check(shadow::getQuality() == ShadowQuality::FAST, "环境变量 BKLV_SHADOW_QUALITY=fast 生效");
        unsetenv("BKLV_SHADOW_QUALITY");

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
                ImDrawList* list = new ImDrawList(ImGui::GetDrawListSharedData());
                list->_ResetForNewFrame();
                list->_FringeScale    = 1.0f;
                list->_InvFringeScale = 1.0f;
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
            auto covered = [](ImDrawList* list, const ImVec2& p) {
                auto sign = [](const ImVec2& a, const ImVec2& b, const ImVec2& c) {
                    return (a.x - c.x) * (b.y - c.y) - (b.x - c.x) * (a.y - c.y);
                };
                for (int i = 0; i + 2 < list->IdxBuffer.Size; i += 3)
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
                    if (insideRoundedShape(p))
                        inside++;
                    loX = std::min(loX, p.x); hiX = std::max(hiX, p.x);
                    loY = std::min(loY, p.y); hiY = std::max(hiY, p.y);
                    if (p.x <= center.x && p.y <= center.y)
                    {
                        const float dx = p.x - center.x, dy = p.y - center.y;
                        maxCornerDist = std::max(maxCornerDist, std::sqrt(dx * dx + dy * dy));
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
            check(std::get<8>(cutout), "cutout 时圆角外的角落仍有阴影（外圈完整）");
            check(std::get<0>(plain) >= 0, "两种模式的顶点都检查过");
            // 开挖空：控件圆角形状内部一个顶点都没有 → 透明底色也看不到阴影
            checkEqualInt(std::get<0>(cutout), 0, "cutout 时控件形状内部没有任何顶点");
            check(std::get<1>(cutout) > 0, "cutout 画出了东西（不是空实现）");

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
        // 故意不 DestroyContext：没有平台/渲染后端的上下文在 Shutdown 里会踩空
        // （"Forgot to shutdown Platform backend?" 那条路径）。进程马上就退出了。
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
