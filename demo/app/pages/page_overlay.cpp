#include "app/pages.hpp"

#include "app/ui.hpp"
#include "bklv/bklv.hpp"

using namespace bklv;

namespace demo
{

namespace
{

// 弹窗是复用的（每次点按钮不重建），但页面会被 Pager 在切页时 delete ——
// 弹窗里捕获的页面内视图（比如下面那个回显 Label）会因此悬空。
// 所以每次重建页面时把**没开着**的旧弹窗丢掉，下面重新建一份带新捕获的。
void dropStaleDialogs(Dialog*& dialog, EditTextDialog*& textDialog)
{
    if (dialog != nullptr && !dialog->isOpen())
    {
        delete dialog;
        dialog = nullptr;
    }
    if (textDialog != nullptr && !textDialog->isOpen())
    {
        delete textDialog;
        textDialog = nullptr;
    }
}

} // namespace

Box* buildOverlayPage()
{
    Box* page = makeColumn(getTheme().number("metric.spacing.md", 12.0f));
    page->addView(makeHint(getStr("overlay/hint")));

    // ---- 弹窗 ----
    Rectangle* dialogCard = makeCard();
    dialogCard->addView(makeText(getStr("theme/dialog_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));
    dialogCard->addView(makeHint(getStr("theme/dialog_desc")));

    static Dialog* dialog = nullptr;
    static EditTextDialog* textDialog = nullptr;
    dropStaleDialogs(dialog, textDialog);

    Button* openDialog = new Button(getStr("theme/open_dialog"));
    openDialog->setOnClick([] {
        if (dialog == nullptr)
        {
            dialog = new Dialog();
            dialog->setTitle(getStr("theme/dialog_demo_title"));
            dialog->setText(getStr("theme/dialog_demo_text"));
            dialog->addButton(getStr("hints/ok"), [] { dialog->close(); });
            dialog->addButton(getStr("theme/dialog_cancel"), [] { dialog->close(); });
        }
        dialog->open();
    });
    dialogCard->addView(openDialog);
    page->addView(dialogCard);

    // ---- 文本输入 ----
    Rectangle* inputCard = makeCard();
    inputCard->addView(makeText(getStr("theme/input_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));
    inputCard->addView(makeHint(getStr("theme/input_desc")));

    Label* entered = makeHint(getStr("theme/input_echo", getStr("theme/input_empty")));

    Button* openInput = new Button(getStr("theme/open_input"));
    openInput->setOnClick([entered] {
        if (textDialog == nullptr)
        {
            textDialog = new EditTextDialog();
            textDialog->setTitle(getStr("theme/input_dialog_title"));
            textDialog->setHintText(getStr("theme/input_hint"));
            textDialog->setMaxLength(16);
            textDialog->setOnSubmit([entered](const std::string& text) {
                entered->setText(getStr("theme/input_echo",
                    text.empty() ? getStr("theme/input_empty") : text));
                textDialog->close();
            });
            textDialog->setOnCancel([] { textDialog->close(); });
        }
        textDialog->open();
    });

    Box* inputRow = makeRow(getTheme().number("metric.spacing.md", 12.0f));
    inputRow->addView(openInput);
    inputRow->addView(entered);
    inputCard->addView(inputRow);
    page->addView(inputCard);

    return page;
}

} // namespace demo
