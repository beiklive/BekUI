// 无窗口测试：states 的视图级通道（`states.<状态>.self`）。

#include <cstdio>
#include <memory>
#include <string>

#include "bklv/core/appearance.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/appearance_view.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

const char* kJson = R"({
  "box": { "width": 200, "height": 56 },
  "layers": [
    { "op": "rect", "id": "bg", "fill": "#202020" }
  ],
  "states": {
    "focused": {
      "self": {
        "translateX": { "to": 100, "duration": 160, "easing": "cubicOut" },
        "alpha":      { "to": 0.5, "duration": 160 }
      },
      "bg": {
        "fill": { "to": "#FF0000", "duration": 160 }
      }
    }
  }
})";

} // namespace

int main()
{
    std::printf("== states.self：视图级通道 ==\n");

    setTheme("default");

    std::string error;
    std::shared_ptr<const Appearance> spec = Appearance::compile(kJson, getTheme(), error, "test");
    check(spec != nullptr, "外观能编译（self + 分组层写法）");
    if (spec == nullptr)
    {
        std::printf("  错误：%s\n", error.c_str());
        return 1;
    }

    // 解析产物：self 的通道进了 stateViewClips，分组写法落成层通道
    const auto& clips = spec->getStateViewClips("focused");
    checkEqualInt((int)clips.size(), 2, "self 里解析出两个通道");
    checkEqualInt((int)spec->getTransitions("focused").size(), 1, "分组写法解析出一个层通道");

    AppearanceView view("base/box", false);
    check(view.applyAppearanceJson(kJson, error), "视图能套用这份外观");

    checkFloat(view.getTransform().translateX, 0.0f, "初始位移 0");

    view.setAppearanceState("focused", true);
    view.stepStateViewChannels(0.2f); // 超过 160ms
    checkFloat(view.getTransform().translateX, 100.0f, "聚焦后 translateX 到 100");
    checkFloat(view.getAlpha(), 0.5f, "聚焦后 alpha 到 0.5");

    view.setAppearanceState("focused", false);
    view.stepStateViewChannels(0.2f);
    checkFloat(view.getTransform().translateX, 0.0f, "失焦后回到 0");
    checkFloat(view.getAlpha(), 1.0f, "失焦后 alpha 回到 1");

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
