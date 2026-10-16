// 无窗口测试：「不规则四边形」扩展层（demo/contrib/quad_layer.cpp）。
//
// 这个测试有两个目的：
//   ① 钉住随机游走的**纯函数性质**（确定性、范围、连续、四顶点互不相同、gain 生效）——
//      纯数学部分在 contrib/quad_layer.hpp 里，所以能直接断言，不用建窗口；
//   ② 证明"第三方层"这条路径真的能跑通：注册 → 主题 JSON 里写 op → 引擎解析参数 →
//      钩子拿到上下文并画进 ImDrawList。测试**直接编 demo 里那份源码**（见 CMakeLists
//      的 target_sources），而不是在测试里复制一份 —— 复制的那份会慢慢漂移
//      （stripe 的测试桩与样例就漂过：桩只认 fill，样例还读 angle）。
//
// 绘制那段照抄 shadow_test 的经验：要真 ImGui 上下文 + _ResetForNewFrame + _FringeScale，
// 否则自建列表要么崩、要么静默什么都不画。

#include <cmath>
#include <cstdio>
#include <string>

#include "contrib/quad_layer.hpp" // demo/contrib（测试的 include 目录里加了 demo/）

#include "bklv/compile/layer_registry.hpp"
#include "bklv/core/appearance.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

// demo 侧声明的两个入口（注册 + 纯数学在 contrib/quad_layer.hpp 里）
void ensureRegistered()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    demo::registerQuadLayer();
}

std::shared_ptr<const Appearance> compileQuad(const std::string& layerJson, std::string& error)
{
    return Appearance::compile("{ \"layers\": [ " + layerJson + " ] }", getTheme(), error);
}

} // namespace

int main()
{
    initResources();
    ensureRegistered();

    const auto& gainAll = std::vector<float>{ 1.0f, 1.0f, 1.0f, 1.0f };

    std::printf("\n== 1. 随机游走是纯函数：确定、有界、连续 ==\n");
    {
        // 同输入同输出（可复现：同一个 seed 每次跑出来一样）
        const float a = demo::quad::wanderOffset(7, 0, 0, 1.234f, 1.5f);
        const float b = demo::quad::wanderOffset(7, 0, 0, 1.234f, 1.5f);
        checkFloat(a, b, "同 (seed, 顶点, 轴, 时间) 得到同一个值");

        // 范围 [-1, 1]
        bool inRange = true;
        bool varied  = false;
        for (int i = 0; i < 400; i++)
        {
            const float v = demo::quad::wanderOffset(3, i % 4, i % 2, (float)i * 0.037f, 2.0f);
            if (v < -1.0f || v > 1.0f)
                inRange = false;
            if (std::fabs(v) > 0.2f)
                varied = true;
        }
        check(inRange, "所有取值都落在 [-1, 1]");
        check(varied, "取值确实在动（不是恒为 0）");

        // 连续：相邻时刻差一个很小的 dt 时，值不会跳（值噪声的段内插值）
        const float t0 = 3.0f;
        const float v0 = demo::quad::wanderOffset(11, 2, 1, t0, 1.0f);
        const float v1 = demo::quad::wanderOffset(11, 2, 1, t0 + 0.001f, 1.0f);
        check(std::fabs(v1 - v0) < 0.05f, "相邻时刻连续（差值 < 0.05）");

        // 不同顶点 / 不同轴 / 不同种子都要走不同的序列（否则四个顶点会一起动）
        const float vx = demo::quad::wanderOffset(5, 0, 0, 2.0f, 1.0f);
        const float vy = demo::quad::wanderOffset(5, 0, 1, 2.0f, 1.0f);
        const float v1st = demo::quad::wanderOffset(5, 1, 0, 2.0f, 1.0f);
        check(std::fabs(vx - vy) > 1e-4f, "同一顶点的 x / y 是两条序列");
        check(std::fabs(vx - v1st) > 1e-4f, "不同顶点的序列不同");
        const float other = demo::quad::wanderOffset(6, 0, 0, 2.0f, 1.0f);
        check(std::fabs(vx - other) > 1e-4f, "换 seed 换一套随机");

        // speed <= 0 = 不动（回到矩形本身，方便"先要形状不要动"）
        checkFloat(demo::quad::wanderOffset(9, 1, 0, 12.0f, 0.0f), 0.0f, "speed 0 时不动");
        checkFloat(demo::quad::wanderOffset(9, 1, 0, 12.0f, -3.0f), 0.0f, "speed 负数也不动");
    }

    std::printf("\n== 2. 四个顶点的偏移：gain 生效、默认全 1 ==\n");
    {
        const float gain[4] = { 1.0f, 0.5f, 2.0f, 0.0f };
        const demo::quad::VertexOffsets scaled = demo::quad::vertexOffsets(7, 4.2f, 1.5f, gain);
        const demo::quad::VertexOffsets plain  = demo::quad::vertexOffsets(7, 4.2f, 1.5f, nullptr);

        checkFloat(scaled.x[0], plain.x[0], "gain=1 的顶点与默认一致");
        checkFloat(scaled.x[1], plain.x[1] * 0.5f, "gain=0.5 减半");
        checkFloat(scaled.y[2], plain.y[2] * 2.0f, "gain=2 翻倍");
        checkFloat(scaled.x[3], 0.0f, "gain=0 的顶点钉在角上");
        checkFloat(scaled.y[3], 0.0f, "gain=0 的顶点钉在角上（y 也是）");
    }

    std::printf("\n== 3. 注册与参数解析（引擎按声明的模式解析 7 个参数）==\n");
    {
        const compile::LayerType* type = compile::findLayer("quad");
        check(type != nullptr, "quad 注册进了注册表");
        if (type != nullptr)
        {
            checkEqual(type->idPrefix, "quad", "默认 id 前缀");
            checkEqualSize(type->params.size(), 7, "声明了 7 个参数");
            checkEqualSize(type->keys.size(), 7, "keys 由参数派生（设计器候选自动跟上）");
        }

        std::string error;
        auto appearance = compileQuad(
            R"({ "op": "quad", "id": "blob", "amplitude": 10, "speed": 1.2, "seed": 7,
                 "gain1": 0.5, "fill": "#112233FF", "strokeWidth": 2 })",
            error);
        check(appearance != nullptr, "主题里写 op: quad 能编译");
        if (appearance != nullptr)
        {
            const AppearanceLayer& layer = appearance->getLayers()[0];
            checkEqualInt((int)layer.paramCount, 7, "参数槽个数 = 声明个数");
            checkFloat(layer.params[0].number, 10.0f, "amplitude 解析（uiScale=1）");
            checkFloat(layer.params[1].number, 1.2f, "speed 解析");
            checkFloat(layer.params[2].number, 7.0f, "seed 解析");
            checkFloat(layer.params[3].number, 1.0f, "没写的 gain0 用默认值 1");
            checkFloat(layer.params[4].number, 0.5f, "写了的 gain1 生效");
            checkColor(layer.fill, IM_COL32(0x11, 0x22, 0x33, 0xFF), "fill 走引擎字段");

            // 写错参数类型 → 编译失败（不是静默忽略）
            error.clear();
            check(compileQuad(R"({ "op": "quad", "amplitude": "大" })", error) == nullptr,
                "参数类型写错 -> 编译失败并报错");
        }
    }

    std::printf("\n== 4. 真的画一遍：钩子拿到上下文并产出顶点 ==\n");
    {
        // 照 shadow_test 的经验：要真上下文 + _ResetForNewFrame + _FringeScale
        ImGui::CreateContext();

        std::string error;
        auto appearance = compileQuad(
            R"({ "op": "quad", "id": "blob", "amplitude": 6, "speed": 1.0, "seed": 3,
                 "fill": "#3366CCFF", "stroke": "#FFFFFFFF", "strokeWidth": 2 })",
            error);
        check(appearance != nullptr, "绘制用的外观编译成功");
        if (appearance != nullptr)
        {
            AppearanceInstance instance;
            instance.setAppearance(appearance);

            ImDrawList* list = new ImDrawList(ImGui::GetDrawListSharedData());
            list->_ResetForNewFrame();
            list->_FringeScale = list->_InvFringeScale = 1.0f;

            const AppearanceLayer& layer = appearance->getLayers()[0];
            const compile::LayerType* type = compile::findLayer("quad");

            compile::LayerRect rect;
            rect.minX = 20.0f;
            rect.minY = 30.0f;
            rect.maxX = 120.0f;
            rect.maxY = 90.0f;
            rect.alpha = 1.0f;

            const int vtxBefore = list->VtxBuffer.Size;
            compile::LayerDrawContext ctx{ layer, instance, 0, list, rect, nullptr, nullptr,
                nullptr, nullptr, nullptr, type };
            type->draw(ctx);

            check(list->VtxBuffer.Size > vtxBefore, "画出了顶点（填充 + 描边）");
            check(list->CmdBuffer.Size > 0, "至少有一条绘制命令");

            // 顶点必须落在层矩形附近（幅度被夹住，不会飞到天外）
            bool inside = true;
            for (int i = 0; i < list->VtxBuffer.Size; i++)
            {
                const ImVec2& p = list->VtxBuffer[i].pos;
                if (p.x < rect.minX - 24.0f || p.x > rect.maxX + 24.0f
                    || p.y < rect.minY - 24.0f || p.y > rect.maxY + 24.0f)
                    inside = false;
            }
            check(inside, "顶点都在这一层的矩形附近（幅度被夹住）");

            // 幅度夹取：给一个夸张的 amplitude 也不该崩（内部夹到短边的 20%）
            error.clear();
            // 注意要给 fill：这个层不像内置 rect 有默认色，没写 fill 就什么都不画
            auto wild = compileQuad(
                R"({ "op": "quad", "amplitude": 5000, "gain0": 9, "gain1": -4, "seed": 2,
                     "fill": "#FF0000FF" })",
                error);
            check(wild != nullptr, "极端参数也能编译");
            if (wild != nullptr)
            {
                AppearanceInstance wildInstance;
                wildInstance.setAppearance(wild);
                ImDrawList* wildList = new ImDrawList(ImGui::GetDrawListSharedData());
                wildList->_ResetForNewFrame();
                wildList->_FringeScale = wildList->_InvFringeScale = 1.0f;

                compile::LayerDrawContext wildCtx{ wild->getLayers()[0], wildInstance, 0, wildList,
                    rect, nullptr, nullptr, nullptr, nullptr, nullptr, type };
                type->draw(wildCtx);
                check(wildList->VtxBuffer.Size > 0, "极端参数仍然画出了东西（没崩）");

                // gain 再大，顶点也被夹在层附近（否则形状会飞出可视区）
                bool wildInside = true;
                for (int i = 0; i < wildList->VtxBuffer.Size; i++)
                {
                    const ImVec2& p = wildList->VtxBuffer[i].pos;
                    if (p.x < rect.minX - 40.0f || p.x > rect.maxX + 40.0f
                        || p.y < rect.minY - 40.0f || p.y > rect.maxY + 40.0f)
                        wildInside = false;
                }
                check(wildInside, "gain 写多大都被夹在层附近（不会甩出去）");
            }
        }

        // 故意不 DestroyContext：没接平台/渲染后端的上下文在 Shutdown 里会踩空
        // （和 shadow_test 同一个理由，进程马上退出）
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
