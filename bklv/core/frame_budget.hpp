#pragma once

// 帧预算：把"可以延后"的主线程工作按住时间闸门。
//
// 目标（用户拍板）：**界面永不卡顿，任务可以晚**。所以 `App::frame` 每帧开头设一个预算，
// 那些"这一帧不做也看不出来"的活 —— 后台任务投回主线程的回调、纹理上传、动图换帧、
// SVG 栅格化、纹理缓存淘汰 —— 在预算用完后主动停手，剩下的留给下一帧。
//
// **不吃预算的**：布局、视图 `update()` 里的业务逻辑、输入处理、绘制。
// 它们就是"这一帧必须做的事"，少了就是掉内容；预算只管上面那一类可延后的。
//
// 边界（别误解成硬实时）：
//   · 粒度是"一个可延后工作单元"，**不能打断**正在跑的那一个（比如一张超大图的上传、
//     一个在回调里做了重活的宿主）。单帧仍可能被一个不可分的单元拖长。
//   · 预算用尽只影响"谁来做、什么时候做"，不影响正确性：延后的活下一帧继续，
//     纹理缓存会持续超预算直到淘汰完（不会漏，只是慢一点）。
//   · 没调 `beginFrame()` 时 `isExhausted()` 恒为 false —— 不设预算 = 不限制（老行为）。
//
// 观测：`App::getFrameStats()` 给帧时间分位数与超时帧数；`getDeferredCount()` 是本帧
// 有多少个可延后单元因为预算被推迟（调参时看它：一直是 0 说明预算给得太大或者没有可延后的活）。

#include <cstddef>

namespace bklv::budget
{

// App::frame 每帧开头调用。budgetMs <= 0 表示这一帧不限制（仍会记 deferred 计数）。
void beginFrame(double budgetMs);

// App::frame 每帧末尾调用（之后 isFrameActive() 变 false）
void endFrame();

bool isFrameActive();

// 预算是否已经用完（没有活动帧时恒为 false）
bool isExhausted();

// 还能用多少毫秒（没有活动帧 / 不限制时返回一个很大的值）
double getRemainingMs();

double getBudgetMs();

// 具体调用点"因为预算停手"时上报一次（只用于观测，不影响判断）
void noteDeferred();

// 本帧上报的推迟次数（上一帧的值在 endFrame 之后仍可读）
size_t getDeferredCount();

// 测试 / 全局重置用（core/globals.hpp 的重置钩子也会调它）
void resetFrameBudget();

} // namespace bklv::budget
