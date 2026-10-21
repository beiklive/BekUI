// JSON 主题能力：演示控件全部来自 resources/theme/default/demo/ 下的外观 JSON，
// 右侧滚动显示这些 JSON 的原文（改 JSON 就能改样式，不用改 C++）。

#include "app/pages.hpp"
#include "app/pages/showcase.hpp"

#include "bklv/core/icon_font.hpp"

using namespace bklv;

namespace
{

AppearanceView* makeDemoView(const char* appearance, const std::string& title,
    const std::string& info, const char* iconName)
{
    AppearanceView* view = new AppearanceView(appearance, false);
    view->setText(content::TEXT, title);
    if (!info.empty())
        view->setText(content::INFO, info);
    if (iconName != nullptr)
        view->setIcon(content::ICON, bklv::icon::glyphByName(iconName));
    return view;
}

} // namespace

namespace demo
{

Box* buildJsonThemePage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setGap(getTheme().number("metric.spacing.lg", 24.0f));

    page->addView(makeShowcaseSection("JSON · 渐变 + 阴影 + 多文本层",
        makeDemoView("demo/gradient_card", "主题卡片", "gradient + shadow", "star"),
        readThemeJson("demo/gradient_card.json")));

    page->addView(makeShowcaseSection("JSON · 胶囊 + 描边 + 阴影",
        makeDemoView("demo/pill", "开始游戏", "", nullptr),
        readThemeJson("demo/pill.json")));

    page->addView(makeShowcaseSection("JSON · 内阴影 + 描边 + 图标 / 信息",
        makeDemoView("demo/inner_card", "内阴影卡片", "inner: true", "settings"),
        readThemeJson("demo/inner_card.json")));

    return page;
}

} // namespace demo
