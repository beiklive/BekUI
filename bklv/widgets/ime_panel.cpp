#include "bklv/widgets/ime_panel.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

#include "bklv/3rd/imgui/imgui.h"
#include "bklv/core/app.hpp"
#include "bklv/core/font.hpp"
#include "bklv/core/gesture.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/views/label.hpp"
#include "bklv/widgets/button.hpp"

#include <nlohmann/json.hpp>

namespace bklv
{
namespace
{

// ---------------------------------------------------------------- 拼音字库

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
        const std::string pinyins = it.value().get<std::string>();
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

// ---------------------------------------------------------------- UTF-8

std::vector<std::string> splitChars(const std::string& value)
{
    std::vector<std::string> chars;
    for (size_t i = 0; i < value.size();)
    {
        const unsigned char lead = (unsigned char)value[i];
        size_t length = 1;
        if ((lead & 0x80) == 0)
            length = 1;
        else if ((lead & 0xE0) == 0xC0)
            length = 2;
        else if ((lead & 0xF0) == 0xE0)
            length = 3;
        else if ((lead & 0xF8) == 0xF0)
            length = 4;

        length = std::min(length, value.size() - i);
        chars.push_back(value.substr(i, length));
        i += length;
    }
    return chars;
}

std::string joinChars(const std::vector<std::string>& chars, size_t count)
{
    std::string out;
    for (size_t i = 0; i < count && i < chars.size(); i++)
        out += chars[i];
    return out;
}

size_t byteOffsetOf(const std::string& value, size_t charIndex)
{
    size_t offset = 0;
    size_t chars  = 0;
    while (offset < value.size() && chars < charIndex)
    {
        const unsigned char lead = (unsigned char)value[offset];
        size_t length = 1;
        if ((lead & 0xE0) == 0xC0)
            length = 2;
        else if ((lead & 0xF0) == 0xE0)
            length = 3;
        else if ((lead & 0xF8) == 0xF0)
            length = 4;
        offset += std::min(length, value.size() - offset);
        chars++;
    }
    return offset;
}

// ---------------------------------------------------------------- 输入框

float textWidth(const std::string& value, float fontSize)
{
    ImFont* font = getDefaultFont();
    if (font == nullptr)
        return 0.0f;
    return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, value.c_str()).x;
}

// 记录按下位置（轻触），用来把光标放到点击处
class TapPositionGesture : public GestureRecognizer
{
  public:
    explicit TapPositionGesture(std::function<void(const Point&)> handler)
        : handler(std::move(handler))
    {
    }

    bool recognize(GestureState state, const Point& position, const Point& delta) override
    {
        (void)delta;
        if (state == GestureState::START)
        {
            this->start  = position;
            this->active = true;
            return true;
        }
        if (state == GestureState::END && this->active)
        {
            this->active = false;
            if (this->handler)
                this->handler(this->start);
            return true;
        }
        return this->active;
    }

  private:
    std::function<void(const Point&)> handler;
    Point start{ 0.0f, 0.0f };
};

} // namespace

// 大输入框：背景 + 文本 + 光标（点击定位）
class ImeField : public View
{
  public:
    std::function<std::string()> textProvider;
    std::function<size_t()> caretProvider;
    std::function<void(size_t)> onCaret;
    float blinkTimer = 0.0f;

    ImeField()
    {
        this->addGesture(std::make_unique<TapPositionGesture>([this](const Point& p) {
            this->handleTap(p);
        }));
    }

    std::string getClassString() const override { return "bklv::ImeField"; }

    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        FrameContext*) override
    {
        if (drawList == nullptr || width <= 0.0f || height <= 0.0f)
            return;

        const ImU32 bg = getTheme().color("color.surface.card");
        const ImU32 border = getTheme().color("color.border.default");
        const ImU32 textColor = getTheme().color("color.text.primary");
        const float radius = getTheme().number("metric.radius.md", 6.0f);

        drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), bg, radius);
        drawList->AddRect(ImVec2(x, y), ImVec2(x + width, y + height), border, radius, 0, 1.0f);

        const std::string value = this->textProvider ? this->textProvider() : std::string();
        const size_t caretIndex = this->caretProvider ? this->caretProvider() : 0;
        const float fontSize    = 24.0f;
        const float padding     = 16.0f;
        ImFont* font            = getDefaultFont();

        if (font != nullptr)
        {
            const std::string prefix = joinChars(splitChars(value), caretIndex);
            const float caretX       = x + padding + textWidth(prefix, fontSize);
            const float baselineY    = y + (height - fontSize) * 0.5f;

            drawList->AddText(font, fontSize, ImVec2(x + padding, baselineY), textColor,
                value.c_str());

            if (this->blinkTimer < 0.5f)
            {
                drawList->AddLine(ImVec2(caretX, y + 14.0f),
                    ImVec2(caretX, y + height - 14.0f), textColor, 2.0f);
            }
        }
    }

  private:
    void handleTap(const Point& position)
    {
        const Rect frame = this->getFrame();
        const float localX = position.x - frame.x - 16.0f;
        const std::string value = this->textProvider ? this->textProvider() : std::string();
        const std::vector<std::string> chars = splitChars(value);

        size_t caret = 0;
        float accumulated = 0.0f;
        for (size_t i = 0; i < chars.size(); i++)
        {
            const float w = textWidth(chars[i], 24.0f);
            if (localX < accumulated + w * 0.5f)
                break;
            accumulated += w;
            caret = i + 1;
        }
        if (this->onCaret)
            this->onCaret(caret);
    }
};

// ---------------------------------------------------------------- 面板

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
    this->setPadding(20.0f);
    this->setGap(16.0f);
    this->setJustifyContent(JustifyContent::SPACE_BETWEEN);
    this->setBackgroundColor(getTheme().color("color.common.overlay", IM_COL32(0, 0, 0, 150)));

    this->buildLayout();
    this->caret = splitChars(this->text).size();
    this->refreshLabels();

    // 手柄 / 键盘 LB、RB：移动光标（触摸点击另有一条路）
    this->registerAction(ControllerButton::LB, "光标左", [this](View*) { this->moveCaret(-1); });
    this->registerAction(ControllerButton::RB, "光标右", [this](View*) { this->moveCaret(1); });
}

void ImePanel::buildLayout()
{
    this->field = new ImeField();
    this->field->setWidthPercentage(100.0f);
    this->field->setHeight(84.0f);
    this->field->setMarginTop(8.0f);
    this->field->textProvider = [this] { return this->textWithComposing(); };
    this->field->caretProvider = [this] { return this->caretChars(); };
    this->field->onCaret = [this](size_t index) { this->setCaret(index); };
    this->addView(this->field);

    this->hintLabel = new Label();
    this->hintLabel->setFontSize(14.0f);
    this->hintLabel->setTextColor(getTheme().color("color.text.dim"));
    this->addView(this->hintLabel);

    this->candidateRow = new Box(Axis::ROW);
    this->candidateRow->setGap(8.0f);
    this->addView(this->candidateRow);

    Box* keyboard = new Box(Axis::COLUMN);
    keyboard->setWidthPercentage(100.0f);
    keyboard->setGap(10.0f);
    keyboard->setPadding(14.0f);
    keyboard->setBackgroundColor(getTheme().color("color.surface.card"));
    keyboard->setCornerRadius(getTheme().number("metric.radius.lg", 8.0f));

    Box* columns = new Box(Axis::ROW);
    columns->setWidthPercentage(100.0f);
    columns->setGap(16.0f);
    columns->setAlignItems(AlignItems::FLEX_END);

    Box* numberPad = new Box(Axis::COLUMN);
    numberPad->setWidth(190.0f);
    numberPad->setGap(6.0f);
    this->buildNumberPad(numberPad);
    columns->addView(numberPad);

    Box* letters = new Box(Axis::COLUMN);
    letters->setGrow(1.0f);
    letters->setGap(6.0f);
    this->buildFullKeyboard(letters);
    columns->addView(letters);

    keyboard->addView(columns);
    this->addView(keyboard);
}

void ImePanel::buildNumberPad(Box* parent)
{
    const char* grid[4][3] = {
        { "7", "8", "9" },
        { "4", "5", "6" },
        { "1", "2", "3" },
        { ".", "0", "退格" },
    };

    for (int r = 0; r < 4; r++)
    {
        Box* row = new Box(Axis::ROW);
        row->setGap(6.0f);
        row->setJustifyContent(JustifyContent::CENTER);
        for (int c = 0; c < 3; c++)
        {
            const std::string label = grid[r][c];
            Button* key = new Button(label);
            key->setMinWidth(0.0f);
            key->setWidth(56.0f);
            key->setHeight(46.0f);
            if (label == "退格")
                key->setOnClick([this] { this->backspace(); });
            else if (label == ".")
                key->setOnClick([this] {
                    if (!this->numeric)
                        this->insertText(".");
                });
            else
                key->setOnClick([this, label] { this->insertText(label); });
            row->addView(key);
        }
        parent->addView(row);
    }
}

void ImePanel::buildFullKeyboard(Box* parent)
{
    const char* rows[3] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
    for (int r = 0; r < 3; r++)
    {
        Box* row = new Box(Axis::ROW);
        row->setGap(6.0f);
        row->setJustifyContent(JustifyContent::CENTER);
        for (const char* c = rows[r]; *c != 0; c++)
        {
            const std::string value(1, *c);
            Button* key = new Button(value);
            key->setMinWidth(0.0f);
            key->setWidth(52.0f);
            key->setHeight(46.0f);
            this->letterKeys.emplace_back(key, value);
            key->setOnClick([this, value] {
                if (this->chinese && !this->numeric)
                {
                    this->composing += value;
                    this->caret = splitChars(this->text).size();
                    this->refreshLabels();
                    this->refreshCandidates();
                }
                else
                    this->insertText(value);
            });
            row->addView(key);
        }
        parent->addView(row);
    }

    Box* bottom = new Box(Axis::ROW);
    bottom->setGap(8.0f);
    bottom->setWidthPercentage(100.0f);
    bottom->setAlignItems(AlignItems::CENTER);

    Button* language = new Button("中/英");
    language->setMinWidth(0.0f);
    language->setOnClick([this] { this->toggleLanguage(); });
    language->setVisibility(this->numeric ? Visibility::GONE : Visibility::VISIBLE);
    bottom->addView(language);

    Button* shiftKey = new Button("大写");
    shiftKey->setMinWidth(0.0f);
    shiftKey->setOnClick([this] { this->toggleShift(); });
    shiftKey->setVisibility(this->numeric ? Visibility::GONE : Visibility::VISIBLE);
    bottom->addView(shiftKey);

    Button* space = new Button("空格");
    space->setMinWidth(0.0f);
    space->setGrow(1.0f);
    space->setOnClick([this] { this->insertText(" "); });
    bottom->addView(space);

    Button* cancelButton = new Button("取消");
    cancelButton->setMinWidth(0.0f);
    cancelButton->setOnClick([this] { this->cancel(); });
    bottom->addView(cancelButton);

    Button* confirmButton = new Button("确定");
    confirmButton->setMinWidth(0.0f);
    confirmButton->setOnClick([this] { this->confirm(); });
    bottom->addView(confirmButton);

    parent->addView(bottom);
}

std::string ImePanel::displayText() const
{
    return this->text;
}

std::string ImePanel::textWithComposing() const
{
    if (this->composing.empty())
        return this->text;

    const size_t offset = byteOffsetOf(this->text, this->caret);
    return this->text.substr(0, offset) + this->composing + this->text.substr(offset);
}

size_t ImePanel::caretChars() const
{
    return this->caret + splitChars(this->composing).size();
}

void ImePanel::insertText(const std::string& value)
{
    if (value.empty())
        return;

    if (this->numeric)
    {
        for (char c : value)
            if ((c < '0' || c > '9') && c != '.')
                return;
    }

    std::string insert = value;
    if (this->shift && !this->chinese)
        for (char& c : insert)
            c = (char)std::toupper((unsigned char)c);

    if (splitChars(this->text).size() + splitChars(insert).size() > (size_t)this->maxLength)
        return;

    const size_t offset = byteOffsetOf(this->text, this->caret);
    this->text.insert(offset, insert);
    this->caret += splitChars(insert).size();
    this->refreshLabels();
}

void ImePanel::insertAscii(const std::string& value)
{
    this->insertText(value);
}

void ImePanel::backspace()
{
    if (!this->composing.empty())
    {
        std::vector<std::string> chars = splitChars(this->composing);
        chars.pop_back();
        this->composing = joinChars(chars, chars.size());
        this->refreshLabels();
        this->refreshCandidates();
        return;
    }

    if (this->caret == 0 || this->text.empty())
        return;

    std::vector<std::string> chars = splitChars(this->text);
    chars.erase(chars.begin() + (long)(this->caret - 1));
    this->text = joinChars(chars, chars.size());
    this->caret--;
    this->refreshLabels();
}

void ImePanel::commitComposing()
{
    if (this->composing.empty())
        return;
    this->insertText(this->composing);
    this->composing.clear();
}

void ImePanel::moveCaret(int delta)
{
    const long target = (long)this->caret + delta;
    const long maxCaret = (long)splitChars(this->text).size();
    this->caret = (size_t)std::max(0L, std::min(target, maxCaret));
    this->refreshLabels();
}

void ImePanel::setCaret(size_t charIndex)
{
    const size_t maxCaret = splitChars(this->text).size();
    this->caret = std::min(charIndex, maxCaret);
    this->refreshLabels();
}

void ImePanel::toggleLanguage()
{
    if (this->numeric)
        return;
    this->commitComposing();
    this->chinese = !this->chinese;
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

std::vector<std::string> ImePanel::getCandidates() const
{
    std::vector<std::string> out;
    if (!this->chinese || this->composing.empty())
        return out;

    for (const auto& item : pinyinDictionary())
    {
        if (item.first.rfind(this->composing, 0) == 0)
        {
            if (std::find(out.begin(), out.end(), item.second) == out.end())
                out.push_back(item.second);
            if (out.size() >= 8)
                break;
        }
    }
    return out;
}

void ImePanel::refreshCandidates()
{
    if (this->candidateRow == nullptr)
        return;

    for (Button* button : this->candidateButtons)
        this->candidateRow->removeView(button, true);
    this->candidateButtons.clear();

    const std::vector<std::string> candidates = this->getCandidates();
    for (size_t i = 0; i < candidates.size(); i++)
    {
        Button* button = new Button(candidates[i]);
        button->setOnClick([this, i] { this->chooseCandidate(i); });
        this->candidateRow->addView(button);
        this->candidateButtons.push_back(button);
    }
}

void ImePanel::chooseCandidate(size_t index)
{
    const std::vector<std::string> candidates = this->getCandidates();
    if (index >= candidates.size())
        return;

    this->insertText(candidates[index]);
    this->composing.clear();
    this->refreshLabels();
    this->refreshCandidates();
}

void ImePanel::refreshLabels()
{
    this->field->blinkTimer = 0.0f;
    if (this->hintLabel == nullptr)
        return;

    if (this->composing.empty())
    {
        const std::string mode = this->numeric ? "数字输入" : (this->chinese ? "拼音输入" : "英文输入");
        this->hintLabel->setText(mode + "  ·  " + std::to_string(splitChars(this->text).size())
            + "/" + std::to_string(this->maxLength) + "  ·  点击输入框或 LB/RB 移动光标");
    }
    else
    {
        this->hintLabel->setText("拼音：" + this->composing + "  ·  点击候选上屏");
    }
}

void ImePanel::update()
{
    Box::update();
    this->blinkTimer += getFrameDeltaSeconds();
    if (this->blinkTimer >= 1.0f)
        this->blinkTimer = 0.0f;
    if (this->field != nullptr)
        this->field->blinkTimer = this->blinkTimer;
}

void ImePanel::confirm()
{
    this->commitComposing();
    this->finish(true);
}

void ImePanel::cancel()
{
    this->finish(false);
}

void ImePanel::finish(bool accepted)
{
    const std::string result = this->text;
    std::function<void(bool, const std::string&)> done = this->callback;
    if (App* app = App::getCurrent())
        app->popOverlay(this, true);
    if (done)
        done(accepted, result);
}

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
