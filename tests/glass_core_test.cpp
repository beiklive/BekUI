// 无窗口测试：玻璃的"思路层"（请求队列 / 模糊计划 / 形状解析 / 调参钳制）。
//
// 这一层是纯数据 + 纯函数，跨后端共用 —— 两条后端（GL / VK）和设计器都吃同一份逻辑，
// 所以它一旦算错，表现是"两条后端都怪"，而且很难从画面上反推。这里把不变量钉住。

#include <cmath>
#include <cstdio>
#include <string>

#include "bklv/core/glass/glass_backend.hpp"
#include "bklv/core/glass/glass_blur_plan.hpp"
#include "bklv/core/glass/glass_geometry.hpp"
#include "bklv/core/glass/glass_queue.hpp"
#include "bklv/core/glass/glass_request.hpp"
#include "bklv/core/glass/glass_service.hpp"
#include "bklv/core/glass/glass_style.hpp"
#include "bklv/core/glass/glass_tuning.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

glass::GlassRequest makeRequest(float minX, float minY, float maxX, float maxY)
{
    glass::GlassRequest request;
    request.minMax = ImVec4(minX, minY, maxX, maxY);
    return request;
}

// 只记调用顺序的假玻璃后端。
// 玻璃服务只认 `GlassBackend` 抽象接口 —— 这正是"core 不认识 backends"的验收方式：
// 用假后端就能把"注入 / 补齐 / 能力门禁 / 每帧顺序"全部跑一遍，不需要窗口与图形 API。
class RecordingBackend : public glass::GlassBackend
{
  public:
    bool ensure(int framebufferWidth, int framebufferHeight) override
    {
        this->width  = framebufferWidth;
        this->height = framebufferHeight;
        this->calls += "E";
        return this->ready;
    }

    bool isReady() const override { return this->ready; }
    glass::EffectCapabilities getCapabilities() const override { return this->caps; }

    void beginFrame(const glass::GlassTuning& tuning) override
    {
        this->calls += "B";
        this->blurRadius = tuning.blurRadius;
    }

    void submit(glass::GlassRequestQueue& queue, const glass::GlassRequest& request,
        const glass::GlassTuning&, int frameWidth, int frameHeight) override
    {
        this->calls += "S";
        this->lastDensity = request.density;
        this->frameWidth  = frameWidth;
        this->frameHeight = frameHeight;
        queue.submit(request);
    }

    void prepareDrawData(ImDrawData&, const glass::GlassRequestQueue&) override { this->calls += "P"; }
    void finishFrame() override { this->calls += "F"; }

    bool ready = true;
    glass::EffectCapabilities caps{};
    std::string calls;
    float lastDensity = -1.0f;
    int width        = 0;
    int height       = 0;
    int frameWidth   = 0;
    int frameHeight  = 0;
    float blurRadius = 0.0f;
};

float weightSum(const glass::GlassBlurPlan& plan)
{
    float sum = plan.centerWeight;
    for (uint32_t i = 0; i < plan.pairCount; i++)
        sum += plan.pairWeights[i] * 2.0f;
    return sum;
}

} // namespace

int main()
{
    std::printf("== 1. 请求队列：先攒后收口，收口后不再收 ==\n");
    {
        glass::GlassRequestQueue queue;
        check(queue.isEmpty(), "新队列是空的");

        glass::GlassBackdropStyle frameStyle;
        frameStyle.blurRadius = 24.0f;
        frameStyle.quality    = glass::GlassQuality::BALANCED;
        queue.beginFrame(frameStyle, 2.0f);
        queue.submit(makeRequest(0, 0, 10, 10));
        queue.submit(makeRequest(20, 20, 40, 40));
        check(queue.getRequests().size() == 2, "收口前收到两块玻璃");
        checkFloat(queue.getDensity(), 2.0f, "密度按帧记下来（Retina 上是 2）");
        checkFloat(queue.getBackdropStyle().blurRadius, 24.0f, "整帧的模糊参数也记下来了");
        checkFloat(queue.getRequests()[0].minMax.x, 0.0f, "顺序保持：第一块还是第一块");
        checkFloat(queue.getRequests()[1].minMax.x, 20.0f, "顺序保持：第二块没被换序");

        queue.seal();
        queue.submit(makeRequest(50, 50, 60, 60));
        check(queue.getRequests().size() == 2, "收口之后再交也不收（避免同一帧反复改）");

        glass::GlassBackdropStyle otherStyle;
        queue.beginFrame(otherStyle, 0.0f);
        check(queue.isEmpty(), "新的一帧清空队列");
        checkFloat(queue.getDensity(), 1.0f, "密度传 0 时回落到 1（不让 0 传到后端）");
        queue.submit(makeRequest(1, 1, 2, 2));
        check(queue.getRequests().size() == 1, "新的一帧又开始收");
    }

    std::printf("\n== 2. 模糊计划：归一化、配对、质量档与半径都单调 ==\n");
    {
        check(makeGlassBlurPlan(0.0f, glass::GlassQuality::BALANCED).pairCount == 0,
            "半径 0 -> 不模糊（没有配对）");
        check(makeGlassBlurPlan(-5.0f, glass::GlassQuality::BALANCED).pairCount == 0,
            "负半径也当成不模糊");
        check(makeGlassBlurPlan(INFINITY, glass::GlassQuality::BALANCED).pairCount == 0,
            "非有限半径不模糊（不让 NaN 进后端）");

        const glass::GlassBlurPlan minimal  = makeGlassBlurPlan(32.0f, glass::GlassQuality::MINIMAL);
        const glass::GlassBlurPlan balanced = makeGlassBlurPlan(32.0f, glass::GlassQuality::BALANCED);
        const glass::GlassBlurPlan full     = makeGlassBlurPlan(32.0f, glass::GlassQuality::FULL);

        check(minimal.pairCount > 0, "最小档也有配对");
        check(minimal.pairCount <= balanced.pairCount, "质量越高配对不少于最小档");
        check(balanced.pairCount <= full.pairCount, "完整档配对最多");
        check(full.pairCount <= glass::kGlassGaussianPairCapacity, "配对不超过容量");

        // 权重必须归一化：中心 + 每个配对的两侧，和应为 1（差一点点是浮点误差）
        checkFloat(weightSum(balanced), 1.0f, "权重归一化（和 = 1）");
        check(balanced.centerWeight > 0.0f && balanced.centerWeight < 1.0f, "中心权重在 (0,1)");

        // 配对权重递减、偏移递增：这是"配对高斯"能近似高斯的必要条件
        bool weightsDescending = true;
        bool offsetsAscending  = true;
        for (uint32_t i = 0; i < balanced.pairCount; i++)
        {
            if (balanced.pairWeights[i] <= 0.0f)
                weightsDescending = false;
            if (i > 0)
            {
                if (balanced.pairWeights[i] > balanced.pairWeights[i - 1])
                    weightsDescending = false;
                if (balanced.pairOffsets[i] <= balanced.pairOffsets[i - 1])
                    offsetsAscending = false;
            }
        }
        check(weightsDescending, "配对权重从中心往外递减");
        check(offsetsAscending, "配对偏移从中心往外递增");

        const glass::GlassBlurPlan wide = makeGlassBlurPlan(96.0f, glass::GlassQuality::FULL);
        check(wide.pairCount >= full.pairCount, "半径越大覆盖越宽（配对不会更少）");
    }

    std::printf("\n== 3. 形状解析：矩形 / 胶囊 / 正圆共用一条钳制规则 ==\n");
    {
        const ImVec4 square  = ImVec4(0.0f, 0.0f, 48.0f, 48.0f);
        const ImVec4 wide    = ImVec4(0.0f, 0.0f, 200.0f, 40.0f);

        checkFloat(glass::resolveGlassRadius(square, glass::CornerRadii::all(6.0f),
                       glass::GlassShapeKind::ROUNDED_RECT),
            6.0f, "圆角矩形：用写下的半径");
        checkFloat(glass::resolveGlassRadius(wide, glass::CornerRadii::all(999.0f),
                       glass::GlassShapeKind::ROUNDED_RECT),
            20.0f, "半径超过短边一半时钳到短边一半（不会出现怪形状）");
        checkFloat(glass::resolveGlassRadius(square, glass::CornerRadii::all(0.0f),
                       glass::GlassShapeKind::CIRCLE),
            24.0f, "正圆：短边的一半");
        checkFloat(glass::resolveGlassRadius(wide, glass::CornerRadii::all(0.0f),
                       glass::GlassShapeKind::CAPSULE),
            20.0f, "胶囊：短边的一半（两端是半圆）");

        // 四角半径不一致时取最大（后端只支持一个统一半径）
        glass::CornerRadii uneven;
        uneven.topLeft     = 3.0f;
        uneven.topRight    = 9.0f;
        uneven.bottomRight = 4.0f;
        uneven.bottomLeft  = 1.0f;
        checkFloat(glass::resolveGlassRadius(square, uneven, glass::GlassShapeKind::ROUNDED_RECT),
            9.0f, "四角不一致时取最大");

        checkFloat(glass::resolveGlassRadius(ImVec4(0, 0, 0, 10), glass::CornerRadii::all(4.0f),
                       glass::GlassShapeKind::ROUNDED_RECT),
            0.0f, "尺寸非正返回 0（调用方据此跳过这块玻璃）");
    }

    std::printf("\n== 4. 调参：越界值必须被钳回来 ==\n");
    {
        glass::GlassTuning tuning;
        tuning.blurRadius         = 999.0f;
        tuning.quality            = 9;
        tuning.tintA              = 5.0f;
        tuning.tintR              = -1.0f;
        tuning.refractionAmount   = 120.0f;
        tuning.chromaticAberration = 3.0f;
        tuning.barRadius          = -8.0f;
        tuning.clampToValidRange();

        checkFloat(tuning.blurRadius, 48.0f, "模糊半径钳到上限 48");
        checkEqualInt(tuning.quality, 2, "质量档钳到 2");
        checkFloat(tuning.tintA, 1.0f, "染色透明度钳到 1");
        checkFloat(tuning.tintR, 0.0f, "染色分量钳到 0");
        checkFloat(tuning.refractionAmount, 0.0f, "折射量上限是 0（负值才是凹陷）");
        checkFloat(tuning.chromaticAberration, 1.0f, "色散钳到 1");
        checkFloat(tuning.barRadius, 0.0f, "圆角钳到 0 以上");
    }

    std::printf("\n== 5. 请求默认值：形状/来源/强度都要有确定初值 ==\n");
    {
        glass::GlassRequest request;
        check(request.shape == glass::GlassShapeKind::ROUNDED_RECT, "默认形状是圆角矩形");
        check(request.backdropSource == glass::GlassBackdropSource::FRAME,
            "默认采样「这一帧已画好的内容」");
        checkFloat(request.density, 1.0f, "默认密度 1");
        checkFloat(request.opacity, 1.0f, "默认不透明");
        checkFloat(request.blurMix, 1.0f, "默认全模糊");
        check(request.drawList == nullptr, "默认没有注入点（由宿主填）");
    }

    std::printf("\n== 6. 玻璃服务：后端由宿主注入，服务只认抽象接口 ==\n");
    {
        glass::GlassService& service = glass::getGlassService();

        // 没注入后端（没有玻璃能力的图形后端就是这个样子）：整层关闭，submit 不炸也不排队
        service.setBackend(nullptr);
        service.setEnabled(true);
        service.beginFrame(1280, 720, 1.0f);
        check(!service.isSupported(), "没注入后端 -> 不支持");
        service.submit(makeRequest(0, 0, 10, 10));
        checkEqualInt((int)service.getRequestCount(), 0, "没后端时不排队");

        RecordingBackend backend;
        backend.caps.backdropCapture = true;
        backend.caps.blur            = true;

        service.setBackend(&backend);
        service.beginFrame(1280, 720, 2.0f);
        check(service.isSupported(), "注入了可用后端 -> 支持");
        checkEqualInt(backend.width, 1280, "ensure 拿到帧缓冲宽（物理像素）");
        checkEqualInt(backend.height, 720, "ensure 拿到帧缓冲高（物理像素）");
        check(backend.calls == "BE", "帧首顺序：先 beginFrame 再 ensure（和旧 VK 路径一致）");

        glass::GlassRequest request = makeRequest(10.0f, 20.0f, 110.0f, 60.0f);
        request.density = 0.0f; // 没填：由服务补帧密度
        service.submit(request);
        check(backend.calls == "BES", "提交被转给了后端");
        checkFloat(backend.lastDensity, 2.0f, "density 为 0 时服务补上帧密度 2");
        checkEqualInt((int)service.getRequestCount(), 1, "请求进了服务持有的队列");
        checkEqualInt(backend.frameWidth, 1280, "后端拿到帧宽（兜住空的裁剪矩形要用）");
        checkEqualInt(backend.frameHeight, 720, "后端拿到帧高");

        ImDrawData drawData{};
        service.prepareDrawData(drawData);
        check(backend.calls == "BESP", "注入点在 draw data 那一趟");
        service.prepareDrawData(drawData);
        check(backend.calls == "BESP", "同一帧只注入一次（避免重复插命令）");

        service.finishFrame();
        check(backend.calls == "BESPF", "帧末收口并转给后端");
        service.submit(makeRequest(0, 0, 5, 5));
        checkEqualInt((int)service.getRequestCount(), 1, "收口之后再交也不收");

        // 能力不足的后端（例如只有对象、没有实现模糊）：下一帧就不算支持
        backend.calls = "";
        backend.caps.blur = false;
        service.beginFrame(1280, 720, 2.0f);
        check(!service.isSupported(), "后端能力不足（blur=false）-> 不支持");

        // 关掉开关：不该再去问后端要资源（ensure 不会发生）
        backend.calls = "";
        backend.caps.blur = true;
        service.setEnabled(false);
        service.beginFrame(1280, 720, 2.0f);
        check(!service.isSupported(), "开关关着 -> 不支持");
        check(backend.calls.find('E') == std::string::npos, "开关关着时不 ensure（不白建资源）");

        // 单例不能留着指向栈上的假后端
        service.setBackend(nullptr);
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
