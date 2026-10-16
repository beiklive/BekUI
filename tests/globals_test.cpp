// 无窗口测试：全局变量管理器（`core/globals.hpp`）。
//
// 它存在的理由（评审 §4.2 P0②）：库里的进程级状态原来散在各处的函数内 static ——
// 列举不出来、重置不掉。这里钉住四件事：
//   ① 对象槽的语义：懒创建、同名字同对象、类型不符返回 nullptr 而不崩
//   ② 借用 vs 拥有：resetAll() 只析构"拥有"的那些
//   ③ 数值槽：number / flag / text，类型不符或不存在都回到调用方给的缺省值
//   ④ resetAll() 真的把子系统复位：EventBus 换新对象（旧订阅作废）、InputManager 焦点清空、
//      注册表清空 —— 而且**内置的表项不会因此静默丢失**（清空后重新注册，这是最容易写错的一条）

#include <cstdio>
#include <string>
#include <vector>

#include "bklv/bklv.hpp"

#include "bklv/compile/layer_registry.hpp"
#include "bklv/core/globals.hpp"
#include "bklv/views/data_source.hpp"
#include "bklv/views/item_binder.hpp"
#include "bklv/widgets/widget_types.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

// 计数器：验证"拥有"（store 负责 delete）与"借用"（store 只丢引用）的区别
struct Counted
{
    static int alive;
    Counted() { alive++; }
    Counted(const Counted&) = delete;
    Counted& operator=(const Counted&) = delete;
    ~Counted() { alive--; }
};
int Counted::alive = 0;

// 可聚焦的最小 Box（普通 Box 不可聚焦，giveFocus() 会被 canFocus() 挡掉）
class FocusBox : public Box
{
  public:
    bool isFocusable() const override { return true; }
    std::string getClassString() const override { return "FocusBox"; }
};

// 订阅测试用的事件类型
struct PingEvent
{
    int value = 0;
};

// 钩子顺序：resetAll() 按登记顺序的**逆序**调用
std::vector<std::string> g_hookOrder;

void hookA()
{
    g_hookOrder.push_back("A");
}

void hookB()
{
    g_hookOrder.push_back("B");
}

bool contains(const std::vector<std::string>& lines, const std::string& needle)
{
    for (const std::string& line : lines)
    {
        if (line.find(needle) != std::string::npos)
            return true;
    }
    return false;
}

} // namespace

int main()
{
    bklv::initResources();

    globals::GlobalStore& store = globals::getGlobalStore();

    std::printf("\n== 1. 对象槽：懒创建、同名字同对象、类型不符不崩 ==\n");
    {
        store.reset("test.object");
        Counted* first = store.obtain<Counted>("test.object");
        check(first != nullptr, "obtain 建出来了");
        checkEqualInt(Counted::alive, 1, "只建了一个");
        check(store.obtain<Counted>("test.object") == first, "同一个名字拿到同一个对象");
        checkEqualInt(Counted::alive, 1, "没有重复构造");
        check(store.find<Counted>("test.object") == first, "find 能拿到");
        check(store.find<Counted>("test.missing") == nullptr, "find 不创建");
        checkEqualInt(Counted::alive, 1, "find 之后个数没变");
        check(store.has("test.object"), "has 为真");

        // 同名换类型是编程错误：返回 nullptr，不崩、也不偷偷建第二个
        check(store.find<int>("test.object") == nullptr, "按别的类型 find -> nullptr");
        check(store.obtain<int>("test.object") == nullptr, "按别的类型 obtain -> nullptr");
        checkEqualInt(Counted::alive, 1, "类型不符不会建第二个对象");

        store.reset("test.object");
        checkEqualInt(Counted::alive, 0, "reset 会析构拥有的对象");
        check(!store.has("test.object"), "reset 之后槽没了");
    }

    std::printf("\n== 2. 借用 vs 拥有：resetAll 只析构自己拥有的 ==\n");
    {
        Counted borrowed;
        store.set<Counted>("test.borrowed", &borrowed, false);
        store.obtain<Counted>("test.owned");
        checkEqualInt(Counted::alive, 2, "一个借用 + 一个拥有");

        store.resetAll();
        checkEqualInt(Counted::alive, 1, "resetAll 只析构了拥有的那个（借用的还在栈上）");
        store.reset("test.borrowed");
        checkEqualInt(Counted::alive, 1, "摘掉借用槽也不会 delete 它不是它的东西");
    }

    std::printf("\n== 3. 数值槽：三种类型 + 缺省值 ==\n");
    {
        store.setNumber("test.number", 1.25);
        store.setFlag("test.flag", true);
        store.setText("test.text", "你好");

        checkFloat((float)store.getNumber("test.number"), 1.25f, "number 存取一致");
        check(store.getFlag("test.flag"), "flag 存取一致");
        checkEqual(store.getText("test.text"), "你好", "text 存取一致");

        checkFloat((float)store.getNumber("test.missing", 7.5), 7.5f, "缺的 number 用调用方的缺省值");
        check(!store.getFlag("test.missing", false), "缺的 flag 用缺省值");
        checkEqual(store.getText("test.missing", "兜底"), "兜底", "缺的 text 用缺省值");
        // 类型不符也回到缺省值（不抛、不转换）
        checkFloat((float)store.getNumber("test.text", 3.0), 3.0f, "按 number 读一个 text 槽 -> 缺省值");
        check(!store.getFlag("test.number", false), "按 flag 读一个 number 槽 -> 缺省值");

        store.reset("test.number");
        checkFloat((float)store.getNumber("test.number", -1.0), -1.0f, "reset 连数值槽一起清");
    }

    std::printf("\n== 4. 重置钩子：后进先出，重复登记只留一次 ==\n");
    {
        g_hookOrder.clear();
        store.addResetHook("test.hookA", &hookA);
        store.addResetHook("test.hookB", &hookB);
        store.addResetHook("test.hookA", &hookA); // 重复：只留第一次
        checkEqualInt((int)store.getResetHookCount(), 2, "重复登记同一个名字只留一次");

        store.resetAll();
        checkEqualInt((int)g_hookOrder.size(), 2, "两个钩子各跑一次");
        if (g_hookOrder.size() == 2)
        {
            checkEqual(g_hookOrder[0], "B", "后登记的钩子先跑（依赖者先复位）");
            checkEqual(g_hookOrder[1], "A", "先登记的后跑");
        }
        checkEqualInt((int)store.getResetHookCount(), 0, "钩子随重置一起清空（下次用到会重新登记）");
    }

    std::printf("\n== 5. describe()：能一眼看出表里有什么 ==\n");
    {
        store.obtain<Counted>("test.owned");
        Counted borrowed;
        store.set<Counted>("test.borrowed", &borrowed, false);
        store.setNumber("test.number", 2.5);
        store.setFlag("test.onlyFlag", false);
        store.setText("test.text", "abc");
        store.addResetHook("test.hookA", &hookA);

        const std::vector<std::string> lines = store.describe();
        check(contains(lines, "test.owned") && contains(lines, "拥有"), "拥有对象标了\"拥有\"");
        check(contains(lines, "test.borrowed") && contains(lines, "借用"), "借用指针标了\"借用\"");
        check(contains(lines, "test.number = 2.5"), "数值按类型打印");
        check(contains(lines, "test.onlyFlag = false"), "flag 打成 true/false");
        check(contains(lines, "test.text = \"abc\""), "text 带引号");
        check(contains(lines, "test.hookA"), "钩子也列出来");

        store.resetAll();
    }

    std::printf("\n== 6. 真实子系统：resetAll 之后是干净的一轮 ==\n");
    {
        // EventBus：订阅随旧对象一起作废。
        // 句柄刻意 new 出来不 delete：重置会把总线整个换掉，栈上的 Subscription 析构时
        // 会去碰已经不存在的总线（这正是"重置之前先退订"的现实约束）。测试里漏一个句柄即可。
        Subscription* handle = new Subscription(
            getEventBus().subscribe<PingEvent>([](const PingEvent&) {}));
        check(getEventBus().getTotalSubscriberCount() > 0, "先订阅一条");

        globals::resetGlobals();

        // 注意：不能断言"地址变了" —— 重新 new 出来的对象完全可能复用同一个地址。
        // 有意义的断言是"这是新的一轮状态"：订阅清零，而且它已经登记回表里。
        EventBus& after = getEventBus();
        checkEqualInt((int)after.getTotalSubscriberCount(), 0, "旧订阅跟着旧对象一起作废");
        check(globals::getGlobalStore().find<EventBus>("eventBus") == &after,
            "重置之后 getEventBus() 重新懒创建，并登记回全局表");
        (void)handle;
    }
    {
        // InputManager：焦点清空（它握着指向视图的指针，重置必须发生在视图销毁之前）
        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(200.0f, 200.0f);
        FocusBox* child = new FocusBox();
        child->setDimensions(50.0f, 50.0f);
        root->addView(child);

        getInputManager().setRoot(root);
        getInputManager().giveFocus(child);
        check(getInputManager().getCurrentFocus() == child, "先把焦点放在 child 上");

        globals::resetGlobals();
        check(getInputManager().getCurrentFocus() == nullptr, "重置之后焦点清空（新对象）");

        // 收尾：视图还活着，先把输入层摘干净再删
        getInputManager().setRoot(nullptr);
        getInputManager().resetFocusState();
        delete root;
    }
    {
        // 注册表：清空后必须能重新注册，而且内置的表项不能因此静默丢失
        check(compile::registeredLayers().size() >= 6, "层注册表一开始有内置的 6 种 op");

        std::string error;
        const bool sourceAdded = data::registerSource(
            "globals_test_source", [] { return (RecyclerDataSource*)nullptr; }, error);
        check(sourceAdded, "注册一个数据源");
        const bool binderAdded = binding::registerItemBinder(
            "globals_test_binder", [](View&, size_t, size_t) {}, error);
        check(binderAdded, "注册一个 item 绑定函数");

        globals::resetGlobals();

        check(!data::hasSource("globals_test_source"), "重置后数据源表清空");
        check(!binding::hasItemBinder("globals_test_binder"), "重置后绑定函数表清空");
        check(compile::registeredLayers().size() >= 6,
            "重置后内置 op 自动回来了（只清表不翻标志的话这里会静默少掉）");
        check(isViewTypeRegistered("Box"), "内置视图类型也自动回来了");

        // 清空之后还能重新注册控件那一批（注册标志也要跟着复位）
        registerWidgetViewTypes();
        check(isViewTypeRegistered("Button"), "重新注册控件视图类型成功（没有卡在「已注册」标志上）");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
