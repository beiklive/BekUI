#include "demo_common.h" // 引入项目的 SDL 窗口和 ImGui 帧循环。
#include <yoga/Yoga.h> // 引入 Yoga 的布局节点和样式 API。
#include <algorithm> // 引入 max，用于给画布宽度设下限。
#include <cmath> // 引入 floor，用于取整画布坐标。
#include <cstdio> // 引入 snprintf，用于格式化布局尺寸文本。

// 记录整棵树在本帧画布上的绘制原点，供各绘制步骤共享。
static ImVec2 g_canvas_origin;

// 在主画布上绘制文本，不占用任何 ImGui 窗口的光标。
static void CanvasText(ImDrawList* draw_list, float x, float y, const char* text, ImU32 color) { // 按绝对坐标输出一行文字。
    draw_list->AddText(ImVec2(g_canvas_origin.x + x, g_canvas_origin.y + y), color, text); // 用绘制列表直接写字。
} // 结束画布文本绘制。

static void DrawYogaDemo() { // 在主画布上绘制 Yoga 自动布局示例。
    const ImGuiViewport* viewport = ImGui::GetMainViewport(); // 取得主视口，代替窗口提供坐标系。
    ImDrawList* draw_list = ImGui::GetForegroundDrawList(); // 取得主画布绘制列表，它不依赖窗口栈。
    const float margin = 24.0f; // 定义内容相对主画布左上角的留白。
    g_canvas_origin = ImVec2(viewport->WorkPos.x + margin, viewport->WorkPos.y + margin); // 固定本帧的画布原点。
    YGNodeRef root = YGNodeNew(); // 创建根布局节点。
    YGNodeStyleSetWidth(root, std::floor(std::max(320.0f, viewport->WorkSize.x - margin * 2.0f))); // 让根节点宽度跟随主画布可用宽度。
    YGNodeStyleSetHeight(root, 270.0f); // 设置根节点高度。
    YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow); // 设置子节点沿水平方向排列。
    YGNodeStyleSetAlignItems(root, YGAlignCenter); // 设置子节点沿交叉轴居中。
    YGNodeStyleSetJustifyContent(root, YGJustifySpaceBetween); // 设置子节点沿主轴两端分布。
    YGNodeStyleSetGap(root, YGGutterColumn, 16.0f); // 设置相邻子节点之间的水平间距。
    YGNodeStyleSetPadding(root, YGEdgeAll, 16.0f); // 设置根节点四周内边距。
    YGNodeRef cards[3]; // 声明三个子卡片节点。
    for (size_t index = 0; index < 3; ++index) { // 逐个创建三个布局子节点。
        cards[index] = YGNodeNew(); // 创建当前卡片节点。
        YGNodeStyleSetFlexGrow(cards[index], 1.0f); // 让卡片分享剩余的横向空间。
        YGNodeStyleSetMinWidth(cards[index], 150.0f); // 限制卡片最小宽度。
        YGNodeStyleSetMaxWidth(cards[index], index == 1 ? 260.0f : 320.0f); // 为中间卡片设置更小的最大宽度。
        YGNodeStyleSetHeight(cards[index], 160.0f); // 设置卡片固定高度。
        YGNodeInsertChild(root, cards[index], index); // 将卡片插入根布局节点。
    } // 结束子节点创建循环。
    YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR); // 根据根节点样式计算整个布局树。
    CanvasText(draw_list, 0.0f, 0.0f, "Yoga 负责计算布局，ImGui 只负责按计算结果绘制。", IM_COL32(230, 235, 240, 255)); // 在主画布上写说明标题。
    CanvasText(draw_list, 0.0f, 26.0f, "横向 Flex + gap + padding + 最小/最大宽度", IM_COL32(150, 160, 175, 255)); // 在主画布上列出本示例能力。
    const float canvas_top = 58.0f; // 定义布局树相对画布原点的纵向偏移。
    const ImU32 card_colors[3] = { // 定义三张卡片的背景颜色。
        IM_COL32(57, 96, 145, 255), // 定义第一张卡片为蓝色。
        IM_COL32(70, 125, 100, 255), // 定义第二张卡片为绿色。
        IM_COL32(145, 94, 67, 255) // 定义第三张卡片为橙棕色。
    }; // 结束卡片颜色定义。
    const char* card_names[3] = {"固定约束", "较窄上限", "弹性扩展"}; // 准备卡片标题文本。
    for (size_t index = 0; index < 3; ++index) { // 根据 Yoga 计算结果绘制每张卡片。
        const float x = g_canvas_origin.x + YGNodeLayoutGetLeft(cards[index]); // 计算卡片在主画布上的横坐标。
        const float y = g_canvas_origin.y + canvas_top + YGNodeLayoutGetTop(cards[index]); // 计算卡片在主画布上的纵坐标。
        const float width = YGNodeLayoutGetWidth(cards[index]); // 读取 Yoga 计算出的卡片宽度。
        const float height = YGNodeLayoutGetHeight(cards[index]); // 读取 Yoga 计算出的卡片高度。
        draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), card_colors[index], 10.0f); // 绘制带圆角的卡片背景。
        draw_list->AddText(ImVec2(x + 12.0f, y + 14.0f), IM_COL32_WHITE, card_names[index]); // 在卡片顶部绘制标题。
        char size_text[64]; // 声明卡片尺寸文本缓冲区。
        std::snprintf(size_text, sizeof(size_text), "%.0f x %.0f", width, height); // 将计算出的宽高格式化为文本。
        draw_list->AddText(ImVec2(x + 12.0f, y + 42.0f), IM_COL32(230, 235, 240, 255), size_text); // 在卡片中显示布局尺寸。
    } // 结束卡片绘制循环。
    const float root_height = YGNodeLayoutGetHeight(root); // 读取根节点由内容撑开的高度。
    CanvasText(draw_list, 0.0f, canvas_top + root_height + 16.0f, "调整窗口宽度、gap、min/max width 后，布局会在下一帧重算。", IM_COL32(150, 160, 175, 255)); // 在主画布上写后续实验提示。
    YGNodeFreeRecursive(root); // 释放根节点和其拥有的全部子节点。
} // 结束 Yoga 示例绘制函数。

int main() { // 程序入口。
    return learn_imgui::RunDemo("Demo 04 - Yoga Flexbox", DrawYogaDemo); // 创建窗口并持续运行 Yoga 示例。
} // 结束程序入口。

