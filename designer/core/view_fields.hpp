#pragma once

// 视图节点的属性字段：**从控件自己的属性表读**（不手写清单）。
// 做法：用 ViewTemplate 实例化一个"只有 type 的最小节点"，读它的 attributes() + 公共表。
// 好处：27 种类型、以后新增的控件/属性都自动出现；坏处：需要实例化一次（很便宜）。

#include <string>
#include <vector>

#include "bklv/core/attributes.hpp"

namespace designer
{

struct ViewFieldDesc
{
    std::string             key;
    bklv::AttributeKind     kind = bklv::AttributeKind::STRING;
    std::string             allowed; // ENUM 的候选（"row|column"）
    bool                    common = false; // 来自公共属性表（所有控件都有）
};

// type 未注册（或实例化失败）返回空表
std::vector<ViewFieldDesc> viewFields(const std::string& type);

} // namespace designer
