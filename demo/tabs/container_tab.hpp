#pragma once

// 「容器」标签页：对话框、列表容器、通知三种形态。
//
// 这一页是"补回来的"：改成标签页之前它们散在首页上，重构时被一起删掉了。
//   - 对话框：玻璃遮罩 + 图标标题 + 底部按钮均分（一个按钮时铺满整行）
//   - 列表容器：ListBox（滚动条、空态、焦点自动滚入视口）
//   - 通知：图标形态 / 图片形态 / 连发 5 条（堆叠不设上限，逐条收回）

namespace bklv
{
class View;
}

namespace demo
{

class DemoApp;

bklv::View* buildContainerTab(DemoApp& app);

} // namespace demo
