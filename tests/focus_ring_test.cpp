// 无窗口测试：焦点框（流光框）的几何、LUT 采样区间与绘制图元。
//
// 这一层是"纯函数 + ImDrawList"——不起窗口就能把形状、相位、接缝拆分全部钉住。
// 颜色本身来自贴图（不在这里断言），这里管的是**位置与 UV**对不对。

#include <algorithm>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/core/focus_ring.hpp"
#include "bklv/views/label.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

focus_ring::Outline makeOutline(float x, float y, float width, float height, float radius)
{
    focus_ring::Outline outline;
    outline.bounds       = Rect(x, y, width, height);
    outline.cornerRadius = radius;
    return outline;
}

// 只要求"是一张有效的 LUT 贴图"：绘制只写顶点与 UV，不需要真后端。
TextureRef fakeLut()
{
    TextureRef lut;
    lut.index   = 0;
    lut.texture = (ImTextureID)1;
    lut.width   = 512;
    lut.height  = 4;
    return lut;
}

// 顶点布局（focus_ring.cpp 的条带发射保证这个顺序）：
//   每个**横截面** 4 个顶点：farA(羽边外沿, alpha 0) / nearA(描边外缘, 实) /
//                            nearB(描边另一侧外缘, 实) / farB(羽边外沿, alpha 0)
// 相邻段**共用**同一个横截面（这是"连续条带"的关键），所以顶点数 = 横截面数 × 4，
// 索引按"每段 3 条带 × 6 索引"铺开。
constexpr int kVertsPerCross = 4;

// 扫一遍 UV（按索引读每个四边形两个顶点的 U），看有没有哪个四边形的 U 跨度不合理。// 扫一遍 UV，看有没有哪个贴图四边形的 U 跨度不合理（跨过整张贴图 = 被拉花）。
struct UvScan
{
    int quads       = 0;
    int smeared     = 0;   // U 倒着走，或者跨度远超一段
    float widest    = 0.0f;
};

UvScan scanUv(const focus_ring::Outline& outline, const focus_ring::Style& style, float phase,
    float maxSpan)
{
    ImDrawList list(ImGui::GetDrawListSharedData());
    list._ResetForNewFrame();
    list._FringeScale    = 1.0f;
    list._InvFringeScale = 1.0f;

    focus_ring::draw(&list, outline, style, phase);

    UvScan scan;
    // 每 6 个索引是一个四边形：取前两个索引对应的顶点 U 比较。
    for (int i = 0; i + 5 < list.IdxBuffer.Size; i += 6)
    {
        const ImDrawIdx ia = list.IdxBuffer[i];
        const ImDrawIdx ib = list.IdxBuffer[i + 1];
        if (ia >= list.VtxBuffer.Size || ib >= list.VtxBuffer.Size)
            continue;

        const float u0 = list.VtxBuffer[ia].uv.x;
        const float u1 = list.VtxBuffer[ib].uv.x;
        const float du = u1 - u0;

        scan.quads++;
        scan.widest = std::max(scan.widest, du);
        if (du < 0.0f || du > maxSpan + 0.01f)
            scan.smeared++;
    }
    return scan;
}

// 取一批顶点的 UV 与颜色，用来判断"换类型到底改了哪一部分"：
// 流光改 UV（颜色流动）、呼吸改 alpha（UV 不动）、静止两者都不动。
void collectVertices(const focus_ring::Outline& outline, const focus_ring::Style& style, float phase,
    std::vector<ImVec2>& uvs, std::vector<ImU32>& colors)
{
    ImDrawList list(ImGui::GetDrawListSharedData());
    list._ResetForNewFrame();
    list._FringeScale    = 1.0f;
    list._InvFringeScale = 1.0f;

    focus_ring::draw(&list, outline, style, phase);

    uvs.clear();
    colors.clear();
    for (int i = 0; i < list.VtxBuffer.Size; i++)
    {
        uvs.push_back(list.VtxBuffer[i].uv);
        colors.push_back(list.VtxBuffer[i].col);
    }
}

int countVertices(const focus_ring::Outline& outline, const focus_ring::Style& style, float phase)
{
    ImDrawList list(ImGui::GetDrawListSharedData());
    list._ResetForNewFrame();
    list._FringeScale    = 1.0f;
    list._InvFringeScale = 1.0f;

    focus_ring::draw(&list, outline, style, phase);
    return list.VtxBuffer.Size;
}

// 画一圈，重建中线折线：每个横截面的 (nearA + nearB) / 2。
std::vector<ImVec2> ringCenterline(const focus_ring::Outline& outline,
    const focus_ring::Style& style, float phase, std::vector<ImU32>* alphas = nullptr)
{
    ImDrawList list(ImGui::GetDrawListSharedData());
    list._ResetForNewFrame();
    list._FringeScale    = 1.0f;
    list._InvFringeScale = 1.0f;

    focus_ring::draw(&list, outline, style, phase);

    std::vector<ImVec2> points;
    for (int i = 0; i + kVertsPerCross - 1 < list.VtxBuffer.Size; i += kVertsPerCross)
    {
        const ImVec2& a = list.VtxBuffer[i + 1].pos; // nearA
        const ImVec2& b = list.VtxBuffer[i + 2].pos; // nearB
        points.push_back(ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f));
        if (alphas != nullptr)
        {
            alphas->push_back(list.VtxBuffer[i].col);     // farA
            alphas->push_back(list.VtxBuffer[i + 1].col); // nearA
            alphas->push_back(list.VtxBuffer[i + 2].col); // nearB
            alphas->push_back(list.VtxBuffer[i + 3].col); // farB
        }
    }
    return points;
}

// 连续条带的两个结构不变量（以前"每段各自发四边形"时都不成立）：
//   ① 每个四边形的两个端点 U 单调、跨度不超过一段；
//   ② 同一个横截面只发一次（顶点不重复）—— 重复就意味着又回到了"每段封口"，
//      封口叠在相邻段的斜坡上就是那圈毛刺。
int countDuplicateCrossSections(const focus_ring::Outline& outline, const focus_ring::Style& style,
    float phase)
{
    ImDrawList list(ImGui::GetDrawListSharedData());
    list._ResetForNewFrame();
    list._FringeScale    = 1.0f;
    list._InvFringeScale = 1.0f;

    focus_ring::draw(&list, outline, style, phase);

    int duplicates = 0;
    const int crosses = list.VtxBuffer.Size / kVertsPerCross;
    for (int i = 0; i < crosses; i++)
    {
        const ImVec2& ai = list.VtxBuffer[i * kVertsPerCross + 1].pos;
        for (int j = i + 1; j < crosses; j++)
        {
            const ImVec2& bj = list.VtxBuffer[j * kVertsPerCross + 1].pos;
            const float dx = ai.x - bj.x, dy = ai.y - bj.y;
            if (std::sqrt(dx * dx + dy * dy) < 0.01f)
                duplicates++;
        }
    }
    return duplicates;
}

// 每个四边形铺出来的带子是否完整：索引数 / 6 应当等于"段数 × 3 条带"。
int countQuads(const focus_ring::Outline& outline, const focus_ring::Style& style, float phase)
{
    ImDrawList list(ImGui::GetDrawListSharedData());
    list._ResetForNewFrame();
    list._FringeScale    = 1.0f;
    list._InvFringeScale = 1.0f;

    focus_ring::draw(&list, outline, style, phase);
    return list.IdxBuffer.Size / 6;
}

} // namespace

int main()
{
    std::printf("== 1. 相位：按流动周期折成 [0,1) ==\n");
    {
        checkFloat(focus_ring::phaseForTime(0.0f, 1.8f), 0.0f, "0 秒 -> 相位 0");
        checkFloat(focus_ring::phaseForTime(0.9f, 1.8f), 0.5f, "半圈 -> 相位 0.5");
        checkFloat(focus_ring::phaseForTime(1.8f, 1.8f), 0.0f, "整圈回到 0");
        checkFloat(focus_ring::phaseForTime(4.5f, 1.8f), 0.5f, "两圈半 -> 相位 0.5");
        checkFloat(focus_ring::phaseForTime(-0.45f, 1.8f), 0.75f, "负时间也折回 [0,1)");
        checkFloat(focus_ring::phaseForTime(3.0f, 0.0f), 0.0f, "周期 <= 0 等于不流动");
    }

    std::printf("\n== 2. 周长：矩形 / 圆角矩形 / 正圆（半径自动钳制） ==\n");
    {
        checkFloat(focus_ring::perimeter(makeOutline(0, 0, 100, 50, 0)), 300.0f, "矩形 = 2(w+h)");
        checkFloat(focus_ring::perimeter(makeOutline(0, 0, 100, 50, 10)),
            160.0f + 60.0f + 2.0f * 3.14159265f * 10.0f, "圆角矩形 = 直边 + 2πr");
        // 半径超出短边一半时按短边一半算：50x50 + radius 999 就是一个正圆
        checkFloat(focus_ring::perimeter(makeOutline(0, 0, 50, 50, 999.0f)),
            2.0f * 3.14159265f * 25.0f, "50x50 的圆 = 2πr（r=25）");
        checkFloat(focus_ring::perimeter(makeOutline(0, 0, 48, 48, 24.0f)),
            2.0f * 3.14159265f * 24.0f, "48x48 圆角 24 也是正圆");
    }

    std::printf("\n== 3. 沿周长取点（t=0 在上边起点，顺时针） ==\n");
    {
        const focus_ring::Outline rect = makeOutline(10.0f, 20.0f, 100.0f, 50.0f, 0.0f);
        const Point start              = focus_ring::pointAt(rect, 0.0f);
        checkFloat(start.x, 10.0f, "t=0 的 x");
        checkFloat(start.y, 20.0f, "t=0 的 y（上边）");

        const Point quarter = focus_ring::pointAt(rect, 0.25f);
        checkFloat(quarter.x, 85.0f, "t=0.25 落到上边 3/4 处");
        checkFloat(quarter.y, 20.0f, "仍然在上边");

        const Point half = focus_ring::pointAt(rect, 0.5f);
        checkFloat(half.x, 110.0f, "t=0.5 在右上角（周长正好一半）");
        checkFloat(half.y, 70.0f, "y 到底边");

        // 圆：t=0.25 应该落在 3 点钟方向
        const Point circle = focus_ring::pointAt(makeOutline(0, 0, 50, 50, 999.0f), 0.25f);
        checkFloat(circle.x, 50.0f, "圆的 t=0.25 在右侧");
        checkFloat(circle.y, 25.0f, "圆的 t=0.25 在垂直中间");

        // 越界与负数都要折回来，不能画到别处
        const Point wrapped = focus_ring::pointAt(rect, 1.25f);
        checkFloat(wrapped.x, 85.0f, "t>1 折回");
        checkFloat(wrapped.y, 20.0f, "t>1 折回（y）");
    }

    std::printf("\n== 4. 分段数：自动按周长，显式优先，且有上下限 ==\n");
    {
        focus_ring::Style style;
        const int small = focus_ring::segmentCount(makeOutline(0, 0, 20, 20, 0), style);
        check(small >= focus_ring::kMinSegments, "小控件也不少于下限");
        check(small <= focus_ring::kMaxSegments, "小控件不超过上限");

        const int huge = focus_ring::segmentCount(makeOutline(0, 0, 4000, 2000, 0), style);
        check(huge == focus_ring::kMaxSegments, "超大控件钳到上限");

        style.segments = 8;
        check(focus_ring::segmentCount(makeOutline(0, 0, 4000, 2000, 0), style) == 8,
            "显式分段数优先");
    }

    std::printf("\n== 5. LUT 采样区间：跨接缝要拆 ==\n");
    {
        const focus_ring::LutSpan plain = focus_ring::lutSpanAt(0.1f, 0.2f, 0.5f);
        checkFloat(plain.u0, 0.6f, "u0 = t0 + 相位");
        checkFloat(plain.u1, 0.7f, "u1 = t1 + 相位");
        check(!plain.wrapped, "没跨接缝");

        const focus_ring::LutSpan across = focus_ring::lutSpanAt(0.0f, 0.5f, 0.75f);
        checkFloat(across.u0, 0.75f, "跨接缝时的 u0");
        checkFloat(across.u1, 0.25f, "跨接缝时的 u1");
        check(across.wrapped, "u1 < u0 就是要拆开画");

        // t1 = 1 就是绕回起点，UV 也回到 0 —— 这一段确实跨了接缝（画的时候后半段是退化的，会跳过）
        const focus_ring::LutSpan edge = focus_ring::lutSpanAt(0.9f, 1.0f, 0.0f);
        checkFloat(edge.u0, 0.9f, "t=0.9 的 u");
        checkFloat(edge.u1, 0.0f, "t=1 回到 0");
        check(edge.wrapped, "绕回起点也算跨接缝（拆开画）");
    }

    std::printf("\n== 5b. 换类型：流光动 UV、呼吸动 alpha、静止都不动 ==\n");
    {
        ImGui::CreateContext();

        const focus_ring::Outline outline = makeOutline(10, 10, 80, 40, 6.0f);
        focus_ring::Style style;
        style.lut      = fakeLut();
        style.segments = 12;

        std::vector<ImVec2> uvsA, uvsB;
        std::vector<ImU32> colorsA, colorsB;

        // 流光：相位一变，UV 跟着变（颜色沿着边框走）
        style.type = focus_ring::Type::FLOW;
        collectVertices(outline, style, 0.0f, uvsA, colorsA);
        collectVertices(outline, style, 0.4f, uvsB, colorsB);
        // 注意：相位不同 -> 跨接缝的那一段落在不同位置，顶点数可能不同（拆分的后半段有时退化），
        // 所以只比公共前缀里"有没有 UV 变化"。
        const size_t flowCommon = std::min(uvsA.size(), uvsB.size());
        bool flowUvChanged      = false;
        for (size_t i = 0; i < flowCommon; i++)
            flowUvChanged = flowUvChanged || uvsA[i].x != uvsB[i].x;
        check(flowCommon > 0 && flowUvChanged, "流光：相位改变采样位置（UV 变了）");

        // 静止：相位不影响 UV
        style.type = focus_ring::Type::SOLID;
        collectVertices(outline, style, 0.0f, uvsA, colorsA);
        collectVertices(outline, style, 0.4f, uvsB, colorsB);
        const size_t solidCommon = std::min(uvsA.size(), uvsB.size());
        bool solidUvSame         = solidCommon > 0;
        for (size_t i = 0; i < solidCommon && solidUvSame; i++)
            solidUvSame = uvsA[i].x == uvsB[i].x;
        check(solidUvSame, "静止：相位不影响 UV");

        // 呼吸：UV 不动，透明度在变（颜色分量不同）
        style.type = focus_ring::Type::BREATHING;
        collectVertices(outline, style, 0.0f, uvsA, colorsA);
        collectVertices(outline, style, 0.25f, uvsB, colorsB);
        const size_t breathCommon = std::min(uvsA.size(), uvsB.size());
        bool breathUvSame         = breathCommon > 0;
        for (size_t i = 0; i < breathCommon && breathUvSame; i++)
            breathUvSame = uvsA[i].x == uvsB[i].x;
        check(breathUvSame, "呼吸：UV 不动（还是同一张贴图）");
        check(colorsA.size() > 1 && colorsB.size() > 1 && colorsA[1] != colorsB[1],
            "呼吸：透明度在随时间起伏");

        // 三种类型画出来的顶点数一致（几何共用一条路径）
        style.type = focus_ring::Type::FLOW;
        const int flowCount = countVertices(outline, style, 0.0f);
        style.type          = focus_ring::Type::SOLID;
        const int solidCount = countVertices(outline, style, 0.0f);
        style.type           = focus_ring::Type::BREATHING;
        const int breathCount = countVertices(outline, style, 0.0f);
        check(flowCount == solidCount && flowCount == breathCount,
            "换类型不改变几何（共用同一条描边路径）");

        // 自定义渲染器：挂上就由它画
        style.type = focus_ring::Type::CUSTOM;
        check(countVertices(outline, style, 0.0f) == 0, "CUSTOM 没挂渲染器 -> 不画");
        int customCalls = 0;
        style.customRenderer = [&customCalls](ImDrawList* list, const focus_ring::Outline&,
                                   const focus_ring::Style&, float) {
            customCalls++;
            list->AddCircleFilled(ImVec2(0.0f, 0.0f), 4.0f, IM_COL32_WHITE);
        };
        check(countVertices(outline, style, 0.0f) > 0 && customCalls > 0,
            "CUSTOM 挂了渲染器 -> 交给它画");
    }

    std::printf("\n== 6. 绘制：贴图无效/关掉时一个顶点都不画 ==\n");
    {
        focus_ring::Outline outline = makeOutline(10, 10, 48, 48, 24.0f /* 圆 */);
        focus_ring::Style style;
        style.type = focus_ring::Type::FLOW;

        check(countVertices(outline, style, 0.0f) == 0, "没贴图 -> 不画（不退回自己算的颜色）");

        style.lut = fakeLut();
        const int flow = countVertices(outline, style, 0.0f);
        check(flow > 0, "有贴图 -> 画出来了");

        style.type = focus_ring::Type::NONE;
        check(countVertices(outline, style, 0.0f) == 0, "类型 NONE -> 不画");

        style.type  = focus_ring::Type::FLOW;
        style.alpha = 0.0f;
        check(countVertices(outline, style, 0.0f) == 0, "全透明 -> 不画");

        // 分段数越多顶点越多：证明 style.segments 真的走到绘制里
        focus_ring::Style coarse = style;
        coarse.alpha             = 1.0f;
        coarse.segments          = 8;
        focus_ring::Style fine   = coarse;
        fine.segments            = 64;
        check(countVertices(outline, fine, 0.0f) > countVertices(outline, coarse, 0.0f),
            "分段越多顶点越多");

        // 呼吸态也走贴图（只是透明度在变），一样要画出来
        focus_ring::Style breathing = style;
        breathing.type              = focus_ring::Type::BREATHING;
        breathing.alpha             = 1.0f;
        check(countVertices(outline, breathing, 0.25f) > 0, "呼吸态用同一张贴图");

        // 接缝拆分：闭环里任何相位都**恰好有一段**跨接缝（所以不能靠"顶点数变多"判断），
        // 真正的不变量是"没有任何一个四边形的 U 跨过整张贴图"——没拆的话那条会倒着走。
        focus_ring::Style seam = style;
        seam.alpha             = 1.0f;
        seam.segments          = 4;
        const float maxSpan    = 1.0f / 4.0f;

        int totalQuads = 0;
        for (float phase : { 0.0f, 0.13f, 0.25f, 0.5f, 0.75f, 0.99f })
        {
            const UvScan scan = scanUv(outline, seam, phase, maxSpan);
            totalQuads += scan.quads;
            check(scan.quads > 0 && scan.smeared == 0, "接缝处的段被拆开画，UV 没有拉花");
        }
        check(totalQuads > 0, "各个相位都画出了四边形");

        // 注意：不调 ImGui::DestroyContext()（没接平台后端的上下文会在 Shutdown 里踩空）
    }

    std::printf("\n== 7. 形状跟着布局与变换走（控件只回答「我是什么形状」） ==\n");
    {
        Box* row = new Box(Axis::ROW);
        row->setWidth(400.0f);
        row->setHeight(60.0f);

        Label* child = new Label();
        child->setText("x");
        child->setWidth(100.0f);
        child->setHeight(40.0f);
        row->addView(child);

        YGNodeCalculateLayout(row->getYGNode(), 400.0f, 60.0f, YGDirectionLTR);

        const focus_ring::Outline plain = child->getFocusOutline();
        checkFloat(plain.bounds.width, 100.0f, "默认形状 = 自己的世界矩形（宽）");
        checkFloat(plain.bounds.height, 40.0f, "默认形状 = 自己的世界矩形（高）");
        checkFloat(plain.cornerRadius, 0.0f, "默认没有圆角（外观驱动的控件才会补）");
        checkFloat(plain.scale, 1.0f, "没有变换时缩放是 1");

        // 放大（聚焦反馈就是这个通道）：形状跟着放大，半径也要跟着放大
        child->setScaleX(1.5f);
        child->setScaleY(1.5f);
        const focus_ring::Outline scaled = child->getFocusOutline();
        checkFloat(scaled.bounds.width, 150.0f, "放大 1.5 倍后形状也放大");
        checkFloat(scaled.scale, 1.5f, "缩放系数上报给画框的人（用来放大圆角半径）");
        checkFloat(scaled.bounds.x + scaled.bounds.width * 0.5f,
            plain.bounds.x + plain.bounds.width * 0.5f, "以中心为锚点放大");

        // 位移（滚动/转场都走这条）：形状要跟着走
        row->setTranslation(30.0f, 12.0f);
        const focus_ring::Outline moved = child->getFocusOutline();
        checkFloat(moved.bounds.x, scaled.bounds.x + 30.0f, "父级位移后 x 跟着走");
        checkFloat(moved.bounds.y, scaled.bounds.y + 12.0f, "父级位移后 y 跟着走");

        delete row;
    }

    std::printf("\n== 8. 几何精度：连续条带（无封口毛刺）、圆按转角细分 ==\n");
    {
        ImGui::CreateContext();

        focus_ring::Style style;
        style.type      = focus_ring::Type::SOLID; // 相位固定，几何与颜色都稳定
        style.lut       = fakeLut();
        style.lineWidth = 6.0f;
        style.expand    = 2.0f;
        style.alpha     = 1.0f;
        const float band = 1.5f; // 测试里 _FringeScale = 1 -> 羽边每边 1.5px（kBandPhysicalPixels）

        // ① 同一个横截面只发一次：连续条带的结构证明（"每段各自发四边形"必然产生重复顶点）
        // ② 四边形数 = 横截面数 × 3（外羽边 / 实心 / 内羽边 三条都铺满）
        // ③ 描边等宽、羽边固定 1px、羽边外沿 alpha = 0
        for (const auto& shape : { makeOutline(0, 0, 200, 80, 14.0f), makeOutline(0, 0, 48, 48, 24.0f),
                 makeOutline(0, 0, 300, 60, 0.0f) }) // 正圆 / 圆角矩形 / 直角矩形
        {
            // 只有 U 接缝那一处会复制一份横截面（位置完全相同、只差 UV）——
            // 那不是"段封口"：两段几何重合、都是不透明的，不会出现两条 alpha 斜坡叠加。
            checkEqualInt(countDuplicateCrossSections(shape, style, 0.0f), 1,
                "只有 U 接缝复制一次横截面（其余段全部共享）");

            const int crosses = countVertices(shape, style, 0.0f) / kVertsPerCross;
            check(crosses >= 12, "一圈至少 12 个横截面");
            // 每段 3 条带（外羽边/实心/内羽边）；U 接缝那一处会多拆出一段，所以允许 +1
            const int spans = countQuads(shape, style, 0.0f) / 3;
            check(spans == crosses || spans == crosses + 1,
                "三条带都铺满（条带数 = 横截面数，最多 +1 给 U 接缝拆段）");
        }

        // ④ 每个横截面的 4 个顶点：羽边外侧 alpha 0、描边两侧实心、描边等宽、羽边 1px
        {
            const focus_ring::Outline shape = makeOutline(0, 0, 160, 48, 8.0f);
            ImDrawList list(ImGui::GetDrawListSharedData());
            list._ResetForNewFrame();
            list._FringeScale    = 1.0f;
            list._InvFringeScale = 1.0f;
            focus_ring::draw(&list, shape, style, 0.0f);

            checkEqualInt(list.VtxBuffer.Size % kVertsPerCross, 0, "每个横截面固定 4 个顶点");

            bool alphaOk  = true;
            bool widthOk  = true;
            bool bandOk   = true;
            for (int i = 0; i + 3 < list.VtxBuffer.Size; i += 4)
            {
                const ImU32 a0 = (list.VtxBuffer[i].col >> 24) & 0xFFu;
                const ImU32 a1 = (list.VtxBuffer[i + 1].col >> 24) & 0xFFu;
                const ImU32 a2 = (list.VtxBuffer[i + 2].col >> 24) & 0xFFu;
                const ImU32 a3 = (list.VtxBuffer[i + 3].col >> 24) & 0xFFu;
                if (a0 != 0 || a3 != 0 || a1 == 0 || a2 == 0)
                    alphaOk = false;

                const ImVec2& nearA = list.VtxBuffer[i + 1].pos;
                const ImVec2& nearB = list.VtxBuffer[i + 2].pos;
                const ImVec2& farA  = list.VtxBuffer[i].pos;
                const ImVec2& farB  = list.VtxBuffer[i + 3].pos;

                const float width = std::sqrt((nearA.x - nearB.x) * (nearA.x - nearB.x)
                    + (nearA.y - nearB.y) * (nearA.y - nearB.y));
                if (std::fabs(width - style.lineWidth) > 0.05f)
                    widthOk = false;

                const float bandA = std::sqrt((farA.x - nearA.x) * (farA.x - nearA.x)
                    + (farA.y - nearA.y) * (farA.y - nearA.y));
                const float bandB = std::sqrt((farB.x - nearB.x) * (farB.x - nearB.x)
                    + (farB.y - nearB.y) * (farB.y - nearB.y));
                if (std::fabs(bandA - band) > 0.05f || std::fabs(bandB - band) > 0.05f)
                    bandOk = false;
            }
            check(alphaOk, "羽边外沿 alpha = 0、描边两侧实心");
            check(widthOk, "描边宽度处处等于 lineWidth");
            check(bandOk, "羽边宽度处处等于 1.5px");
        }

        // ⑤ 圆按**转角**细分：中线每段转角 <= 6°，且每个中点都落在圆上（不是折线）
        {
            const focus_ring::Outline circle = makeOutline(0, 0, 128, 128, 64.0f);
            const std::vector<ImVec2> center = ringCenterline(circle, style, 0.0f);
            check(center.size() >= 40, "128 圆切出足够多的段");

            float maxTurn = 0.0f;
            for (size_t i = 0; i < center.size(); i++)
            {
                const ImVec2& a = center[(i + center.size() - 1) % center.size()];
                const ImVec2& b = center[i];
                const ImVec2& c = center[(i + 1) % center.size()];
                const float ax = b.x - a.x, ay = b.y - a.y;
                const float bx = c.x - b.x, by = c.y - b.y;
                const float la = std::sqrt(ax * ax + ay * ay);
                const float lb = std::sqrt(bx * bx + by * by);
                if (la < 1e-4f || lb < 1e-4f)
                    continue;
                const float dot = (ax * bx + ay * by) / (la * lb);
                maxTurn = std::max(maxTurn,
                    std::acos(std::max(-1.0f, std::min(1.0f, dot))) * 180.0f / 3.14159265f);
            }
            check(maxTurn <= 6.0f, "圆上每段转角 <= 6°（看不到折线）");

            // 圆度：所有中线点到圆心的距离都等于 半径 + 半个线宽（形状真的跟着控件走）
            const float cx = 64.0f, cy = 64.0f;
            // 中线半径 = 原半径 + (外扩缝 + 半个线宽)：draw() 里把形状整体外扩了这么多
            const float expect = 64.0f + style.expand + style.lineWidth * 0.5f;
            float maxError = 0.0f;
            for (const ImVec2& p : center)
            {
                const float dx = p.x - cx, dy = p.y - cy;
                maxError = std::max(maxError, std::fabs(std::sqrt(dx * dx + dy * dy) - expect));
            }
            check(maxError < 0.6f, "中线始终落在圆上（圆度误差 < 0.6px）");
        }
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
