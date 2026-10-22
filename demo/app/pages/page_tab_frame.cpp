// 两个 TabFrame 演示页：水平（栏在顶部）与竖直（栏在左侧）。
//
// 数据都是测试数据：每个 tab 页放一行说明 + 一个可聚焦按钮，
// 用来验证"切页正确 + 焦点行为正确"。

#include "app/pages.hpp"
#include "bklv/bklv.hpp"
#include "bklv/core/icon_font.hpp"

using namespace bklv;

namespace demo
{

namespace
{

// 一个 tab 页：说明文字 + 一个按钮（按钮让 FOCUS 模式的"焦点进内容"也能演示）
Box* makeTabPage(const std::string& title, const std::string& detail)
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setGrow(1.0f);
    page->setGap(getTheme().number("metric.spacing.md", 12.0f));

    Label* heading = new Label();
    heading->setText(title);
    heading->setFontSize(getTheme().number("metric.font.size.lg", 24.0f));
    heading->setTextColor(getTheme().color("color.text.primary"));
    page->addView(heading);

    Label* body = new Label();
    body->setText(detail);
    body->setFontSize(getTheme().number("metric.font.size.sm", 16.0f));
    body->setTextColor(getTheme().color("color.text.dim"));
    page->addView(body);

    Button* action = new Button();
    action->setText("测试按钮");
    action->setWidthPercentage(45.0f); // 别拉满整行（列方向默认 stretch）
    action->setOnClick([title] { BKLV_LOG_INFO("点了：%s 里的按钮", title.c_str()); });
    page->addView(action);

    return page;
}

} // namespace

Box* buildHorizontalTabPage()
{
    // 横向：栏在顶部（默认 placement=START），点击切换
    TabFrame* frame = new TabFrame(TabOrientation::HORIZONTAL, TabTriggerMode::CLICK);
    frame->setTitle("水平 TabFrame");
    frame->setWidthPercentage(100.0f);
    frame->setGrow(1.0f);

    frame->addTab("概览", bklv::icon::glyphByName("star"), makeTabPage("概览", "横向 TabFrame 的第 1 页"));
    frame->addTab("详情", bklv::icon::glyphByName("menu"), makeTabPage("详情", "横向 TabFrame 的第 2 页"));
    frame->addTab("设置", bklv::icon::glyphByName("settings"), makeTabPage("设置", "横向 TabFrame 的第 3 页"));

    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setGrow(1.0f);
    page->addView(frame);
    return page;
}

Box* buildVerticalTabPage()
{
    // 竖直：栏在左侧（默认 placement=START），点击切换
    TabFrame* frame = new TabFrame(TabOrientation::VERTICAL, TabTriggerMode::CLICK);
    frame->setTitle("竖直 TabFrame");
    frame->setWidthPercentage(100.0f);
    frame->setGrow(1.0f);

    frame->addTab("账户", bklv::icon::glyphByName("star"), makeTabPage("账户", "竖直 TabFrame 的第 1 页"));
    frame->addSeparator();
    frame->addTab("通知", bklv::icon::glyphByName("settings"), makeTabPage("通知", "竖直 TabFrame 的第 2 页"));
    frame->addTab("关于", bklv::icon::glyphByName("menu"), makeTabPage("关于", "竖直 TabFrame 的第 3 页"));

    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setGrow(1.0f);
    page->addView(frame);
    return page;
}

} // namespace demo
