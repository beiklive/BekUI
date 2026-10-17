#pragma once

// 从**图层注册表**派生检视器字段：不手写字段清单，op 加了参数，设计器自动多一个字段。
// （DESIGN.md §7：检视器由 AttributeDef / LayerParam 生成。）

#include <string>
#include <vector>

namespace designer
{

enum class FieldKind
{
    NUMBER,
    PIXELS,
    COLOR,
    BOOL,
    ENUM,
    TEXT,   // 固定字符串键（如 text 层的 text）
    INSET,  // [上,右,下,左]
    OFFSET, // [x,y]
};

struct FieldDesc
{
    std::string              key;
    FieldKind                kind = FieldKind::NUMBER;
    std::string              label;       // 中文名（空则用 key）
    std::string              help;        // 一句话说明
    std::vector<std::string> enumValues;  // ENUM 的候选
    float                    numberDefault = 0.0f;
    bool                     boolDefault   = false;
    unsigned int             colorDefault  = 0;
    bool                     declaredByRegistry = false; // 来自 LayerParam（否则是固定字段）
};

// 某个 op 在检视器里应出现的字段：注册表 params 在前，固定字段（inset/offset/align…）在后。
// op 未注册时返回空表（调用方应给出"未知 op"提示）。
std::vector<FieldDesc> layerFields(const std::string& op);

// 固定字段表（所有层都有）：id/op 不在这里（它们是身份，不可在检视器里改）
const std::vector<FieldDesc>& fixedLayerFields();

} // namespace designer
