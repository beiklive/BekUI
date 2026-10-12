#pragma once

// 把**外观驱动的控件**注册成 JSON 视图类型，这样页面 JSON（resources/views/*.json）
// 也能直接写 Button / Rectangle / AppletFrame 这些控件。
//
// 为什么单独一个文件：注册表在 core（`registerViewType`），而这些类型在 widgets ——
// 放在 core 里注册就是 core → widgets 的反向依赖。所以由上层（App）调一次。
//
// App::App() 里已经调过了；测试或不建 App 的场合自己要显式调一次。
// 只注册"没有数据/上下文也能用"的那些：列表、Cell（要数据源）、Dialog / 输入框
// （要 App 或交互上下文）不注册。

namespace bklv
{

// 幂等，重复调用无副作用
void registerWidgetViewTypes();

} // namespace bklv
