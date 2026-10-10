#include "demo_common.h" // 引入本项目的 ImGui 初始化和收尾辅助函数。

static bool DrawToggleSwitch(const char* label, bool* value) { // 实现带标签的自定义开关。
    const ImVec2 size(180.0f, 32.0f); // 定义开关的固定宽高。
    const ImVec2 position = ImGui::GetCursorScreenPos(); // 记录控件左上角的屏幕坐标。
    ImGui::InvisibleButton(label, size); // 创建无视觉内容但可交互的按钮区域。
    const bool hovered = ImGui::IsItemHovered(); // 判断鼠标是否悬停在控件上。
    const bool clicked = ImGui::IsItemClicked(); // 判断本帧是否点击控件并保存结果。
    if (clicked) { // 只有点击控件时才切换状态。
        *value = !*value; // 点击时反转开关状态。
    } // 结束点击处理。
    const ImU32 background = ImGui::GetColorU32(*value ? ImGuiCol_ButtonActive : ImGuiCol_FrameBg); // 根据状态选择背景色。
    const ImU32 knob = ImGui::GetColorU32(*value ? ImGuiCol_CheckMark : ImGuiCol_TextDisabled); // 根据状态选择滑块颜色。
    ImDrawList* draw_list = ImGui::GetWindowDrawList(); // 取得当前窗口的底层绘制列表。
    draw_list->AddRectFilled(position, ImVec2(position.x + 54.0f, position.y + 24.0f), background, 12.0f); // 绘制圆角开关背景。
    const float knob_x = position.x + (*value ? 42.0f : 12.0f); // 计算滑块圆心的横坐标。
    draw_list->AddCircleFilled(ImVec2(knob_x, position.y + 12.0f), 8.0f, knob); // 绘制圆形滑块。
    const ImU32 text_color = ImGui::GetColorU32(hovered ? ImGuiCol_Text : ImGuiCol_TextDisabled); // 根据悬停状态选择标签颜色。
    draw_list->AddText(ImVec2(position.x + 68.0f, position.y + 4.0f), text_color, *value ? "已开启" : "已关闭"); // 绘制状态文本。
    return clicked; // 返回本帧是否点击了这个自定义控件。
} // 结束自定义开关。

int main() { // 程序入口。
    return learn_imgui::RunDemo("Demo 03 - Custom Widget", [] { // 创建并运行自定义控件示例窗口。
    ImGui::Begin("Demo 03 - Custom Widget"); // 创建自定义控件示例窗口。
    ImGui::Text("使用 InvisibleButton + ImDrawList 创建控件"); // 说明自定义控件的组成方式。
    static bool enabled = false; // 保存开关状态。
    DrawToggleSwitch("##custom_toggle", &enabled); // 绘制自定义开关并绑定状态。
    ImGui::End(); // 结束窗口。
    }); // 结束自定义控件示例的帧绘制回调。
} // 结束程序入口。
