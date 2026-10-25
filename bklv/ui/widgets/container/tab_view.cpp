#include "bklv/ui/widgets/container/tab_view.hpp"

#include <cstdio>
#include <utility>

#include "bklv/ui/input/input.hpp"
#include "bklv/ui/view/box.hpp"
#include "bklv/ui/view/view.hpp"

using namespace bklv;

TabView::TabView(const std::string& variant)
    : AppearanceView(variant.empty() ? "base/tab/tabview" : "base/tab/tabview." + variant, false)
{
    // 页面在同一个槽里叠着（只显示一个），所以是一列：页面自己 setGrow(1) 就铺满。
    this->setAxis(Axis::COLUMN);
    this->applyTheme();
}

TabView::~TabView() = default;

View* TabView::findRestorable(View* view)
{
    if (view == nullptr)
        return nullptr;
    if (view->hasRestorableState())
        return view;

    Box* box = view->asBox();
    if (box == nullptr)
        return nullptr;
    for (View* child : box->getChildren())
    {
        if (View* found = TabView::findRestorable(child))
            return found;
    }
    return nullptr;
}

bool TabView::isInside(View* root, View* view)
{
    for (View* node = view; node != nullptr; node = node->getParent())
    {
        if (node == root)
            return true;
    }
    return false;
}

void TabView::giveFocusTo(View* view)
{
    if (view == nullptr)
        return;

    View* target = view->isFocusable() ? view : view->getDefaultFocus();
    if (target == nullptr)
        return;
    getInputManager().giveFocus(target);
}

std::string TabView::memoryKey(const Entry& entry) const
{
    if (!entry.id.empty())
        return "tab:" + entry.id;
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "tab:@%llx", (unsigned long long)(uintptr_t)entry.page);
    return std::string(buffer);
}

int TabView::addTab(const std::string& id, View* page)
{
    if (page == nullptr)
        return -1;

    Entry entry;
    entry.id   = id;
    entry.page = page;

    // 页面占满宿主的内容槽
    if (page->getGrow() <= 0.0f)
        page->setGrow(1.0f);

    this->addView(page);
    page->setVisibility(Visibility::GONE);

    this->tabs.push_back(entry);
    const int index = (int)this->tabs.size() - 1;

    if (this->active < 0)
        this->setActiveTab(index, false);

    return index;
}

void TabView::rememberCurrent()
{
    if (this->active < 0 || this->active >= (int)this->tabs.size())
        return;

    Entry& entry = this->tabs[(size_t)this->active];
    if (entry.page == nullptr)
        return;

    View* focus = getInputManager().getCurrentFocus();
    entry.hadFocus = focus != nullptr && TabView::isInside(entry.page, focus);

    const std::string key = this->memoryKey(entry);
    if (entry.hadFocus)
        getInputManager().rememberFor(key);

    entry.hasScroll = false;
    if (View* restorable = TabView::findRestorable(entry.page))
    {
        entry.hasScroll = true;
        entry.scroll   = restorable->getRestorableState();
    }
}

void TabView::restoreCurrent()
{
    if (this->active < 0 || this->active >= (int)this->tabs.size())
        return;

    Entry& entry = this->tabs[(size_t)this->active];
    if (entry.page == nullptr)
        return;

    // 只有"上次焦点确实在这个页面里"才恢复焦点；否则保持当前焦点不动
    // （比如用 A 键激活标签时，焦点本来就该留在标签条上）。
    if (entry.hadFocus)
    {
        TabView::giveFocusTo(entry.page);
        getInputManager().loadFocusFor(this->memoryKey(entry));
    }

    if (entry.hasScroll)
    {
        if (View* restorable = TabView::findRestorable(entry.page))
            restorable->setRestorableState(entry.scroll);
    }
}

void TabView::setActiveTab(int index, bool animated)
{
    (void)animated; // 转场下一版接 ViewTransition（和 ContentStack 一样的配方）

    if (index < 0 || index >= (int)this->tabs.size())
        return;
    if (index == this->active)
        return;

    this->rememberCurrent();

    if (this->active >= 0 && this->active < (int)this->tabs.size())
    {
        if (View* previous = this->tabs[(size_t)this->active].page)
            previous->setVisibility(Visibility::GONE);
    }

    this->active = index;

    Entry& entry = this->tabs[(size_t)index];
    if (entry.page != nullptr)
        entry.page->setVisibility(Visibility::VISIBLE);

    this->restoreCurrent();
    this->invalidate();

    if (this->onTabChanged)
        this->onTabChanged(index, entry.page);
}

void TabView::setActiveTabId(const std::string& id, bool animated)
{
    for (size_t i = 0; i < this->tabs.size(); i++)
    {
        if (this->tabs[i].id == id)
        {
            this->setActiveTab((int)i, animated);
            return;
        }
    }
}

View* TabView::getActivePage() const
{
    return this->getPage(this->active);
}

View* TabView::getPage(int index) const
{
    if (index < 0 || index >= (int)this->tabs.size())
        return nullptr;
    return this->tabs[(size_t)index].page;
}
