// 无窗口测试：账号控件（头像 + 昵称 + 可插拔数据源）。
//
// 对标 borealis 的 views/widgets/account（Switch 专用、只显示头像）：
// 这里无图时用"圆形色块 + 昵称首字"，并且数据源可注入，所以桌面也能用、也能测。

#include <cstdio>
#include <string>

#include "bklv/core/i18n.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/account.hpp"

using namespace bklv;

#include "test_util.hpp"

int main()
{
    bklv::initResources();
    initI18n();
    setLocale("zh-Hans");
    setTheme("default");

    std::printf("== 1. 昵称与首字 ==\n");
    {
        AccountWidget account;
        account.setName("张三");
        checkEqual(account.getName(), "张三", "昵称");
        checkEqual(account.getInitial(), "张", "中文昵称取第一个**字符**（不是字节）");

        account.setName("Alice");
        checkEqual(account.getInitial(), "A", "英文昵称取首字母");

        account.setName("");
        checkEqual(account.getInitial(), "", "空昵称不崩");

        account.setName("🎮player");
        checkEqual(account.getInitial(), "🎮", "emoji 不会被切成半个字节");
    }

    std::printf("\n== 2. 副标题显隐 ==\n");
    {
        AccountWidget account;
        account.setSubtitle("");
        check(true, "空副标题不崩");

        account.setAccount({ "玩家一号", "SW-1234-5678", "" });
        checkEqual(account.getAccount().subtitle, "SW-1234-5678", "整份账号信息能一次设进去");
        check(!account.hasAvatarImage(), "没给头像图时没有贴图（走首字圆形）");
    }

    std::printf("\n== 3. 可插拔数据源 ==\n");
    {
        AccountWidget account;
        checkEqual(account.getName(), "", "默认是空的（不依赖任何平台服务）");

        account.setProvider([] { return AccountInfo{ "本地账号", "离线", "" }; });
        checkEqual(account.getName(), "本地账号", "provider 注入后立刻拿到昵称");
        checkEqual(account.getInitial(), "本", "首字跟着更新");

        // 换一个数据源再 refresh
        AccountInfo next{ "Switch 账号", "在线", "" };
        account.setProvider([&next] { return next; });
        next.name = "改过的名字";
        account.refresh();
        checkEqual(account.getName(), "改过的名字", "refresh 会重新取一次");
    }

    std::printf("\n== 4. 外观与主题 ==\n");
    {
        AccountWidget account;
        check(account.getActiveAppearance().indexOfLayer("avatar") >= 0, "头像层在 JSON 里");
        check(account.getActiveAppearance().indexOfLayer("initial") >= 0, "首字文字层在 JSON 里");
        check(getTheme().color("color.account.avatar", 0) != 0, "头像颜色在主题里");
        checkFloat(getTheme().number("metric.account.avatarSize", 0.0f), 32.0f,
            "头像尺寸来自主题");

        // 给一个不存在的头像路径：不能崩（异步加载失败会保持首字）
        account.setAvatar("resources/__nope__.png");
        check(true, "坏头像路径不崩");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
