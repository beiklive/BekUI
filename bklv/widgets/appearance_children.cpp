#include "bklv/widgets/appearance_children.hpp"

#include "bklv/core/box.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{
namespace appearance
{

namespace
{

void buildOne(Box& parent, const AppearanceChild& spec, int depth)
{
    if (depth > 16)
    {
        BKLV_LOG_WARNING("children 嵌套超过 16 层，后面的不再建");
        return;
    }

    switch (spec.kind)
    {
        case AppearanceChild::Kind::APPEARANCE:
        {
            auto child = std::make_unique<AppearanceView>(spec.name, false);
            // 只覆盖写了的字段（存在标记由 compile 侧设好，见 compile/appearance_children.cpp）
            applyAppearanceBox(child.get(), spec.box);
            if (!spec.text.empty())
                child->setText(spec.text);
            if (!spec.icon.empty())
                child->setIcon(spec.icon);
            if (!spec.info.empty())
                child->setInfoText(spec.info);

            AppearanceView* raw = child.get();
            parent.addView(child.release()); // addView 收裸指针：交给父节点后再 release（AGENTS 的 RAII 约定）
            for (const AppearanceChild& grand : spec.children)
                buildOne(*raw, grand, depth + 1);
            break;
        }
        case AppearanceChild::Kind::BOX:
        {
            auto child = std::make_unique<Box>(Axis::COLUMN);
            applyAppearanceBox(child.get(), spec.box);

            Box* raw = child.get();
            parent.addView(child.release()); // addView 收裸指针：交给父节点后再 release（AGENTS 的 RAII 约定）
            for (const AppearanceChild& grand : spec.children)
                buildOne(*raw, grand, depth + 1);
            break;
        }
        case AppearanceChild::Kind::VIEW_TEMPLATE:
            // 里程碑 3：复用主题里的视图模板（ViewTemplate::instantiate）
            BKLV_LOG_WARNING("children 的 ref \"%s\" 还没接上（里程碑 3），这一项先跳过",
                spec.name.c_str());
            break;
    }
}

} // namespace

void buildChildren(AppearanceView& parent, const std::vector<AppearanceChild>& spec)
{
    for (const AppearanceChild& child : spec)
        buildOne(parent, child, 1);
}

} // namespace appearance
} // namespace bklv
