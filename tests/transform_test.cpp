// 无窗口测试：绘制期变换（缩放 / 旋转 / 斜切 / 位移）。
//
// 分两层：
//   1) 纯数学：Transform2D::apply 的缩放/旋转/斜切/位移与中心点
//   2) 真绘制：用 ImDrawList 画一个外观驱动的矩形，检查顶点真的被变换了，
//      并验证父子叠加的顺序（父 ∘ 子）

#include <cmath>
#include <cstdio>

#include "bklv/core/appearance.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/view.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/transform.hpp"

#include "imgui.h"
#include "imgui_internal.h"

using namespace bklv;

#include "test_util.hpp"

namespace
{

constexpr float PI = 3.14159265f;

// 画一次并返回顶点（无窗口：自己给 ImDrawList 一份 shared data）
struct DrawResult
{
    ImDrawListSharedData shared;
    ImDrawList list{ &shared };
    FrameContext ctx;

    DrawResult()
    {
        shared.TexUvWhitePixel = ImVec2(0.0f, 0.0f);
        shared.ClipRectFullscreen = ImVec4(-10000.0f, -10000.0f, 10000.0f, 10000.0f);
        // 手工构造的 ImDrawList 必须先做一次帧初始化：_CmdHeader / 裁剪栈
        // 指向内部缓冲区，不做的话 PrimReserve 会空指针（踩过）
        list._ResetForNewFrame();
        ctx.drawList = &list;
    }
};

} // namespace

int main()
{
    bklv::initResources();
    initI18n();
    setLocale("zh-Hans");
    setTheme("default");

    std::printf("== 1. 变换数学 ==\n");
    {
        Transform2D t;
        check(t.isIdentity(), "默认是单位变换");
        checkFloat(t.apply(10.0f, 20.0f, Point(0.0f, 0.0f)).x, 10.0f, "单位变换不动点");

        // 位移
        t.setTranslationX(5.0f);
        t.setTranslationY(-3.0f);
        checkFloat(t.apply(10.0f, 20.0f, Point(0.0f, 0.0f)).x, 15.0f, "位移 x");
        checkFloat(t.apply(10.0f, 20.0f, Point(0.0f, 0.0f)).y, 17.0f, "位移 y");
        check(!t.isIdentity(), "位移后不再是单位变换");

        // 缩放：以中心为基准
        t.reset();
        t.setScaleX(2.0f);
        t.setScaleY(0.5f);
        const Point scaled = t.apply(20.0f, 20.0f, Point(10.0f, 10.0f));
        checkFloat(scaled.x, 30.0f, "以中心缩放 x：10 + (20-10)*2");
        checkFloat(scaled.y, 15.0f, "以中心缩放 y：10 + (20-10)*0.5");

        // 绕中心旋转 90°：(20,10) 绕 (10,10) 转 90° → (10,20)
        t.reset();
        t.setRotate(PI * 0.5f);
        const Point rotated = t.apply(20.0f, 10.0f, Point(10.0f, 10.0f));
        checkFloat(rotated.x, 10.0f, "旋转 90° 后 x");
        checkFloat(rotated.y, 20.0f, "旋转 90° 后 y");

        // 斜切：skewX 让 x 随 y 偏移
        t.reset();
        t.setSkewX(PI * 0.25f); // tan(45°) = 1
        const Point skewed = t.apply(0.0f, 10.0f, Point(0.0f, 0.0f));
        checkFloat(skewed.x, 10.0f, "skewX 45° 时 x 偏移量 = y");

        // 自定义中心
        t.reset();
        t.setScaleX(3.0f);
        Transform2D custom = t;
        custom.origin    = Point(0.0f, 0.0f);
        custom.hasOrigin = true;
        checkFloat(custom.apply(10.0f, 0.0f, Point(100.0f, 100.0f)).x, 30.0f,
            "自定义中心优先于控件中心");
    }

    std::printf("\n== 2. 真绘制：顶点被变换 ==\n");
    {
        DrawResult result;

        // 用纯色 Box 当被变换的对象：它只画一个不圆角的矩形（4 个顶点），
        // 不碰字体图集，headless 也能跑
        Box* rect = new Box(Axis::COLUMN);
        rect->setWidth(100.0f);
        rect->setHeight(50.0f);
        rect->setBackgroundColor(IM_COL32(255, 255, 255, 255));
        rect->setPositionType(PositionType::ABSOLUTE);
        rect->setPositionLeft(0.0f);
        rect->setPositionTop(0.0f);

        // 要先跑一次布局：getX/getY 读的是 Yoga 的计算结果（不跑就是 NaN）
        YGNodeCalculateLayout(rect->getYGNode(), 400.0f, 300.0f, YGDirectionLTR);

        // 原样画一遍，记住第一段顶点的位置
        rect->frame(&result.ctx);
        const int plainVtxCount = result.list.VtxBuffer.Size;
        check(plainVtxCount > 0, "矩形画出了顶点");
        const ImVec2 plainFirst = result.list.VtxBuffer[0].pos;

        // 位移 (20, 10)：所有顶点都平移
        result.list.VtxBuffer.clear();
        result.list.IdxBuffer.clear();
        rect->setTranslateX(20.0f);
        rect->setTranslateY(10.0f);
        rect->frame(&result.ctx);
        checkEqualSize((size_t)result.list.VtxBuffer.Size, (size_t)plainVtxCount, "顶点数不变");
        checkFloat(result.list.VtxBuffer[0].pos.x, plainFirst.x + 20.0f, "位移后顶点 x +20");
        checkFloat(result.list.VtxBuffer[0].pos.y, plainFirst.y + 10.0f, "位移后顶点 y +10");

        // 缩放 2 倍（中心不变的那个角会移动）
        result.list.VtxBuffer.clear();
        result.list.IdxBuffer.clear();
        rect->resetTransform();
        rect->setScaleX(2.0f);
        rect->setScaleY(2.0f);
        rect->frame(&result.ctx);

        float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
        for (int i = 0; i < result.list.VtxBuffer.Size; i++)
        {
            minX = std::min(minX, result.list.VtxBuffer[i].pos.x);
            maxX = std::max(maxX, result.list.VtxBuffer[i].pos.x);
            minY = std::min(minY, result.list.VtxBuffer[i].pos.y);
            maxY = std::max(maxY, result.list.VtxBuffer[i].pos.y);
        }
        checkFloat(maxX - minX, 200.0f, "缩放 2 倍后宽度翻倍");
        checkFloat(maxY - minY, 100.0f, "缩放 2 倍后高度翻倍");
        checkFloat((minX + maxX) * 0.5f, 50.0f, "以中心缩放：中心不动");

        delete rect;
    }

    std::printf("\n== 3. 父子叠加顺序（父 ∘ 子）==\n");
    {
        DrawResult result;

        // 父节点不画东西：这样缓冲区里只有子节点的 4 个顶点，方便断言
        Box* parent = new Box(Axis::COLUMN);
        parent->setWidth(200.0f);
        parent->setHeight(200.0f);
        parent->setPositionType(PositionType::ABSOLUTE);
        parent->setPositionLeft(0.0f);
        parent->setPositionTop(0.0f);

        Box* child = new Box(Axis::COLUMN);
        child->setWidth(20.0f);
        child->setHeight(20.0f);
        child->setBackgroundColor(IM_COL32(255, 255, 255, 255));
        child->setPositionType(PositionType::ABSOLUTE);
        child->setPositionLeft(0.0f);
        child->setPositionTop(0.0f);

        parent->addView(child);

        // 子先平移 +10，父整体放大 2 倍（父中心 (100,100)、子中心 (10,10)）
        child->setTranslateX(10.0f);
        parent->setScaleX(2.0f);
        parent->setScaleY(2.0f);

        YGNodeCalculateLayout(parent->getYGNode(), 400.0f, 400.0f, YGDirectionLTR);
        parent->frame(&result.ctx);

        // 期望：子中心 (10,10) 先 +10 → (20,10)；再按父的 2 倍绕 (100,100)：
        // 100 + (20-100)*2 = -60
        float minX = 1e9f, maxX = -1e9f;
        for (int i = 0; i < result.list.VtxBuffer.Size; i++)
        {
            minX = std::min(minX, result.list.VtxBuffer[i].pos.x);
            maxX = std::max(maxX, result.list.VtxBuffer[i].pos.x);
        }
        checkFloat((minX + maxX) * 0.5f, -60.0f, "父变换作用在子变换之后（M_parent * M_child）");

        delete parent;
    }

    std::printf("\n== 4. NaN 变换会被拦下（否则控件静默消失）==\n");
    {
        DrawResult result;

        Box* box = new Box(Axis::COLUMN);
        box->setWidth(40.0f);
        box->setHeight(40.0f);
        box->setBackgroundColor(IM_COL32(255, 255, 255, 255));
        box->setPositionType(PositionType::ABSOLUTE);
        box->setPositionLeft(0.0f);
        box->setPositionTop(0.0f);
        YGNodeCalculateLayout(box->getYGNode(), 400.0f, 300.0f, YGDirectionLTR);

        const float nan = std::nanf("");
        box->setTranslateX(nan);
        box->frame(&result.ctx);

        check(result.list.VtxBuffer.Size > 0, "NaN 变换下仍然画出了顶点");
        bool finite = true;
        for (int i = 0; i < result.list.VtxBuffer.Size; i++)
        {
            const ImVec2& p = result.list.VtxBuffer[i].pos;
            if (!std::isfinite(p.x) || !std::isfinite(p.y))
                finite = false;
        }
        check(finite, "顶点坐标没有被 NaN 污染（变换被忽略，而不是整块消失）");

        delete box;
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
