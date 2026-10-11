#include "bklv/widgets/edit_text_dialog.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "bklv/core/app.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/time.hpp"

namespace bklv
{

// ---------------------------------------------------------------------------
// 文本模型
// ---------------------------------------------------------------------------

namespace
{

// UTF-8 字符长度（首字节的高位决定后续还有几个续字节）
size_t utf8Length(unsigned char first)
{
    if (first < 0x80)
        return 1;
    if ((first >> 5) == 0x6)
        return 2;
    if ((first >> 4) == 0xE)
        return 3;
    if ((first >> 3) == 0x1E)
        return 4;
    return 1;
}

bool isAsciiDigit(const std::string& c)
{
    return c.size() == 1 && c[0] >= '0' && c[0] <= '9';
}

// ImWchar -> UTF-8。自己编，避免依赖 imgui_internal.h（ImTextCharToUtf8 在那里）
std::string appendUtf8(unsigned int codepoint)
{
    std::string out;
    if (codepoint < 0x80)
        out.push_back((char)codepoint);
    else if (codepoint < 0x800)
    {
        out.push_back((char)(0xC0 | (codepoint >> 6)));
        out.push_back((char)(0x80 | (codepoint & 0x3F)));
    }
    else if (codepoint < 0x10000)
    {
        out.push_back((char)(0xE0 | (codepoint >> 12)));
        out.push_back((char)(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (codepoint & 0x3F)));
    }
    else
    {
        out.push_back((char)(0xF0 | (codepoint >> 18)));
        out.push_back((char)(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back((char)(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (codepoint & 0x3F)));
    }
    return out;
}

// 单行文本模型

} // namespace

std::vector<std::string> TextEditModel::splitChars() const
{
    std::vector<std::string> chars;
    for (size_t i = 0; i < this->text.size();)
    {
        const size_t len = utf8Length((unsigned char)this->text[i]);
        const size_t end = std::min(this->text.size(), i + len);
        chars.push_back(this->text.substr(i, end - i));
        i = end;
    }
    return chars;
}

size_t TextEditModel::cursorToByte(const std::vector<std::string>& chars, size_t index) const
{
    size_t bytes = 0;
    for (size_t i = 0; i < index && i < chars.size(); i++)
        bytes += chars[i].size();
    return bytes;
}

size_t TextEditModel::getCharCount() const
{
    return this->splitChars().size();
}

void TextEditModel::setText(const std::string& value)
{
    this->text = value;
    if (this->maxLength > 0)
    {
        // 超长的话按字符截断
        std::vector<std::string> chars = this->splitChars();
        if (chars.size() > this->maxLength)
        {
            chars.resize(this->maxLength);
            this->text.clear();
            for (const std::string& c : chars)
                this->text += c;
        }
    }

    this->cursor = this->getCharCount(); // 光标放到末尾
}

void TextEditModel::setCursor(size_t value)
{
    const size_t count = this->getCharCount();
    this->cursor       = std::min(value, count);
}

bool TextEditModel::insert(const std::string& utf8)
{
    if (utf8.empty())
        return false;

    std::vector<std::string> chars = this->splitChars();
    std::vector<std::string> inserted;

    for (size_t i = 0; i < utf8.size();)
    {
        const size_t len = utf8Length((unsigned char)utf8[i]);
        const size_t end = std::min(utf8.size(), i + len);
        std::string c    = utf8.substr(i, end - i);

        // 数字键盘：非数字直接忽略（和系统数字键盘的行为一致）
        if (this->numeric && !isAsciiDigit(c))
        {
            i = end;
            continue;
        }

        inserted.push_back(c);
        i = end;
    }

    if (inserted.empty())
        return false;

    // 超长就按剩余额度截断，不做"整段拒绝"
    const size_t room = this->maxLength > chars.size() ? this->maxLength - chars.size() : 0;
    if (room == 0)
        return false;

    if (inserted.size() > room)
        inserted.resize(room);

    chars.insert(chars.begin() + (long)std::min(this->cursor, chars.size()), inserted.begin(),
        inserted.end());

    this->text.clear();
    for (const std::string& c : chars)
        this->text += c;

    this->cursor = std::min(this->cursor + inserted.size(), chars.size());
    return true;
}

bool TextEditModel::backspace()
{
    std::vector<std::string> chars = this->splitChars();
    if (this->cursor == 0 || chars.empty())
        return false;

    chars.erase(chars.begin() + (long)(this->cursor - 1));
    this->cursor--;

    this->text.clear();
    for (const std::string& c : chars)
        this->text += c;
    return true;
}

bool TextEditModel::deleteForward()
{
    std::vector<std::string> chars = this->splitChars();
    if (this->cursor >= chars.size())
        return false;

    chars.erase(chars.begin() + (long)this->cursor);

    this->text.clear();
    for (const std::string& c : chars)
        this->text += c;
    return true;
}

void TextEditModel::moveCursor(int delta)
{
    const long next = (long)this->cursor + delta;
    this->cursor    = (size_t)std::max(0L, std::min(next, (long)this->getCharCount()));
}

// ---------------------------------------------------------------------------
// 面板：外观里的 title 文字层要有个数据源
// ---------------------------------------------------------------------------

class EditPanel : public AppearanceView
{
  public:
    EditPanel()
        : AppearanceView("edit_text_dialog", true)
    {
    }

    std::function<std::string()> titleProvider;

  protected:
    std::string getLayerText(const std::string& source) override
    {
        if (source == "title" && this->titleProvider)
            return this->titleProvider();

        return AppearanceView::getLayerText(source);
    }
};

// ---------------------------------------------------------------------------
// 输入框
// ---------------------------------------------------------------------------

class TextField : public AppearanceView
{
  public:
    TextField()
        : AppearanceView("text_field", false)
    {
        this->setWidthPercentage(100.0f);
        this->setHeight(getTheme().number("metric.textField.height", 48.0f));
        this->enableContentMeasurement();
        this->applyTheme();
    }

    std::function<std::string()> textProvider;
    std::function<std::string()> hintProvider;
    std::function<std::string()> counterProvider;
    std::function<size_t()> cursorProvider;

  protected:
    std::string getLayerText(const std::string& source) override
    {
        const std::string text = this->textProvider ? this->textProvider() : std::string();

        if (source == "text")
            return text.empty() && this->hintProvider ? this->hintProvider() : text;
        if (source == "counter")
            return this->counterProvider ? this->counterProvider() : std::string();

        return AppearanceView::getLayerText(source);
    }

    void drawCustomLayer(ImDrawList* drawList, const AppearanceLayer& layer, float x, float y,
        float, float height, const AppearanceInstance& appearance) override
    {
        if (layer.id != "caret" || !this->cursorProvider)
            return;

        // 闪烁按时间算，不进文本模型（也就不进外观状态机）
        const float now = ImGui::GetCurrentContext() != nullptr ? (float)ImGui::GetTime() : 0.0f;
        if (std::fmod(now, 1.0f) > 0.5f)
            return;

        ImFont* font = getDefaultFont();
        if (font == nullptr)
            return;

        // 光标画在"光标前面那段文本"的宽度处；按字符切，中文不会被切坏
        const std::string text = this->textProvider ? this->textProvider() : std::string();
        const size_t cursor    = this->cursorProvider();

        std::string before;
        size_t index = 0;
        for (size_t i = 0; i < text.size() && index < cursor;)
        {
            const size_t len = utf8Length((unsigned char)text[i]);
            const size_t end = std::min(text.size(), i + len);
            before += text.substr(i, end - i);
            i = end;
            index++;
        }

        const int layerIndex = appearance.getAppearance().indexOfLayer(layer.id);
        const float fontSize = appearance.getNumber(layerIndex, AppearanceProp::FontSize);
        const float size     = fontSize > 0.0f ? fontSize : 15.0f;
        const float offsetX  = font->CalcTextSizeA(size, FLT_MAX, 0.0f, before.c_str()).x;
        const ImU32 color    = appearance.getColor(layerIndex, AppearanceProp::Fill);

        drawList->AddRectFilled(ImVec2(x + offsetX, y), ImVec2(x + offsetX + 2.0f, y + height), color);
    }
};

// ---------------------------------------------------------------------------
// 弹窗
// ---------------------------------------------------------------------------

EditTextDialog::EditTextDialog()
    : Box(Axis::COLUMN)
{
    this->setJustifyContent(JustifyContent::CENTER);
    this->setAlignItems(AlignItems::CENTER);

    this->backdrop = new AppearanceView("dialog_backdrop", false);
    this->backdrop->setPositionType(PositionType::ABSOLUTE);
    this->backdrop->setPositionTop(0.0f);
    this->backdrop->setPositionLeft(0.0f);
    this->backdrop->setWidthPercentage(100.0f);
    this->backdrop->setHeightPercentage(100.0f);
    this->addView(this->backdrop);

    // 面板：外观里的 title 文字层画标题；输入框与按钮行**放进面板里**
    EditPanel* panel = new EditPanel();
    panel->titleProvider = [this] { return this->title; };
    panel->setPositionType(PositionType::ABSOLUTE);
    panel->setAlignSelf(AlignSelf::CENTER);
    panel->setWidth(getTheme().number("metric.editTextDialog.width", 560.0f));
    panel->setHeight(AUTO);
    panel->setPadding(getTheme().number("metric.spacing.lg", 16.0f));
    panel->setGap(getTheme().number("metric.spacing.md", 12.0f));
    panel->setJustifyContent(JustifyContent::FLEX_END);
    this->panel = panel;
    this->addView(panel);

    // 输入框：单独一层外观，显示当前文本/占位提示/计数，并自己画光标
    TextField* field = new TextField();
    field->textProvider    = [this] { return this->model.getText(); };
    field->hintProvider    = [this] { return this->hint; };
    field->cursorProvider  = [this] { return this->model.getCursor(); };
    field->counterProvider = [this] {
        return std::to_string(this->model.getCharCount()) + "/"
            + std::to_string(this->model.getMaxLength());
    };
    this->field = field;
    this->field->setMarginTop(getTheme().number("metric.spacing.lg", 16.0f)); // 给标题层让位
    panel->addView(this->field);

    this->confirmButton = new Button(getStr("hints/ok"));
    this->confirmButton->setOnClick([this] { this->handleSubmit(); });
    this->cancelButton = new Button(getStr("widgets/cancel"));
    this->cancelButton->setOnClick([this] { this->handleCancel(); });

    Box* row = new Box(Axis::ROW);
    row->setGap(getTheme().number("metric.spacing.sm", 8.0f));
    row->setJustifyContent(JustifyContent::FLEX_END);
    row->setWidthPercentage(100.0f);
    row->addView(this->confirmButton);
    row->addView(this->cancelButton);
    panel->addView(row);

    this->backdrop->setAppearanceState("closed", true);
    this->panel->setAppearanceState("closed", true);

    this->registerAction(ControllerButton::B, getStr("hints/cancel"), [this](View*) {
        this->handleCancel();
    });
}

void EditTextDialog::setTitle(const std::string& value)
{
    this->title = value;
    this->refreshFieldText();
}

void EditTextDialog::setHintText(const std::string& value)
{
    this->hint = value;
    this->refreshFieldText();
}

void EditTextDialog::setText(const std::string& value)
{
    this->model.setText(value);
    this->refreshFieldText();
}

void EditTextDialog::setMaxLength(size_t value)
{
    this->model.setMaxLength(value);
    this->refreshFieldText();
}

void EditTextDialog::refreshFieldText()
{
    // 输入框显示：有内容就显示内容，空的话显示占位提示
    this->field->setText(this->model.getText().empty() ? this->hint : this->model.getText());
    this->invalidate();
}

void EditTextDialog::open()
{
    if (this->opened)
        return;

    App* app = App::getCurrent();
    if (app == nullptr)
    {
        BKLV_LOG_ERROR("EditTextDialog::open 失败：找不到 App 实例");
        return;
    }

    if (!app->pushOverlay(this))
        return;

    this->opened  = true;
    this->closing = false;

    this->backdrop->setAppearanceState("closed", false);
    this->backdrop->setAppearanceState("open", true);
    this->panel->setAppearanceState("closed", false);
    this->panel->setAppearanceState("open", true);

    this->refreshFieldText();

    // 先问平台：Switch 会弹系统软键盘；桌面没人接手，退回内联键盘输入
    this->platformImeOpen = this->openPlatformIme();
}

bool EditTextDialog::openPlatformIme()
{
    ImeRequest request;
    request.header      = this->title;
    request.subText     = this->hint;
    request.initialText = this->model.getText();
    request.maxLength   = (int)this->model.getMaxLength();
    request.numeric     = this->model.isNumeric();
    request.callback    = [this](bool accepted, const std::string& text) {
        // 软键盘关掉之后回到这个弹窗：取消就当没输，确定就填进去
        if (accepted)
            this->model.setText(text);

        this->refreshFieldText();
    };

    return getImeManager().open(request);
}

void EditTextDialog::handleSubmit()
{
    if (!this->opened || this->closing)
        return;

    this->closing = true;

    if (this->onSubmit)
        this->onSubmit(this->model.getText());
}

void EditTextDialog::handleCancel()
{
    if (!this->opened || this->closing)
        return;

    this->closing = true;

    if (this->onCancel)
        this->onCancel();
}

void EditTextDialog::close()
{
    if (!this->opened || this->closing)
        return;

    this->closing = true;
    this->backdrop->setAppearanceState("open", false);
    this->backdrop->setAppearanceState("closed", true);
    this->panel->setAppearanceState("open", false);
    this->panel->setAppearanceState("closed", true);
}

void EditTextDialog::update()
{
    Box::update();

    // 内联编辑：平台没接手软键盘时，自己读键盘
    if (this->opened && !this->closing && !this->platformImeOpen
        && ImGui::GetCurrentContext() != nullptr)
    {
        ImGuiIO& io = ImGui::GetIO();

        // 字符输入（含中文输入法上屏的文本）
        if (!io.InputQueueCharacters.empty())
        {
            std::string typed;
            for (ImWchar c : io.InputQueueCharacters)
            {
                typed += appendUtf8((unsigned int)c);
            }
            io.InputQueueCharacters.resize(0);

            if (this->model.insert(typed))
                this->refreshFieldText();
        }

        const bool repeat = true;
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace, repeat))
        {
            if (this->model.backspace())
                this->refreshFieldText();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, repeat))
        {
            if (this->model.deleteForward())
                this->refreshFieldText();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, repeat))
            this->model.moveCursor(-1);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, repeat))
            this->model.moveCursor(1);
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
            this->handleSubmit();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            this->handleCancel();
    }

    if (!this->closing)
        return;

    // 关闭补间播完再摘弹层（和 Dialog 同一个理由：摘了就脱离视图树，动效播不出来）
    if (this->backdrop->getAppearanceInstanceForTest().isAnimating()
        || this->panel->getAppearanceInstanceForTest().isAnimating())
        return;

    this->closing = false;
    this->opened  = false;
    this->platformImeOpen = false;

    if (App* app = App::getCurrent())
        app->popOverlay(this, false);
}


} // namespace bklv
