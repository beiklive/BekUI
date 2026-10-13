#pragma once

// 设计器用的矢量图标（不依赖图标字体，直接画）。
//
// 为什么自己画：仓库里只有 Switch 的私有区图标字体，没有 material icon 字体；
// 而且这几个图标形状简单，画出来更清晰、也不受字体缺字形影响。
// **对齐规则**：纵向按行高取中（见 core/icon_align.hpp），不要把图标贴到文字顶上。

#include "bklv/3rd/imgui/imgui.h"

namespace designer
{

enum class Icon
{
    NONE,
    WIDGET,      // 控件：方块
    PAGE,        // 页面：文档
    TABLE,       // 主题表：几行
    LAYER,       // 层：菱形
    STATE,       // 状态：圆
    FIELD,       // 字段：加号
    EYE,         // 可见
    LAYERS_ONLY, // 只看层：叠起来的菱形
    EXPAND,      // 展开全部
    COLLAPSE,    // 收起
    GRID,        // 网格底
    ALIGN,       // 对齐网格
    STRIP,       // 绘制顺序条
    FIT,         // 适应窗宽
    RESET,       // 重置视图
    COPY,        // 复制
    PASTE,       // 粘贴
    PLUS,        // 增加
    TRASH,       // 删除
    CHEVRON_RIGHT,
    CHEVRON_DOWN,
};

// 在 center 画一个 size 大小的图标（size 通常取字号或 14~16）
void drawIcon(ImDrawList* list, Icon icon, ImVec2 center, float size, ImU32 color);

// 画"图标 + 文字"，保证图标与文字纵向对齐（图标在前，光标停在文字后面）
void iconText(Icon icon, const char* text, ImU32 iconColor, const ImVec4& textColor);

// 只有图标的小按钮（返回是否被点）：用于标题右侧那排开关
bool iconButton(const char* id, Icon icon, float size, bool activeState, const char* tooltip);

} // namespace designer
