// 无窗口测试：`rect` 层的渐变（采样 / 参数 / 解析校验）。
//
// 这里钉住的是**纯逻辑**：渐变被算成每顶点颜色之前的那一步。
// 真正画出来是 ImDrawList 顶点色网格（GL / Switch-GLES3 / Vulkan 通吃），
// 像素验收在主题示例上用截图做（见 docs/rect-gradient.md）。

#include <cstdio>
#include <string>

#include "bklv/core/appearance.hpp"
#include "bklv/core/gradient.hpp"

#include "imgui.h"
#include "bklv/core/theme_data.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

Gradient twoStop(ImU32 from, ImU32 to)
{
    Gradient gradient;
    gradient.stopCount     = 2;
    gradient.stops[0]      = GradientStop{ 0.0f, from };
    gradient.stops[1]      = GradientStop{ 1.0f, to };
    return gradient;
}

int alphaOf(ImU32 color) { return (int)((color >> 24) & 0xFFu); }
int redOf(ImU32 color) { return (int)(color & 0xFFu); }
int greenOf(ImU32 color) { return (int)((color >> 8) & 0xFFu); }
int blueOf(ImU32 color) { return (int)((color >> 16) & 0xFFu); }

} // namespace

int main()
{
    std::printf("== 1. 采样：端点、中点、越界钳制 ==\n");
    {
        const Gradient gradient = twoStop(IM_COL32(0, 0, 0, 255), IM_COL32(255, 128, 64, 0));
        checkEqualInt(redOf(sampleGradient(gradient, 0.0f)), 0, "t=0 取第一个停靠点");
        checkEqualInt(redOf(sampleGradient(gradient, 1.0f)), 255, "t=1 取最后一个停靠点");
        checkEqualInt(alphaOf(sampleGradient(gradient, 1.0f)), 0, "alpha 也跟着渐变");
        checkEqualInt(redOf(sampleGradient(gradient, 0.5f)), 128, "中点线性插值");
        checkEqualInt(greenOf(sampleGradient(gradient, 0.5f)), 64, "中点绿通道线性插值");
        checkEqualInt(redOf(sampleGradient(gradient, -3.0f)), 0, "t<0 钳到 0");
        checkEqualInt(redOf(sampleGradient(gradient, 7.0f)), 255, "t>1 钳到 1");
    }

    std::printf("\n== 2. 采样：多段与无效渐变 ==\n");
    {
        Gradient gradient;
        gradient.stopCount = 3;
        gradient.stops[0]  = GradientStop{ 0.0f, IM_COL32(0, 0, 0, 255) };
        gradient.stops[1]  = GradientStop{ 0.25f, IM_COL32(100, 0, 0, 255) };
        gradient.stops[2]  = GradientStop{ 1.0f, IM_COL32(200, 0, 0, 255) };

        checkEqualInt(redOf(sampleGradient(gradient, 0.25f)), 100, "正好落在中间停靠点");
        checkEqualInt(redOf(sampleGradient(gradient, 0.125f)), 50, "第一段的中点");
        checkEqualInt(redOf(sampleGradient(gradient, 0.625f)), 150, "第二段的中点");
        check(redOf(sampleGradient(gradient, 0.6f)) < redOf(sampleGradient(gradient, 0.7f)),
            "第二段是单调的");

        Gradient empty;
        checkEqualInt(sampleGradient(empty, 0.5f), 0, "没有渐变（停靠点 < 2）返回 0");
        check(!empty.isValid(), "空渐变 isValid() 为 false");
        check(gradient.isValid(), "3 个停靠点是有效渐变");
    }

    std::printf("\n== 3. 参数 t：线性按方向投影，径向按距离 ==\n");
    {
        const ImVec2 min(0.0f, 0.0f);
        const ImVec2 max(200.0f, 100.0f);

        Gradient leftToRight = twoStop(0, 0);
        leftToRight.angle    = 0.0f;
        checkFloat(gradientParameter(leftToRight, ImVec2(0.0f, 40.0f), min, max), 0.0f,
            "0 度：左边缘 t=0");
        checkFloat(gradientParameter(leftToRight, ImVec2(200.0f, 40.0f), min, max), 1.0f,
            "0 度：右边缘 t=1");
        checkFloat(gradientParameter(leftToRight, ImVec2(100.0f, 40.0f), min, max), 0.5f,
            "0 度：中点 t=0.5");

        Gradient topToBottom = twoStop(0, 0);
        topToBottom.angle    = 90.0f;
        checkFloat(gradientParameter(topToBottom, ImVec2(50.0f, 0.0f), min, max), 0.0f,
            "90 度：上边缘 t=0");
        checkFloat(gradientParameter(topToBottom, ImVec2(50.0f, 100.0f), min, max), 1.0f,
            "90 度：下边缘 t=1");

        Gradient diagonal = twoStop(0, 0);
        diagonal.angle    = 45.0f;
        checkFloat(gradientParameter(diagonal, ImVec2(0.0f, 0.0f), min, max), 0.0f,
            "45 度：起点角 t=0");
        checkFloat(gradientParameter(diagonal, ImVec2(200.0f, 100.0f), min, max), 1.0f,
            "45 度：对角 t=1");

        Gradient radial = twoStop(0, 0);
        radial.type     = Gradient::Type::RADIAL;
        checkFloat(gradientParameter(radial, ImVec2(100.0f, 50.0f), min, max), 0.0f,
            "径向：中心 t=0");
        checkFloat(gradientParameter(radial, ImVec2(200.0f, 50.0f), min, max), 1.0f,
            "径向：到长边一半处 t=1");
    }

    std::printf("\n== 4. 枚举名：大小写与 _/- 都不敏感 ==\n");
    {
        Gradient::Type type = Gradient::Type::LINEAR;
        check(gradientTypeFromName("linear", type), "linear");
        check(gradientTypeFromName("RADIAL", type) && type == Gradient::Type::RADIAL, "RADIAL");
        check(gradientTypeFromName("Linear", type) && type == Gradient::Type::LINEAR, "Linear");
        check(!gradientTypeFromName("conic", type), "不认识的类型被拒绝");
        check(std::string(gradientTypeName(Gradient::Type::RADIAL)) == "radial", "名字反查");
    }

    std::printf("\n== 5. JSON 解析：写对了能读出来 ==\n");
    {
        const std::string json = R"({
            "layers": [{
                "id": "bg", "op": "rect", "radius": 8,
                "gradient": {
                    "type": "linear", "angle": 90,
                    "stops": [ { "at": 0.0, "color": "#3A60C8FF" },
                               { "at": 1.0, "color": "#FF00FFFF" } ]
                }
            }]
        })";
        std::string error;
        const auto appearance = Appearance::compile(json, getTheme(), error);
        check(appearance != nullptr, "带了 gradient 的外观能编译");
        if (appearance != nullptr)
        {
            const AppearanceLayer& layer = appearance->getLayers()[0];
            check(layer.gradient.isValid(), "解析出 2 个停靠点");
            checkEqualInt(layer.gradient.stopCount, 2, "停靠点数量");
            check(layer.gradient.type == Gradient::Type::LINEAR, "类型 linear");
            checkFloat(layer.gradient.angle, 90.0f, "角度 90");
            checkEqualInt(redOf(layer.gradient.stops[0].color), 0x3A, "第一个停靠点颜色");
            checkEqualInt(redOf(layer.gradient.stops[1].color), 0xFF, "第二个停靠点颜色");
            checkFloat(layer.gradient.stops[1].at, 1.0f, "第二个停靠点的位置");
        }
    }

    std::printf("\n== 6. JSON 解析：写错了必须报错（不静默降级）==\n");
    {
        std::string error;

        const auto tooFew = Appearance::compile(R"({"layers": [{"id": "bg", "op": "rect",
            "gradient": {"stops": [{"at": 0, "color": "#FFFFFFFF"}]}}]})",
            getTheme(), error);
        check(tooFew == nullptr, "只有 1 个停靠点 -> 失败");
        check(error.find("2~") != std::string::npos, "错误里说明了停靠点数量要求");

        error.clear();
        const auto unordered = Appearance::compile(R"({"layers": [{"id": "bg", "op": "rect",
            "gradient": {"stops": [{"at": 0.8, "color": "#FFFFFFFF"},
                                   {"at": 0.2, "color": "#000000FF"}]}}]})",
            getTheme(), error);
        check(unordered == nullptr, "at 不是递增 -> 失败");
        check(error.find("大") != std::string::npos, "错误里提示了 at 必须递增");

        error.clear();
        const auto outOfRange = Appearance::compile(R"({"layers": [{"id": "bg", "op": "rect",
            "gradient": {"stops": [{"at": -0.5, "color": "#FFFFFFFF"},
                                   {"at": 1.0, "color": "#000000FF"}]}}]})",
            getTheme(), error);
        check(outOfRange == nullptr, "at 越界 -> 失败");
        check(error.find("0..1") != std::string::npos, "错误里说明了 0..1 范围");

        error.clear();
        const auto badType = Appearance::compile(R"({"layers": [{"id": "bg", "op": "rect",
            "gradient": {"type": "conic", "stops": [{"at": 0, "color": "#FFFFFFFF"},
                                                    {"at": 1, "color": "#000000FF"}]}}]})",
            getTheme(), error);
        check(badType == nullptr, "不认识的 type -> 失败");
        check(error.find("linear") != std::string::npos, "错误里列出了可选值");

        error.clear();
        const auto tooMany = Appearance::compile(R"({"layers": [{"id": "bg", "op": "rect",
            "gradient": {"stops": [{"at": 0.0, "color": "#FFFFFFFF"},
                                   {"at": 0.3, "color": "#FFFFFFFF"},
                                   {"at": 0.6, "color": "#FFFFFFFF"},
                                   {"at": 0.9, "color": "#FFFFFFFF"},
                                   {"at": 1.0, "color": "#FFFFFFFF"}]}}]})",
            getTheme(), error);
        check(tooMany == nullptr, "超过 4 个停靠点 -> 失败");

        error.clear();
        const auto noColor = Appearance::compile(R"({"layers": [{"id": "bg", "op": "rect",
            "gradient": {"stops": [{"at": 0.0}, {"at": 1.0, "color": "#000000FF"}]}}]})",
            getTheme(), error);
        check(noColor == nullptr, "停靠点缺 color -> 失败");
    }

    std::printf("\n== 7. 网格：顶点数、角点颜色、圆角与羽边 ==\n");
    {
        // 画东西要有真 ImGui 上下文，且三件事缺一不可（见 AGENTS）：
        // 建上下文、自建列表紧跟 _ResetForNewFrame、_FringeScale 设成 1（否则描边静默不画）
        ImGui::CreateContext();
        {
            ImDrawList list(ImGui::GetDrawListSharedData());
            list._ResetForNewFrame();
            list._FringeScale    = 1.0f;
            list._InvFringeScale = 1.0f;

            Gradient straight = twoStop(IM_COL32(0, 0, 0, 255), IM_COL32(255, 255, 255, 255));
            straight.angle    = 90.0f;

            const int before = list.VtxBuffer.Size;
            addGradientRectMesh(&list, straight, ImVec2(0.0f, 0.0f), ImVec2(100.0f, 50.0f),
                0.0f, 1.0f);
            checkEqualInt(list.VtxBuffer.Size - before, 6, "直角轴对齐只发 6 个顶点（两个三角形）");
            if (list.VtxBuffer.Size - before == 6)
            {
                const ImDrawVert& first  = list.VtxBuffer[before + 0];
                const ImDrawVert& third  = list.VtxBuffer[before + 2];
                checkEqualInt(redOf(first.col), 0, "左上角是起点色");
                checkEqualInt(redOf(third.col), 255, "另一角是终点色");
                checkFloat(first.pos.x, 0.0f, "左上角位置");
                checkFloat(third.pos.y, 50.0f, "对角位置的 y");
            }

            const int beforeGrid = list.VtxBuffer.Size;
            Gradient diagonal     = straight;
            diagonal.angle        = 45.0f;
            addGradientRectMesh(&list, diagonal, ImVec2(0.0f, 0.0f), ImVec2(100.0f, 50.0f),
                0.0f, 1.0f);
            checkEqualInt(list.VtxBuffer.Size - beforeGrid, 8 * 8 * 6, "斜角走 8x8 网格（384 个顶点）");

            const int beforeRound = list.VtxBuffer.Size;
            addGradientRectMesh(&list, straight, ImVec2(0.0f, 0.0f), ImVec2(100.0f, 50.0f),
                12.0f, 1.0f);
            const int roundedVerts = list.VtxBuffer.Size - beforeRound;
            check(roundedVerts > 200, "圆角走三角扇 + 羽边（顶点数明显更多）");
            int transparentFringe = 0;
            for (int i = beforeRound; i < list.VtxBuffer.Size; i++)
                if (alphaOf(list.VtxBuffer[i].col) == 0)
                    transparentFringe++;
            check(transparentFringe > 0, "羽边顶点（alpha=0）存在");

            const int beforeScale = list.VtxBuffer.Size;
            addGradientRectMesh(&list, straight, ImVec2(0.0f, 0.0f), ImVec2(100.0f, 50.0f),
                0.0f, 0.5f);
            checkEqualInt(alphaOf(list.VtxBuffer[beforeScale].col), 128, "整体 alpha 会乘进来（255*0.5 四舍五入到 128）");
        }
        // 不收尾：没接平台后端的上下文在 ImGui::Shutdown() 里会踩空（AGENTS 明确写了）
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
