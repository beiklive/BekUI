#include "app/pages/pages.hpp"

#include <string>

#include "app/page_helpers.hpp"
#include "app/widgets.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/input.hpp"
#include "bklv/views/image.hpp"
#include "bklv/views/progress_spinner.hpp"
#include "bklv/views/recycler.hpp"
#include "bklv/views/scrolling_frame.hpp"

using namespace bklv;

namespace demo
{

namespace
{

// 可点的小按钮。
// View::onPointerAction 是 protected（"点一下就执行"是子类的选择），
// 所以这里包一层子类，跟 page_host.cpp 里的 TextButton 一个路子。
class LanguageButton : public Box
{
  public:
    explicit LanguageButton(const std::string& text)
        : Box(Axis::ROW)
    {
        this->setWidth(AUTO);
        this->setHeight(32.0f);
        this->setPadding(0.0f, 14.0f, 0.0f, 14.0f);
        this->setAlignItems(AlignItems::CENTER);
        this->setJustifyContent(JustifyContent::CENTER);
        this->setBackgroundColor(rgb(52, 58, 70));
        this->setCornerRadius(7.0f);

        this->label = makeText(text, 14.0f, rgb(215, 222, 232), true);
        this->addView(this->label);

        this->onPointerAction = true;
    }

    Label* getLabel() const { return this->label; }

  private:
    Label* label = nullptr;
};

} // namespace

Box* buildTextPage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeight(AUTO);

    page->addView(makeTitle("文本排版"));
    page->addView(makeHint("Label 自己实现了换行与截断：按字符测宽、二分找断点，"
                           "排版结果缓存在控件里，只在文本或宽度变化时重算。"));

    // ---- i18n：文案全部来自 resources/i18n/<locale>.json ----
    //
    // 文案是在控件构造时解析成字符串的，所以切换语言**不会**自动重译已有视图，
    // 必须显式重新 setText()（这里就是这么做的）。新创建的视图自动用新语言。
    Box* i18nCard = makeCard();
    i18nCard->addView(makeText("i18n", 16.0f, rgb(200, 208, 220)));

    Label* welcome = makeText(getStr("demo/welcome"), 17.0f, rgb(150, 205, 255));
    welcome->setMarginTop(8.0f);
    i18nCard->addView(welcome);

    Label* desc = makeText(getStr("demo/desc"), 15.0f, rgb(215, 222, 232));
    desc->setLineHeight(1.5f);
    desc->setMarginTop(6.0f);
    i18nCard->addView(desc);

    Label* langLabel = makeText(getStr("demo/lang_current", getLocale()), 14.0f, rgb(170, 180, 195));
    langLabel->setMarginTop(10.0f);
    i18nCard->addView(langLabel);

    // 可点按钮：鼠标点一下或按 A 都触发（onPointerAction + registerAction）
    LanguageButton* langButton = new LanguageButton(getStr("demo/lang_button"));
    langButton->setMarginTop(8.0f);
    Label* langButtonLabel = langButton->getLabel();
    langButton->registerAction(ControllerButton::A, getStr("demo/lang_button"),
        [welcome, desc, langLabel, langButtonLabel](View*) {
            const std::string next = getLocale() == "zh-Hans" ? "en-US" : "zh-Hans";
            setLocale(next);
            welcome->setText(getStr("demo/welcome"));
            desc->setText(getStr("demo/desc"));
            langLabel->setText(getStr("demo/lang_current", getLocale()));
            langButtonLabel->setText(getStr("demo/lang_button"));
        });
    i18nCard->addView(langButton);
    page->addView(i18nCard);

    Box* wrapCard = makeCard();
    wrapCard->addView(makeText("自动换行", 16.0f, rgb(200, 208, 220)));

    Label* paragraph = makeText(
        "这段文字用来自动换行。Yoga 不知道文字有多宽，所以 Label 向它注册了测量函数："
        "布局时按可用宽度逐字测量，算出需要几行、总高多少，再把这行文本画到 ImDrawList 上。"
        "窗口变窄时会重新断行，变宽时会合并回更少的行数。",
        15.0f, rgb(215, 222, 232));
    paragraph->setLineHeight(1.6f);
    paragraph->setMarginTop(8.0f);
    wrapCard->addView(paragraph);
    page->addView(wrapCard);

    Box* truncCard = makeCard();
    truncCard->addView(makeText("单行截断（超宽自动加省略号）", 16.0f, rgb(200, 208, 220)));

    const char* longTexts[] = {
        "这是一条很长的单行文本，宽度不够时应该在末尾出现省略号而不是硬撑出去",
        "短文本",
        "中等的单行文本，刚好会被截断一部分内容显示省略号",
    };
    for (const char* text : longTexts)
    {
        Label* line = makeText(text, 15.0f, rgb(215, 222, 232), true);
        line->setMarginTop(8.0f);
        truncCard->addView(line);
    }
    page->addView(truncCard);

    Box* alignCard = makeCard();
    alignCard->addView(makeText("对齐", 16.0f, rgb(200, 208, 220)));

    const HorizontalAlign aligns[] = { HorizontalAlign::LEFT, HorizontalAlign::CENTER, HorizontalAlign::RIGHT };
    const char* alignNames[]        = { "左对齐", "居中", "右对齐" };
    for (int i = 0; i < 3; ++i)
    {
        Box* line = new Box(Axis::ROW);
        line->setWidthPercentage(100.0f);
        line->setHeight(34.0f);
        line->setMarginTop(6.0f);
        line->setBackgroundColor(rgb(46, 51, 60));
        line->setCornerRadius(6.0f);

        Label* label = makeText(alignNames[i], 15.0f, rgb(215, 222, 232), true);
        label->setWidthPercentage(100.0f);
        label->setHorizontalAlign(aligns[i]);
        line->addView(label);
        alignCard->addView(line);
    }
    page->addView(alignCard);

    return page;
}

} // namespace demo
