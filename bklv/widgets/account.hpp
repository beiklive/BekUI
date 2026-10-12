#pragma once

// 账号信息（头像 + 昵称），对应 borealis 的 views/widgets/account。
//
// 与 borealis 的差别（都是为了桌面可用 + 能测）：
// - borealis 的 `AccountWidget` 是 **Switch 专用**：直接调 account 服务取头像图，
//   没有文字、桌面平台拿到的是空控件。
// - 这里做成"可插拔数据源 + 无图也能看"：
//     · 有头像图（本地文件 / 平台给的数据）就显示图；
//     · 没有图就画一个圆形色块 + 昵称首字（桌面也能看出是谁）；
//     · 昵称与副标题都有（borealis 只显示头像）。
//   Switch 上想接 account 服务，setProvider 注入即可（本仓库不直接依赖该服务）。

#include <functional>
#include <string>

#include "bklv/views/label.hpp"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{

struct AccountInfo
{
    std::string name;       // 昵称（首字会画在头像上）
    std::string subtitle;   // 副标题（可以是好友码 / 状态，空则不显示）
    std::string avatarPath; // 头像图片路径（相对资源目录；空则用首字圆形头像）
};

class AccountWidget : public AppearanceView
{
  public:
    AccountWidget();

    void setAccount(const AccountInfo& value);
    const AccountInfo& getAccount() const { return this->info; }

    // 换成自己的数据源（Switch 上读 account 服务、桌面上读配置都行）
    void setProvider(std::function<AccountInfo()> value);
    void refresh();

    void setName(const std::string& value);
    void setSubtitle(const std::string& value);
    void setAvatar(const std::string& path);

    // 测试用
    const std::string& getName() const { return this->info.name; }
    const std::string& getInitial() const { return this->initial; }
    bool hasAvatarImage() const;

    void applyTheme() override;

    std::string getClassString() const override { return "bklv::AccountWidget"; }

  protected:
    // 外观里的文字层可以直接用 source="initial" 取昵称首字
    std::string getLayerText(const std::string& source) override;

  private:
    void refreshLabels();

    Label* nameLabel     = nullptr;
    Label* subtitleLabel = nullptr;
    class Image* avatar  = nullptr;

    AccountInfo info;
    std::string initial; // 昵称的第一个字符（按 UTF-8 取）
    std::function<AccountInfo()> provider;
    ImU32 avatarColor = 0;
    ImU32 initialColor = 0;
};

} // namespace bklv
