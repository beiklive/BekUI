// 无窗口测试：阴影层与模糊档位开关。

#include <cstdio>
#include <cstdlib>
#include <string>

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

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
