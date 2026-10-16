#pragma once

// 玻璃的"思路层"（Style / Request / Queue / BlurPlan / Tuning / 形状解析）已经搬进
// `bklv/core/glass/` —— 那是库的东西，设计器只是使用者。
// 设计器这边保留的是：GL/VK 后端实现、调参预设、以及门面。
//
// 这个头只做一件事：把库里的玻璃类型引入 `designer::glass`，让后端与门面的既有代码
// 不用逐处改写。**新写的代码建议直接写 `bklv::glass::X`**，别依赖这个垫片。

#include "bklv/core/glass/glass_blur_plan.hpp"
#include "bklv/core/glass/glass_geometry.hpp"
#include "bklv/core/glass/glass_queue.hpp"
#include "bklv/core/glass/glass_request.hpp"
#include "bklv/core/glass/glass_style.hpp"
#include "bklv/core/glass/glass_tuning.hpp"

namespace designer::glass
{
using namespace bklv::glass;
} // namespace designer::glass
