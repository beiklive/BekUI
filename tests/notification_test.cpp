// 无窗口测试：通知（Toast）。
//
// 对标 borealis 的 NotificationManager，但它是普通视图而不是全局单例：
// 这里验证排队、上限淘汰、到期自动消失与清理。

#include <cstdio>
#include <string>

#include "bklv/core/i18n.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/notification.hpp"

using namespace bklv;

#include "test_util.hpp"

int main()
{
    bklv::initResources();
    initI18n();
    setLocale("zh-Hans");

    std::printf("== 1. 弹一条 ==\n");
    {
        NotificationManager manager;
        checkEqualSize(manager.getCount(), 0, "初始没有通知");

        Notification* item = manager.notify("你好");
        check(item != nullptr, "notify 返回了条目");
        checkEqualSize(manager.getCount(), 1, "管理器里有一条");
        check(item->getText() == "你好", "文案正确");
        check(item->getActiveAppearance().indexOfLayer("bg") >= 0, "外观层来自 JSON");
        check(!manager.isFocusable(), "通知浮层不吃焦点");
    }

    std::printf("\n== 2. 上限淘汰最旧的 ==\n");
    {
        NotificationManager manager(2);
        Notification* first  = manager.notify("第一条");
        Notification* second = manager.notify("第二条");
        check(!first->isDismissed(), "两条都还在");

        Notification* third = manager.notify("第三条");
        checkEqualSize(manager.getCount(), 3, "第三条加进来了");
        check(first->isDismissed(), "超上限后最旧的先消失");
        check(!second->isDismissed() && !third->isDismissed(), "后两条不受影响");
    }

    std::printf("\n== 3. 到期自动消失 ==\n");
    {
        Notification* item = new Notification("短暂", 0.05f);
        check(!item->isDismissed(), "刚开始不消失");
        checkFloat(item->getRemaining(), 0.05f, "剩余时间 = 时长");

        item->update(); // 一帧（帧间隔至少几毫秒）
        check(item->getRemaining() < 0.05f, "剩余时间在减少");

        for (int i = 0; i < 600 && !item->isDismissed(); i++)
            item->update();
        check(item->isDismissed(), "时间到就标记消失");

        // 手动消失也幂等
        item->dismiss();
        check(item->isDismissed(), "重复 dismiss 安全");
        delete item;
    }

    std::printf("\n== 4. 清理已消失的条目 ==\n");
    {
        NotificationManager manager(5);
        Notification* item = manager.notify("马上消失", 0.0f);
        item->dismiss();
        checkEqualSize(manager.getCount(), 1, "先还在列表里（等动效）");

        // 反复 update：动效播完后应该被摘掉
        for (int i = 0; i < 200 && manager.getCount() > 0; i++)
            manager.update();
        checkEqualSize(manager.getCount(), 0, "动效结束后从管理器移除");
    }

    std::printf("\n== 5. 样式来自主题 ==\n");
    {
        check(getTheme().color("color.notification.background", 0) != 0, "通知底色在主题里");
        checkFloat(getTheme().number("metric.notification.width", 0.0f), 360.0f, "通知宽度来自主题");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
