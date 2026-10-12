#include "app/pages.hpp"

#include "app/ui.hpp"
#include "bklv/bklv.hpp"

using namespace bklv;

namespace demo
{

namespace
{

bklv::AppletFrame* g_settingsShell = nullptr;

std::string backendName()
{
    return getGraphicsBackendName();
}

} // namespace

void setSettingsShell(bklv::AppletFrame* shell)
{
    g_settingsShell = shell;
}

// 设置页：对标 borealis demo 的 Settings tab——整页由列表型 Cell 组成。
// 这一页同时也是 Cell 家族的演示：detail / bool / radio / selector / slider / input。
Box* buildSettingsPage()
{
    Box* page = makeColumn(getTheme().number("metric.spacing.md", 12.0f));
    page->addView(makeHint(getStr("settings/hint")));

    AppletFrame* shell = g_settingsShell;

    // ---- 说明信息（DetailCell）----
    Rectangle* about = makeCard();
    about->addView(makeText(getStr("settings/about_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    DetailCell* backend = new DetailCell(getStr("settings/backend"), backendName());
    about->addView(backend);

    DetailCell* theme = new DetailCell(getStr("settings/theme"), getTheme().getName());
    about->addView(theme);

    DetailCell* status = new DetailCell(getStr("settings/status"), getStr("settings/status_note"));
    about->addView(status);
    page->addView(about);

    // ---- 开关（BooleanCell）----
    Rectangle* switches = makeCard();
    switches->addView(makeText(getStr("settings/switches_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    BooleanCell* headerCell = new BooleanCell(getStr("settings/show_header"), true);
    headerCell->setOnChange([shell](bool value) {
        if (shell != nullptr)
            shell->showHeader(value);
    });
    switches->addView(headerCell);

    BooleanCell* barCell = new BooleanCell(getStr("settings/show_bottom_bar"), true);
    barCell->setOnChange([shell](bool value) {
        if (shell != nullptr)
            shell->showBottomBar(value);
    });
    switches->addView(barCell);

    BooleanCell* animCell = new BooleanCell(getStr("settings/animate_bars"), true);
    animCell->setOnChange([shell](bool value) {
        if (shell != nullptr)
            shell->setBarsAnimated(value);
    });
    switches->addView(animCell);

    BooleanCell* logCell = new BooleanCell(getStr("settings/show_log"), false);
    switches->addView(logCell);

    // 调试层（对应 borealis Settings 页的 "Debug Layer" 与 XML 的 wireframe）
    BooleanCell* wireCell = new BooleanCell(getStr("settings/wireframe"), false);
    wireCell->setOnChange([](bool value) {
        if (App* app = App::getCurrent())
            app->setDebugLayerEnabled(value);
    });
    switches->addView(wireCell);
    page->addView(switches);

    // ---- 音效（代码合成，桌面/ Switch 都有反馈；不依赖素材）----
    Rectangle* soundCard = makeCard();
    soundCard->addView(makeText(getStr("settings/sound_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    BooleanCell* soundOn = new BooleanCell(getStr("settings/sound_on"), audio::isEnabled());
    soundOn->setOnChange([](bool value) { audio::setEnabled(value); });
    soundCard->addView(soundOn);

    SliderCell* volume = new SliderCell(getStr("settings/volume"));
    volume->setRange(0.0f, 1.0f);
    volume->setValue(audio::getVolume());
    volume->setOnChange([](float value) {
        audio::setVolume(value);
        audio::play(Sound::FocusChange); // 边调边听
    });
    soundCard->addView(volume);

    DetailCell* audition = new DetailCell(getStr("settings/audition"), getStr("settings/audition_hint"));
    static int soundIndex = 0;
    audition->setOnClick([audition] {
        const Sound sounds[] = { Sound::Click, Sound::FocusChange, Sound::Back, Sound::Honk };
        const int count      = (int)(sizeof(sounds) / sizeof(Sound));
        audio::play(sounds[soundIndex % count]);
        audition->setDetail(std::to_string(soundIndex % count));
        soundIndex++;
    });
    soundCard->addView(audition);
    page->addView(soundCard);

    // ---- 选择器（SelectorCell）----
    Rectangle* selectors = makeCard();
    selectors->addView(makeText(getStr("settings/selectors_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    SelectorCell* themeSelector = new SelectorCell(getStr("settings/theme_select"));
    themeSelector->setOptions({ "default", "ocean" });
    themeSelector->setOnChange([](int, const std::string& value) { setTheme(value); });
    selectors->addView(themeSelector);

    SliderCell* duration = new SliderCell(getStr("settings/bar_duration"));
    duration->setRange(0.05f, 0.60f);
    duration->setValue(0.18f);
    duration->setOnChange([shell](float value) {
        if (shell != nullptr)
            shell->setBarAnimationDuration(value);
    });
    selectors->addView(duration);
    page->addView(selectors);

    // ---- 单选（RadioCell）：互斥由这一组自己管 ----
    Rectangle* radios = makeCard();
    radios->addView(makeText(getStr("settings/radios_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    static std::vector<RadioCell*> group;
    group.clear();
    for (const char* key : { "settings/radio_a", "settings/radio_b", "settings/radio_c" })
    {
        RadioCell* radio = new RadioCell(getStr(key), group.empty());
        group.push_back(radio);
        radios->addView(radio);
    }
    for (size_t i = 0; i < group.size(); i++)
    {
        group[i]->setOnChange([i](bool selected) {
            if (!selected)
                return;
            for (size_t j = 0; j < group.size(); j++)
                if (j != i)
                    group[j]->setSelected(false);
        });
    }
    page->addView(radios);

    // ---- 输入（InputCell）----
    Rectangle* inputs = makeCard();
    inputs->addView(makeText(getStr("settings/inputs_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    InputCell* text = new InputCell(getStr("settings/input_text"));
    text->setDetail(getStr("settings/input_text_default"));
    inputs->addView(text);

    InputCell* number = new InputCell(getStr("settings/input_number"));
    number->setNumeric(true);
    number->setDetail("1280");
    inputs->addView(number);
    page->addView(inputs);

    // ---- 通知（对标 borealis Settings 页里的 "Post a notification"）----
    Rectangle* toast = makeCard();
    toast->addView(makeText(getStr("settings/notify_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    DetailCell* post = new DetailCell(getStr("settings/notify_cell"));
    static int count = 0;
    post->setOnClick([post] {
        count++;
        post->setDetail(std::to_string(count));
        if (App* app = App::getCurrent())
            app->notify(getStr("settings/notify_text") + " #" + std::to_string(count));
    });
    toast->addView(post);
    page->addView(toast);

    return page;
}

} // namespace demo
