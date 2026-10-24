#include "bklv/foundation/core/globals.hpp"

#include <map>
#include <sstream>
#include <utility>

#include "bklv/foundation/core/logger.hpp"

namespace bklv::globals
{

namespace
{

// 数值槽：一个 double / 一个 bool / 一段文本，用 kind 区分
struct Value
{
    enum class Kind
    {
        NUMBER,
        FLAG,
        TEXT,
    };

    Kind kind     = Kind::NUMBER;
    double number = 0.0;
    bool flag     = false;
    std::string text;
};

std::string numberToText(double value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

} // namespace

struct GlobalStore::Impl
{
    // 对象槽。destroy 非空 = 本 store 拥有（reset 时调用）；借用指针只有 name/tag。
    struct ObjectEntry
    {
        std::string name;
        std::string typeLabel;
        const void* tag = nullptr;
        void* pointer   = nullptr;
        void (*destroy)(void*) = nullptr;
    };

    struct HookEntry
    {
        std::string name;
        void (*hook)() = nullptr;
    };

    std::vector<ObjectEntry> objects;
    std::vector<HookEntry> hooks;
    std::map<std::string, Value> values;

    ObjectEntry* findObject(const char* name)
    {
        for (ObjectEntry& entry : this->objects)
        {
            if (entry.name == name)
                return &entry;
        }
        return nullptr;
    }

    const ObjectEntry* findObject(const char* name) const
    {
        for (const ObjectEntry& entry : this->objects)
        {
            if (entry.name == name)
                return &entry;
        }
        return nullptr;
    }

    // 类型不符是编程错误：报一条 error 并放弃这次操作（不做隐式转换、不崩）
    bool checkType(const ObjectEntry& entry, const void* tag, const char* typeLabel,
        const char* action) const
    {
        if (entry.tag == tag)
            return true;

        Logger::error(std::string("[全局] 槽 \"") + entry.name + "\" 的类型不符：已登记 "
            + entry.typeLabel + "，这次按 " + (typeLabel != nullptr ? typeLabel : "?")
            + " 来" + action + "。同名槽不许换类型（见 core/globals.hpp 约定 ②）。");
        return false;
    }
};

GlobalStore::GlobalStore()
    : impl(new Impl())
{
}

GlobalStore::~GlobalStore()
{
    // 进程退出时**不做清理**（不清钩子、不 delete 拥有对象），只把表本身放掉。
    //
    // 原因：这张表比其它函数内 static 先构造，所以最后析构 —— 到那一刻，别的 TU 里
    // 的静态（注册表的 map、纹理表、各子系统的容器）可能已经先销毁了。此时跑重置钩子
    // 或 delete 拥有对象都会踩到已析构对象（踩过：json_test 退出时 abort，检查项却是全绿的）。
    // 想让状态干净收尾，就在一切还活着时**显式**调 resetGlobals()：
    // 测试之间、换一个 App 之前都该这么做（见 core/globals.hpp 的用法示例）。
    delete this->impl;
    this->impl = nullptr;
}

// ---------------------------------------------------------------------------
// 对象槽
// ---------------------------------------------------------------------------

void* GlobalStore::slotFind(const char* name, const void* tag) const
{
    if (name == nullptr || this->impl == nullptr)
        return nullptr;

    const Impl::ObjectEntry* entry = this->impl->findObject(name);
    if (entry == nullptr)
        return nullptr;
    if (entry->tag != tag)
    {
        this->impl->checkType(*entry, tag, nullptr, "取用");
        return nullptr;
    }
    return entry->pointer;
}

void* GlobalStore::slotObtain(const char* name, const void* tag, const char* typeLabel,
    void* (*create)(), void (*destroy)(void*))
{
    if (name == nullptr || create == nullptr || this->impl == nullptr)
        return nullptr;

    if (Impl::ObjectEntry* entry = this->impl->findObject(name))
    {
        if (!this->impl->checkType(*entry, tag, typeLabel, "取用"))
            return nullptr;
        return entry->pointer;
    }

    Impl::ObjectEntry entry;
    entry.name      = name;
    entry.typeLabel = typeLabel != nullptr ? typeLabel : "?";
    entry.tag       = tag;
    entry.pointer   = create();
    entry.destroy   = destroy; // 懒创建的对象由本 store 拥有
    this->impl->objects.push_back(std::move(entry));
    return this->impl->objects.back().pointer;
}

void GlobalStore::slotSet(const char* name, const void* tag, const char* typeLabel, void* value,
    void (*destroy)(void*))
{
    if (name == nullptr || this->impl == nullptr)
        return;

    if (Impl::ObjectEntry* entry = this->impl->findObject(name))
    {
        if (!this->impl->checkType(*entry, tag, typeLabel, "覆盖"))
            return;
        // 换主人：旧值是我们拥有的就先放掉
        if (entry->destroy != nullptr && entry->pointer != value)
            entry->destroy(entry->pointer);
        entry->pointer = value;
        entry->destroy = destroy;
        return;
    }

    Impl::ObjectEntry entry;
    entry.name      = name;
    entry.typeLabel = typeLabel != nullptr ? typeLabel : "?";
    entry.tag       = tag;
    entry.pointer   = value;
    entry.destroy   = destroy;
    this->impl->objects.push_back(std::move(entry));
}

bool GlobalStore::has(const char* name) const
{
    if (name == nullptr || this->impl == nullptr)
        return false;
    if (this->impl->findObject(name) != nullptr)
        return true;
    return this->impl->values.count(name) > 0;
}

void GlobalStore::reset(const char* name)
{
    if (name == nullptr || this->impl == nullptr)
        return;

    for (size_t i = 0; i < this->impl->objects.size(); i++)
    {
        Impl::ObjectEntry& entry = this->impl->objects[i];
        if (entry.name != name)
            continue;
        if (entry.destroy != nullptr && entry.pointer != nullptr)
            entry.destroy(entry.pointer);
        this->impl->objects.erase(this->impl->objects.begin() + (long)i);
        break;
    }
    this->impl->values.erase(name);
}

// ---------------------------------------------------------------------------
// 重置钩子
// ---------------------------------------------------------------------------

void GlobalStore::addResetHook(const char* name, void (*hook)())
{
    if (name == nullptr || hook == nullptr || this->impl == nullptr)
        return;

    for (const Impl::HookEntry& entry : this->impl->hooks)
    {
        if (entry.name == name)
            return; // 重复登记只留第一次（各注册入口随手调）
    }

    Impl::HookEntry entry;
    entry.name = name;
    entry.hook = hook;
    this->impl->hooks.push_back(std::move(entry));
}

size_t GlobalStore::getResetHookCount() const
{
    return this->impl != nullptr ? this->impl->hooks.size() : 0;
}

// ---------------------------------------------------------------------------
// 数值槽
// ---------------------------------------------------------------------------

void GlobalStore::setNumber(const char* name, double value)
{
    if (name == nullptr || this->impl == nullptr)
        return;
    Value& slot  = this->impl->values[name];
    slot.kind    = Value::Kind::NUMBER;
    slot.number  = value;
}

double GlobalStore::getNumber(const char* name, double fallback) const
{
    if (name == nullptr || this->impl == nullptr)
        return fallback;
    const auto found = this->impl->values.find(name);
    if (found == this->impl->values.end() || found->second.kind != Value::Kind::NUMBER)
        return fallback;
    return found->second.number;
}

void GlobalStore::setFlag(const char* name, bool value)
{
    if (name == nullptr || this->impl == nullptr)
        return;
    Value& slot = this->impl->values[name];
    slot.kind   = Value::Kind::FLAG;
    slot.flag   = value;
}

bool GlobalStore::getFlag(const char* name, bool fallback) const
{
    if (name == nullptr || this->impl == nullptr)
        return fallback;
    const auto found = this->impl->values.find(name);
    if (found == this->impl->values.end() || found->second.kind != Value::Kind::FLAG)
        return fallback;
    return found->second.flag;
}

void GlobalStore::setText(const char* name, const std::string& value)
{
    if (name == nullptr || this->impl == nullptr)
        return;
    Value& slot = this->impl->values[name];
    slot.kind   = Value::Kind::TEXT;
    slot.text   = value;
}

std::string GlobalStore::getText(const char* name, const std::string& fallback) const
{
    if (name == nullptr || this->impl == nullptr)
        return fallback;
    const auto found = this->impl->values.find(name);
    if (found == this->impl->values.end() || found->second.kind != Value::Kind::TEXT)
        return fallback;
    return found->second.text;
}

// ---------------------------------------------------------------------------
// 全清
// ---------------------------------------------------------------------------

void GlobalStore::resetAll()
{
    if (this->impl == nullptr)
        return;

    // ① 钩子逆序（依赖者先复位）：纹理仓库、注册表的静态表都在这条路上清掉
    for (size_t i = this->impl->hooks.size(); i > 0; i--)
    {
        void (*hook)() = this->impl->hooks[i - 1].hook;
        if (hook != nullptr)
            hook();
    }
    this->impl->hooks.clear();

    // ② 拥有对象按登记顺序析构（顺序确定，便于复现）；借用指针只丢引用
    for (Impl::ObjectEntry& entry : this->impl->objects)
    {
        if (entry.destroy != nullptr && entry.pointer != nullptr)
            entry.destroy(entry.pointer);
        entry.pointer = nullptr;
    }
    this->impl->objects.clear();

    // ③ 数值槽
    this->impl->values.clear();
}

size_t GlobalStore::getObjectCount() const
{
    return this->impl != nullptr ? this->impl->objects.size() : 0;
}

size_t GlobalStore::getValueCount() const
{
    return this->impl != nullptr ? this->impl->values.size() : 0;
}

std::vector<std::string> GlobalStore::describe() const
{
    std::vector<std::string> lines;
    if (this->impl == nullptr)
        return lines;

    for (const Impl::ObjectEntry& entry : this->impl->objects)
    {
        lines.push_back("对象  " + entry.name + "  " + entry.typeLabel
            + (entry.destroy != nullptr ? "（本 store 拥有）" : "（借用）"));
    }
    for (const auto& pair : this->impl->values)
    {
        const Value& value = pair.second;
        switch (value.kind)
        {
            case Value::Kind::NUMBER:
                lines.push_back("数值  " + pair.first + " = " + numberToText(value.number));
                break;
            case Value::Kind::FLAG:
                lines.push_back("数值  " + pair.first + " = " + (value.flag ? "true" : "false"));
                break;
            case Value::Kind::TEXT:
                lines.push_back("数值  " + pair.first + " = \"" + value.text + "\"");
                break;
        }
    }
    for (const Impl::HookEntry& entry : this->impl->hooks)
        lines.push_back("钩子  " + entry.name);

    return lines;
}

GlobalStore& getGlobalStore()
{
    // 表本身是函数内 static：它是"最后一份"全局状态，没法再登记进自己
    static GlobalStore store;
    return store;
}

void resetGlobals()
{
    getGlobalStore().resetAll();
}

} // namespace bklv::globals
