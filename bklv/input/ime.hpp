#pragma once

// 文本输入（IME / 软键盘）。
//
// borealis 的 ImeManager 是"弹系统键盘，输完回调"这一种形态；这里保留同一个接口形状，
// 但明确区分两种输入源：
//
//   · **系统软键盘**（Switch 的 swkbd）：`open()` 返回 true，界面自己不用处理按键，
//     用户在键盘上输完，平台回调把结果给回来。
//   · **内联键盘输入**（桌面）：平台没有"弹软键盘"这回事，`open()` 返回 false，
//     由控件自己读键盘（ImGui 的字符队列 + Backspace/方向键）编辑文本。
//
// 控件（EditTextDialog）两种都能用：先问平台，平台不接手就自己编辑。

#include <functional>
#include <string>

namespace bklv
{

// 键盘上要禁掉哪些键（和 borealis 的 bitmask 对齐，Switch 会真的用上）
enum ImeDisableBits
{
    IME_DISABLE_NONE         = 0,
    IME_DISABLE_SPACE        = 1 << 0,
    IME_DISABLE_AT           = 1 << 1,
    IME_DISABLE_PERCENT      = 1 << 2,
    IME_DISABLE_FORWSLASH    = 1 << 3,
    IME_DISABLE_BACKSLASH    = 1 << 4,
    IME_DISABLE_NUMBERS      = 1 << 5,
    IME_DISABLE_DOWNLOADCODE = 1 << 6,
    IME_DISABLE_USERNAME     = 1 << 7,
};

struct ImeRequest
{
    std::string header;      // 键盘标题
    std::string subText;     // 键盘副标题（说明用途）
    std::string initialText; // 初始文本
    int maxLength      = 32;
    bool numeric       = false;
    int disableBits    = IME_DISABLE_NONE;

    // accepted=false 表示用户取消了
    std::function<void(bool accepted, const std::string& text)> callback;
};

class ImeManager
{
  public:
    virtual ~ImeManager() = default;

    // 返回 true 表示"这次输入交给平台了"（比如 Switch 弹出了软键盘），
    // 回调会在合适的时候被调；返回 false 表示平台不接手，控件自己编辑。
    virtual bool open(const ImeRequest& request) = 0;

    virtual bool isOpen() const { return false; }
    virtual void close() { }
};

// 全局 IME：默认是下面那个"平台不接手"的实现，平台可以替换掉。
ImeManager& getImeManager();
void setImeManager(ImeManager* manager);

// 默认实现：不弹软键盘，让控件自己用键盘输入（桌面的路径）。
class InlineImeManager : public ImeManager
{
  public:
    bool open(const ImeRequest&) override { return false; }
};

// 有没有平台提供的系统键盘（Switch 上是 true，桌面是 false）
bool hasPlatformIme();

} // namespace bklv
