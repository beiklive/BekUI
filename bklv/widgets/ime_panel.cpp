#include "bklv/widgets/ime_panel.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>

#include "bklv/core/app.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/views/label.hpp"
#include "bklv/widgets/button.hpp"

#include <nlohmann/json.hpp>

namespace bklv
{

namespace
{

const std::vector<std::pair<std::string, std::string>>& pinyinDictionary()
{
    static std::vector<std::pair<std::string, std::string>> table;
    if (!table.empty())
        return table;

    std::ifstream file(resourcePath("resources/i18n/pinyin.json"));
    if (!file.is_open())
        return table;

    nlohmann::json doc;
    try
    {
        file >> doc;
    }
    catch (...)
    {
        return table;
    }

    if (!doc.is_object())
        return table;

    for (auto it = doc.begin(); it != doc.end(); ++it)
    {
        if (!it.value().is_string())
            continue;
        std::string pinyins = it.value().get<std::string>();
        size_t start = 0;
        while (start < pinyins.size())
        {
            const size_t end = pinyins.find(' ', start);
            const std::string word = pinyins.substr(start,
                end == std::string::npos ? std::string::npos : end - start);
            if (!word.empty())
                table.emplace_back(word, it.key());
            if (end == std::string::npos)
                break;
            start = end + 1;
        }
    }
    return table;
}

} // namespace

ImePanel::ImePanel(std::string initialText, int maxLengthValue, bool numeric,
    std::function<void(bool, const std::string&)> callback)
    : Box(Axis::COLUMN)
    , text(std::move(initialText))
    , maxLength(maxLengthValue)
    , numeric(numeric)
    , callback(std::move(callback))
{
    this->setWidthPercentage(100.0f);
    this->setHeightPercentage(100.0f);
    this->setJustifyContent(JustifyContent::FLEX_END);
    this->setBackgroundColor(getTheme().color("color.common.overlay", IM_COL32(0, 0, 0, 120)));

    // 输入框 + 提示
    Box* panel = new Box(Axis::COLUMN);
    panel->setWidthPercentage(100.0f);
    panel->setPadding(16.0f);
    panel->setGap(8.0f);
    panel->setBackgroundColor(getTheme().color("color.surface.card"));
    panel->setCornerRadius(getTheme().number("metric.radius.lg", 8.0f));

    this->textLabel = new Label();
    this->textLabel->setFontSize(20.0f);
    this->textLabel->setTextColor(getTheme().color("color.text.primary"));
    panel->addView(this->textLabel);

    this->hintLabel = new Label();
    this->hintLabel->setFontSize(13.0f);
    this->hintLabel->setTextColor(getTheme().color("color.text.dim"));
    panel->addView(this->hintLabel);

    this->candidateRow = new Box(Axis::ROW);
    this->candidateRow->setGap(8.0f);
    panel->addView(this->candidateRow);

    this->buildKeyboard();
    panel->addView(this->letterRows);

    this->addView(panel);
    this->refreshLabels();
    this->refreshCandidates();
}

void ImePanel::buildKeyboard()
{
    this->letterRows = new Box(Axis::COLUMN);
    this->letterRows->setGap(6.0f);
    this->letterRows->setAlignItems(AlignItems::CENTER);

    const char* rows[3] = { "1234567890", "qwertyuiop", "asdfghjkl" };
    for (int r = 0; r < 3; r++)
    {
        Box* row = new Box(Axis::ROW);
        row->setGap(6.0f);
        row->setJustifyContent(JustifyContent::CENTER);
        for (const char* c = rows[r]; *c != 0; c++)
        {
            const std::string value(1, *c);
            Button* key = new Button(value);
            key->setWidth(56.0f);
            key->setHeight(46.0f);
            this->letterKeys.emplace_back(key, value);
            key->setOnClick([this, value] {
                if (this->chinese && value[0] >= 'a' && value[0] <= 'z')
                {
                    this->composing += value;
                    this->refreshLabels();
                    this->refreshCandidates();
                }
                else
                {
                    this->appendChar(value);
                }
            });
            row->addView(key);
        }
        this->letterRows->addView(row);
    }

    // 第三行补 z x c v b n m
    Box* row3 = new Box(Axis::ROW);
    row3->setGap(6.0f);
    row3->setJustifyContent(JustifyContent::CENTER);
    for (const char* c = "zxcvbnm"; *c != 0; c++)
    {
        const std::string value(1, *c);
        Button* key = new Button(value);
        key->setWidth(56.0f);
        key->setHeight(46.0f);
        this->letterKeys.emplace_back(key, value);
        key->setOnClick([this, value] {
            if (this->chinese)
            {
                this->composing += value;
                this->refreshLabels();
                this->refreshCandidates();
            }
            else
                this->appendChar(value);
        });
        row3->addView(key);
    }
    this->letterRows->addView(row3);

    Box* controls = new Box(Axis::ROW);
    controls->setGap(8.0f);
    controls->setJustifyContent(JustifyContent::CENTER);

    Button* language = new Button("中/英");
    language->setOnClick([this] { this->toggleLanguage(); });
    controls->addView(language);

    Button* shiftKey = new Button("大写");
    shiftKey->setOnClick([this] { this->toggleShift(); });
    controls->addView(shiftKey);

    Button* space = new Button("空格");
    space->setGrow(1.0f);
    space->setOnClick([this] { this->appendChar(" "); });
    controls->addView(space);

    Button* back = new Button("退格");
    back->setOnClick([this] { this->backspace(); });
    controls->addView(back);

    Button* cancelButton = new Button("取消");
    cancelButton->setOnClick([this] { this->cancel(); });
    controls->addView(cancelButton);

    Button* confirmButton = new Button("确定");
    confirmButton->setOnClick([this] { this->confirm(); });
    controls->addView(confirmButton);

    this->letterRows->addView(controls);
}

void ImePanel::refreshLabels()
{
    if (this->textLabel != nullptr)
        this->textLabel->setText(this->text.empty() ? " " : this->text);
    if (this->hintLabel != nullptr)
        this->hintLabel->setText(this->composing.empty()
            ? (this->chinese ? "拼音输入" : "英文输入")
            : "拼音：" + this->composing);
}

std::vector<std::string> ImePanel::getCandidates() const
{
    std::vector<std::string> out;
    if (!this->chinese || this->composing.empty())
        return out;

    for (const auto& item : pinyinDictionary())
    {
        if (item.first.rfind(this->composing, 0) == 0 && item.second != this->text)
        {
            if (std::find(out.begin(), out.end(), item.second) == out.end())
                out.push_back(item.second);
            if (out.size() >= 6)
                break;
        }
    }
    return out;
}

void ImePanel::refreshCandidates()
{
    if (this->candidateRow == nullptr)
        return;

    for (View* child : std::vector<View*>(this->candidateRow->getChildren()))
        this->candidateRow->removeView(child, true);

    const std::vector<std::string> candidates = this->getCandidates();
    for (size_t i = 0; i < candidates.size(); i++)
    {
        Button* candidate = new Button(candidates[i]);
        candidate->setOnClick([this, i] { this->chooseCandidate(i); });
        this->candidateRow->addView(candidate);
    }
}

void ImePanel::chooseCandidate(size_t index)
{
    const std::vector<std::string> candidates = this->getCandidates();
    if (index >= candidates.size())
        return;

    this->text += candidates[index];
    this->composing.clear();
    this->refreshLabels();
    this->refreshCandidates();
}

void ImePanel::appendChar(const std::string& value)
{
    if (this->numeric)
    {
        for (char c : value)
            if (c < '0' || c > '9')
                return;
    }

    std::string insert = value;
    if (this->shift && !this->chinese)
        for (char& c : insert)
            c = (char)std::toupper((unsigned char)c);
    if (this->text.size() + insert.size() > (size_t)this->maxLength)
        return;
    this->text += insert;
    this->refreshLabels();
}

void ImePanel::backspace()
{
    if (!this->composing.empty())
        this->composing.pop_back();
    else if (!this->text.empty())
    {
        // 按 UTF-8 字符删
        size_t index = this->text.size() - 1;
        while (index > 0 && ((unsigned char)this->text[index] & 0xC0) == 0x80)
            index--;
        this->text.erase(index);
    }
    this->refreshLabels();
    this->refreshCandidates();
}

void ImePanel::commitPinyin()
{
    if (!this->composing.empty())
    {
        this->text += this->composing;
        this->composing.clear();
    }
}

void ImePanel::toggleLanguage()
{
    if (this->numeric)
        return;
    this->chinese = !this->chinese;
    this->commitPinyin();
    this->refreshLabels();
    this->refreshCandidates();
}

void ImePanel::toggleShift()
{
    this->shift = !this->shift;
    this->relabelKeys();
}

void ImePanel::relabelKeys()
{
    for (auto& entry : this->letterKeys)
    {
        std::string label = entry.second;
        if (this->shift && !this->chinese)
            for (char& c : label)
                c = (char)std::toupper((unsigned char)c);
        entry.first->setText(label);
    }
}

void ImePanel::insertAscii(const std::string& value)
{
    this->appendChar(value);
}

void ImePanel::confirm()
{
    this->commitPinyin();
    this->finish(true);
}

void ImePanel::cancel()
{
    this->finish(false);
}

void ImePanel::finish(bool accepted)
{
    // popOverlay(..., true) 会 delete this —— 回调与结果必须先拷出来
    const std::string result = this->text;
    std::function<void(bool, const std::string&)> done = this->callback;
    if (App* app = App::getCurrent())
        app->popOverlay(this, true);
    if (done)
        done(accepted, result);
}

// ---------------------------------------------------------------------------
// ImeManager
// ---------------------------------------------------------------------------

namespace
{
class PanelImeManager : public ImeManager
{
  public:
    bool open(const ImeRequest& request) override
    {
        App* app = App::getCurrent();
        if (app == nullptr)
            return false;
        auto* panel = new ImePanel(request.initialText, request.maxLength, request.numeric,
            request.callback);
        if (!app->pushOverlay(panel))
        {
            delete panel;
            return false;
        }
        return true;
    }
};
} // namespace

ImeManager& getPanelImeManager()
{
    static PanelImeManager manager;
    return manager;
}

} // namespace bklv
