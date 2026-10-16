#pragma once

// 全局变量管理器：**进程级状态的唯一登记处**。
//
// 为什么要有它（评审 §4.2 P0②）：库里原来散着若干"进程级单例"—— `EventBus`、
// `InputManager`、纹理仓库、几个注册表、当前 `App` —— 各写各的函数内 `static`。
// 它们有三个共性问题：
//   ① 列举不出来：没人知道这台进程里到底有多少份全局状态、分别是谁的；
//   ② 重置不了：跑完一轮测试、或关掉一个 `App` 之后状态还在（订阅、焦点、纹理、
//      注册过的类型全留着），于是"第二个 App"永远不干净；
//   ③ 交代不清：宿主（模拟器、编辑器）想知道"你往我这台进程里放了什么"，只能读代码。
//
// 这个类把它们收进一张表：
//   · **对象槽**：`obtain<T>(name)` 懒创建（由 store 拥有）/ `set<T>(name, ptr, owned)`
//     放借来的指针（App、平台、后端这类别人拥有的东西）
//   · **数值槽**：`setNumber / setFlag / setText` —— 放零散的全局开关与调参；
//     宿主自己的全局变量也可以放这儿，省一个没人管的 `static`
//   · **重置钩子**：子系统自己的静态表（纹理仓库的映射、注册表的函数表）搬不进表里，
//     就登记一个钩子，`resetAll()` 按**后进先出**调用
//
// 三条约定：
// ① **新增进程级状态一律登记到这里**，并保证 `resetAll()` 之后还能重新懒创建。
//    不要再新写 `static X instance;` 这种"拿不到、清不掉"的东西。
// ② 名字用短 camelCase（`eventBus` / `inputManager` / `app`），全库唯一；
//    同一个名字先当 A 用、后当 B 用是编程错误：返回 nullptr + 一条 error，不崩。
// ③ 只在**主线程**访问（库是"一个进程一套界面"，有全局 ImGui 上下文）。
//    这不是"多实例"方案，它解决的是可列举 / 可重置 / 可交代这三件事。
//
// 用法：
//     auto& bus = *bklv::globals::getGlobalStore().obtain<bklv::EventBus>("eventBus");
//     bklv::globals::getGlobalStore().setNumber("myApp.uiScale", 1.5);
//     bklv::globals::resetGlobals();   // 一轮结束（测试之间 / 换 App 之前）

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace bklv::globals
{

// 每个类型一个唯一地址（不依赖 RTTI：Switch 构建带 -fno-rtti）。
// inline 变量模板保证跨翻译单元是同一个地址；类型不符时地址也对不上。
template <typename T>
inline const char kTypeTag = 0;

template <typename T>
const void* typeTag()
{
    return &kTypeTag<T>;
}

// 人类可读的类型名（给 describe() / 报错用）。同样不依赖 RTTI：读编译器宏。
template <typename T>
const char* typeName()
{
#if defined(_MSC_VER)
    const char* pretty = __FUNCSIG__; // "...typeName<struct bklv::EventBus>(void)"
    const char* open   = std::strstr(pretty, "typeName<");
    const char* start  = open != nullptr ? open + 9 : nullptr;
#else
    const char* pretty = __PRETTY_FUNCTION__; // "const char* ... [with T = bklv::EventBus]"
    const char* open   = std::strstr(pretty, "T = ");
    const char* start  = open != nullptr ? open + 4 : nullptr;
#endif
    (void)pretty;

    // 截到 "]" / ">" / ";" / "," 为止（Clang 与 GCC 的收尾符号不一样）
    static thread_local char buffer[128];
    if (start == nullptr)
        return "?";
    size_t length = 0;
    while (start[length] != '\0' && length + 1 < sizeof(buffer))
    {
        const char c = start[length];
        if (c == ']' || c == '>' || c == ';' || c == ',' || c == ' ')
            break;
        buffer[length] = c;
        length++;
    }
    buffer[length] = '\0';

    // 匿名命名空间里的类型会带 "(anonymous namespace)::" 前缀，读日志时是噪音
    static thread_local char cleaned[128];
    const char* noise = "(anonymous namespace)::";
    const size_t noiseLength = std::strlen(noise);
    if (std::strncmp(buffer, noise, noiseLength) == 0)
    {
        std::snprintf(cleaned, sizeof(cleaned), "%s", buffer + noiseLength);
        return cleaned;
    }
    return buffer;
}

class GlobalStore
{
  public:
    GlobalStore();
    ~GlobalStore();

    GlobalStore(const GlobalStore&) = delete;
    GlobalStore& operator=(const GlobalStore&) = delete;

    // ---- 对象槽 ----

    // 取一个对象；没有就默认构造一个（由 store 拥有，reset 时析构）。
    // 名字已被别的类型占用时返回 nullptr + 一条 error。
    template <typename T>
    T* obtain(const char* name)
    {
        return static_cast<T*>(this->slotObtain(name, typeTag<T>(), typeName<T>(),
            []() -> void* { return new T(); }, [](void* pointer) { delete static_cast<T*>(pointer); }));
    }

    // 只取不建：没有、或类型不符都返回 nullptr
    template <typename T>
    T* find(const char* name) const
    {
        return static_cast<T*>(this->slotFind(name, typeTag<T>()));
    }

    // 放一个指针。owned=true 表示"store 负责 delete"（必须是 new 出来的）；
    // 借用指针（App / 平台 / 图形后端）用 owned=false，谁借的谁保证它活到最后。
    template <typename T>
    void set(const char* name, T* value, bool owned = false)
    {
        this->slotSet(name, typeTag<T>(), typeName<T>(), value,
            owned ? [](void* pointer) { delete static_cast<T*>(pointer); } : nullptr);
    }

    // 这个槽在不在（不管类型）
    bool has(const char* name) const;

    // 摘掉一个槽：拥有者会被析构，借用指针只丢引用；同名数值槽一并清掉
    void reset(const char* name);

    // ---- 重置钩子（子系统自己的静态表）----

    // 重复登记同一个名字只保留第一次（各注册入口随手调，不用自己判重）。
    // resetAll() 按登记顺序的**逆序**调用（依赖者先复位）。
    void addResetHook(const char* name, void (*hook)());
    size_t getResetHookCount() const;

    // ---- 数值槽 ----

    void setNumber(const char* name, double value);
    double getNumber(const char* name, double fallback = 0.0) const;
    void setFlag(const char* name, bool value);
    bool getFlag(const char* name, bool fallback = false) const;
    void setText(const char* name, const std::string& value);
    std::string getText(const char* name, const std::string& fallback = std::string()) const;

    // ---- 全清 ----

    // 先跑重置钩子（逆序），再按登记顺序析构"拥有"的对象，最后清空两张表。
    void resetAll();

    // ---- 调试 / 测试 ----

    size_t getObjectCount() const;
    size_t getValueCount() const;
    // 每行一条，例如：
    //   对象  eventBus  bklv::EventBus（本 store 拥有）
    //   数值  myApp.uiScale = 1.5
    //   钩子  textureStore
    std::vector<std::string> describe() const;

  private:
    // 模板都走这三个类型擦除入口（真正的存储在 .cpp 的 Impl 里）
    void* slotFind(const char* name, const void* tag) const;
    void* slotObtain(const char* name, const void* tag, const char* typeLabel,
        void* (*create)(), void (*destroy)(void*));
    void slotSet(const char* name, const void* tag, const char* typeLabel, void* value,
        void (*destroy)(void*));

    struct Impl;
    Impl* impl = nullptr;
};

// 全局唯一的那张表（它自己是函数内 static —— 表本身没法登记进表里）
GlobalStore& getGlobalStore();

// 一轮结束时的标准收尾：等价于 getGlobalStore().resetAll()
void resetGlobals();

} // namespace bklv::globals
