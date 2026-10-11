// 无窗口测试：文本编辑模型 + 文本输入弹窗。
//
// 编辑逻辑（插入/退格/删除/光标/长度上限/数字过滤）是纯数据，能直接断言；
// 弹窗部分验它挂没挂成弹层、提交/取消回调、以及 B 冒泡。

#include <cstdio>
#include <string>

#include "bklv/core/app.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/ime.hpp"
#include "bklv/core/input.hpp"
#include "bklv/widgets/edit_text_dialog.hpp"

using namespace bklv;

static int g_failed = 0;
static int g_checks = 0;

static void check(bool ok, const std::string& what)
{
    g_checks++;
    if (ok)
        std::printf("  [通过] %s\n", what.c_str());
    else
    {
        g_failed++;
        std::printf("  [失败] %s\n", what.c_str());
    }
}

static void checkEqual(const std::string& got, const std::string& want, const std::string& what)
{
    g_checks++;
    if (got == want)
        std::printf("  [通过] %s  =>  %s\n", what.c_str(), got.c_str());
    else
    {
        g_failed++;
        std::printf("  [失败] %s  期望 %s 实际 %s\n", what.c_str(), want.c_str(), got.c_str());
    }
}

static void checkEqualSize(size_t got, size_t want, const std::string& what)
{
    g_checks++;
    if (got == want)
        std::printf("  [通过] %s  =>  %zu\n", what.c_str(), got);
    else
    {
        g_failed++;
        std::printf("  [失败] %s  期望 %zu 实际 %zu\n", what.c_str(), want, got);
    }
}

int main()
{
    std::printf("\n== 1. 文本模型：插入与光标 ==\n");
    {
        TextEditModel model;
        checkEqual(model.getText(), "", "初始为空");
        checkEqualSize(model.getCursor(), 0, "光标在开头");

        check(model.insert("abc"), "插入 abc");
        checkEqual(model.getText(), "abc", "内容正确");
        checkEqualSize(model.getCursor(), 3, "光标跟到末尾");
        check(!model.insert(""), "插空串不算改动");

        // 光标移到中间再插
        model.setCursor(1);
        check(model.insert("X"), "在光标处插入");
        checkEqual(model.getText(), "aXbc", "插在光标位置");
    }

    std::printf("\n== 2. 中文按字符（不是字节）走 ==\n");
    {
        TextEditModel model;
        model.insert("中文测试");
        checkEqualSize(model.getCharCount(), 4, "四个汉字算四个字符");
        checkEqualSize(model.getCursor(), 4, "光标在末尾");

        model.moveCursor(-2);
        checkEqualSize(model.getCursor(), 2, "左移两格");
        check(model.insert("好"), "中间插入一个汉字");
        checkEqual(model.getText(), "中文好测试", "没有被切坏成半个字");

        check(model.backspace(), "退格");
        checkEqual(model.getText(), "中文测试", "退格删掉一个完整汉字");
    }

    std::printf("\n== 3. 退格 / 前删 / 方向键 ==\n");
    {
        TextEditModel model;
        model.insert("hello");
        model.setCursor(0);
        check(!model.backspace(), "光标在最前时退格无效");
        check(model.deleteForward(), "前删有效");
        checkEqual(model.getText(), "ello", "删掉第一个字符");

        model.setCursor(4);
        check(!model.deleteForward(), "光标在末尾时前删无效");
        check(model.backspace(), "退格有效");
        checkEqual(model.getText(), "ell", "退格删掉最后一个字符");

        model.moveCursor(-10);
        checkEqualSize(model.getCursor(), 0, "左移会被夹到 0");
        model.moveCursor(99);
        checkEqualSize(model.getCursor(), 3, "右移会被夹到末尾");
    }

    std::printf("\n== 4. 长度上限与数字过滤 ==\n");
    {
        TextEditModel model;
        model.setMaxLength(5);
        check(model.insert("abcdefgh"), "超长插入会截断而不是整段拒绝");
        checkEqual(model.getText(), "abcde", "只留前 5 个字符");
        check(!model.insert("x"), "已经满了就不再接受");

        TextEditModel numeric;
        numeric.setNumeric(true);
        numeric.setMaxLength(4);
        check(numeric.insert("12ab34"), "数字模式只收数字");
        checkEqual(numeric.getText(), "1234", "字母被过滤，数字留下");

        TextEditModel preset;
        preset.setMaxLength(3);
        preset.setText("abcdef");
        checkEqual(preset.getText(), "abc", "setText 也会按上限截断");
        checkEqualSize(preset.getCursor(), 3, "setText 后光标在末尾");
    }

    std::printf("\n== 5. 弹窗：挂载与提交/取消 ==\n");
    {
        App app;
        Box* root = new Box(Axis::COLUMN);
        app.setRootView(root);

        EditTextDialog* dialog = new EditTextDialog();
        dialog->setTitle("输入名称");
        dialog->setHintText("请输入");
        dialog->setText("初始值");
        dialog->setMaxLength(10);

        std::string submitted;
        bool cancelled = false;
        dialog->setOnSubmit([&submitted](const std::string& text) { submitted = text; });
        dialog->setOnCancel([&cancelled] { cancelled = true; });

        check(!dialog->isOpen(), "初始是关着的");
        dialog->open();
        check(dialog->isOpen(), "open() 之后打开");
        check(root->getChildrenCount() == 1, "挂成了弹层");
        check(getInputManager().hasModal(), "输入切到模态");

        // 桌面没有平台 IME：openPlatformIme 返回 false，退回内联编辑
        check(!hasPlatformIme(), "桌面没有平台软键盘");
        check(!dialog->openPlatformIme(), "平台不接手，控件自己编辑");

        check(dialog->insertText("追加"), "内联插入");
        checkEqual(dialog->getText(), "初始值追加", "内容更新");

        dialog->handleSubmit();
        checkEqual(submitted, "初始值追加", "提交回调拿到最终文本");

        for (int i = 0; i < 60; i++)
            dialog->update();
        check(!getInputManager().hasModal(), "关闭补间播完后摘掉弹层");

        // 取消路径
        dialog->open();
        dialog->handleCancel();
        check(cancelled, "取消回调被调用");
        for (int i = 0; i < 60; i++)
            dialog->update();
        check(!dialog->isOpen(), "取消后也回到关闭状态");
    }

    std::printf("\n== 6. 平台 IME 接手的路径 ==\n");
    {
        App app;
        Box* root = new Box(Axis::COLUMN);
        app.setRootView(root);

        // 假装平台有软键盘：记录请求，并模拟"用户输完点了确定"
        struct FakeIme : ImeManager
        {
            bool opened = false;
            ImeRequest last;
            bool open(const ImeRequest& request) override
            {
                this->opened = true;
                this->last   = request;
                return true;
            }
        };

        FakeIme fake;
        setImeManager(&fake);

        EditTextDialog* dialog = new EditTextDialog();
        dialog->setTitle("平台键盘");
        dialog->setText("旧值");
        dialog->setMaxLength(6);
        dialog->open();

        check(fake.opened, "平台 IME 被调用");
        checkEqual(fake.last.header, "平台键盘", "标题传给了键盘");
        checkEqual(fake.last.initialText, "旧值", "初始文本传给了键盘");
        checkEqualSize((size_t)fake.last.maxLength, 6, "长度上限传给了键盘");

        // 平台回调：用户确定，输入了"新文本"
        fake.last.callback(true, "新文本");
        checkEqual(dialog->getText(), "新文本", "键盘结果写回了弹窗");

        // 取消：保持原样
        fake.last.callback(false, "");
        checkEqual(dialog->getText(), "新文本", "键盘取消不改内容");

        setImeManager(nullptr); // 还原成默认
        check(!hasPlatformIme(), "还原后没有平台 IME");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
