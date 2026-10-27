#pragma once

#include "bklv/ui/view/view.hpp"

namespace demo
{

class DemoApp;

// 第六个标签页：动画。
//  - 系统内部动画：demo::SystemCard 每帧改 layout 位移 + 玻璃层 radius（连续形变/位移）
//  - JSON 动画：card_transition 外观里的 transitions.show，由右侧按钮触发
bklv::View* buildAnimationTab(DemoApp& app);

} // namespace demo
