#include "app/pages/pages.hpp"

#include <string>

#include "app/page_helpers.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/applet_frame.hpp"
#include "bklv/widgets/button.hpp"
#include "bklv/widgets/rectangle.hpp"

using namespace bklv;

namespace demo
{

namespace
{

// 一层内容：标题 + 若干按钮；点按钮就往下钻一层
Box* makeLevel(const std::string& title, int depth, AppletFrame* frame)
{
    Box* level = new Box(Axis::COLUMN);
    level->setWidthPercentage(100.0f);
    level->setHeight(AUTO);

    level->addView(makeText(title, 20.0f, getTheme().color("color.text.primary")));

    Label* hint = makeText(getStr("applet/level_hint", depth), 14.0f,
        getTheme().color("color.text.dim"));
    hint->setTextColorExplicit(false);
    hint->setLineHeight(1.5f);
    level->addView(hint);

    Box* row = makeRow(12.0f);
    row->setAlignItems(AlignItems::CENTER);
    row->setMarginTop(16.0f);

    // 再下一层（最多演示 3 层，避免无限下钻）
    if (depth < 3)
    {
        Button* deeper = new Button(getStr("applet/push"));
        deeper->setOnClick([frame, depth] {
            frame->pushContentView(makeLevel(getStr("applet/level", depth + 1), depth + 1, frame));
        });
        row->addView(deeper);
    }

    Button* back = new Button(getStr("applet/pop"));
    back->setOnClick([frame] { frame->popContentView(); });
    row->addView(back);

    level->addView(row);
    return level;
}

} // namespace

Box* buildAppletPage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeight(AUTO);

    page->addView(makeTitle(getStr("tab/applet")));
    page->addView(makeHint(getStr("applet/hint")));

    // AppletFrame 要撑满一块区域才能看出页头/内容/底栏的分层
    Box* host = new Box(Axis::COLUMN);
    host->setWidthPercentage(100.0f);
    host->setHeight(420.0f);

    AppletFrame* frame = new AppletFrame();
    frame->setTitle(getStr("applet/title"));
    frame->setSubtitle(getStr("applet/subtitle"));
    frame->setOnDismiss([] {
        // 已经在最外层还按 B：这里只是通知，演示里什么都不用做
    });

    // 内容栈的第一层
    frame->pushContentView(makeLevel(getStr("applet/level", 1), 1, frame));

    // 页头样式切换（换一份外观文件，不改代码）
    Button* styleToggle = new Button(getStr("applet/toggle_header"));
    styleToggle->setOnClick([frame, styleToggle] {
        static bool popup = false;
        popup = !popup;
        frame->setHeaderStyle(popup ? AppletHeaderStyle::Popup : AppletHeaderStyle::Regular);
        styleToggle->setText(popup ? getStr("applet/header_regular") : getStr("applet/header_popup"));
    });

    Box* controls = makeRow(12.0f);
    controls->setAlignItems(AlignItems::CENTER);
    Box* styleSlot = new Box(Axis::COLUMN);
    styleSlot->setWidth(220.0f);
    styleSlot->addView(styleToggle);
    controls->addView(styleSlot);

    host->addView(frame);
    page->addView(host);
    page->addView(controls);

    return page;
}

} // namespace demo
