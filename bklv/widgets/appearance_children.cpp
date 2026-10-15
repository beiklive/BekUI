#include "bklv/widgets/appearance_children.hpp"

#include "bklv/core/box.hpp"
#include "bklv/core/icon_font.hpp" // 内容槽的 @icon/名字 -> 字形
#include "bklv/core/json_view.hpp" // ref: 视图模板

#include <algorithm>
#include "bklv/core/logger.hpp"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{
namespace appearance
{

namespace
{

// 正在构建的外观名链（同一个外观在自己子树里又出现 = 环，见规格 §12.5 规则 6）
std::vector<std::string>& buildingStack()
{
    static std::vector<std::string> stack;
    return stack;
}

// 出作用域必定出栈（用 RAII 而不是手写 pop：手写时插错位置=只在出环那条路径才出栈，
// 结果同名兄弟被误判成循环引用 —— 这个坑我踩过）
struct BuildingGuard
{
    ~BuildingGuard() { buildingStack().pop_back(); }
};

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
            // 环检测：A 的子树里又引用 A（或 A->B->A）时，硬停下来并报错
            std::vector<std::string>& stack = buildingStack();
            if (std::find(stack.begin(), stack.end(), spec.name) != stack.end())
            {
                BKLV_LOG_ERROR("children 里出现循环引用：%s 在自己子树里又被引用了，已停止", spec.name.c_str());
                break;
            }
            stack.push_back(spec.name);
            BuildingGuard guard; // 出错/提前 break 也会正确出栈

            auto child = std::make_unique<AppearanceView>(spec.name, false);
            // 只覆盖写了的字段（存在标记由 compile 侧设好，见 compile/appearance_children.cpp）
            applyAppearanceBox(child.get(), spec.box);
            if (!spec.text.empty())
                child->setText(spec.text);
            if (!spec.icon.empty())
            {
                // JSON 里写 `@icon/star` 这种名字也能用：图标控件要的是**字形**，
                // 而字形只有 C++ 侧能查（`icon::glyphByName`）—— 这里替使用者查一次。
                // 直接写字形（或普通文字）也照旧可行，不破坏既有写法。
                if (spec.icon.rfind("@icon/", 0) == 0)
                {
                    const std::string iconName = spec.icon.substr(6); // 局部变量：保证 c_str() 生命周期
                    child->setIcon(icon::glyphByName(iconName.c_str()));
                }
                else
                    child->setIcon(spec.icon);
            }
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
        {
            // ref 引用的是主题 `view/` 里的**视图模板**：复用现成的 ViewTemplate + instantiate，
            // 所以库里已有的一切控件（Button / Label / Cell…）都能被外观引用，
            // 绑定与 i18n 也沿用同一套，不用另造"控件注册表"。
            std::shared_ptr<const ViewTemplate> tmpl = loadViewTemplateFromTheme(spec.name);
            if (tmpl == nullptr)
            {
                BKLV_LOG_WARNING("children 的 ref \"%s\" 找不到视图模板", spec.name.c_str());
                break;
            }

            // instantiate() 返回裸指针（库的既有约定：失败返回 nullptr）
            View* view = tmpl->instantiate();
            if (view == nullptr)
            {
                BKLV_LOG_WARNING("children 的 ref \"%s\" 实例化失败", spec.name.c_str());
                break;
            }
            if (Box* box = view->asBox())
                applyAppearanceBox(box, spec.box);

            parent.addView(view);
            break;
        }
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
