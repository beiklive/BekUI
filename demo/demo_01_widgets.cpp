#include "demo_common.h" // 引入本项目的 ImGui 初始化和收尾辅助函数。

int main() { // 程序入口。
    return learn_imgui::RunDemo("Demo 01 - Widgets", [] { // 创建并运行基础控件示例窗口。
        ImGui::Begin("Demo 01 - Widgets"); // 创建一个用于学习基础控件的窗口。
        ImGui::Text("基础控件示例"); // 显示普通文本。
        ImGui::Separator(); // 绘制分隔线，划分控件区域。
        static bool enabled = true; // 保存复选框的状态，static 让状态跨帧保留。
        ImGui::Checkbox("启用功能", &enabled); // 创建复选框并绑定状态变量。
        static int count = 3; // 保存整数输入框的数值。
        ImGui::InputInt("数量", &count); // 创建整数输入控件。
        static float progress = 0.65f; // 保存浮点滑块的数值。
        ImGui::SliderFloat("进度", &progress, 0.0f, 1.0f); // 创建范围为 0 到 1 的滑块。
        static int choice = 1; // 保存单选项当前索引。
        ImGui::RadioButton("选项 A", &choice, 0); // 创建第一个单选按钮。
        ImGui::SameLine(); // 将下一个控件放到当前控件右侧。
        ImGui::RadioButton("选项 B", &choice, 1); // 创建第二个单选按钮。
        const char* colors[] = {"红色", "绿色", "蓝色"}; // 准备下拉框的选项文本。
        static int color_index = 0; // 保存下拉框当前选项。
        ImGui::Combo("颜色", &color_index, colors, 3); // 创建下拉选择框。
        if (ImGui::Button("点击按钮")) { // 创建按钮并判断本帧是否被点击。
            count += 1; // 点击按钮后将数量增加一。
        } // 结束按钮点击处理。
        ImGui::SameLine(); // 将文本放到按钮右侧。
        ImGui::Text("当前数量: %d", count); // 显示当前数量。
        ImGui::End(); // 结束窗口。
    }); // 结束基础控件示例的帧绘制回调。
} // 结束程序入口。

