#include "demo_common.h" // 引入本项目的 ImGui 初始化和收尾辅助函数。

int main() { // 程序入口。
    return learn_imgui::RunDemo("Demo 02 - Layout", [] { // 创建并运行布局示例窗口。
    ImGui::Begin("Demo 02 - Layout"); // 创建布局示例窗口。
    ImGui::Text("布局 API 示例"); // 显示布局示例标题。
    ImGui::Separator(); // 绘制分隔线。
    ImGui::BeginChild("左侧区域", ImVec2(220.0f, 180.0f), true); // 创建固定宽度的子窗口。
    ImGui::Text("左侧导航"); // 在子窗口中显示标题。
    ImGui::Button("首页", ImVec2(-1.0f, 0.0f)); // 使用负宽度让按钮填满可用宽度。
    ImGui::Button("设置", ImVec2(-1.0f, 0.0f)); // 创建第二个填满宽度的按钮。
    ImGui::EndChild(); // 结束左侧子窗口。
    ImGui::SameLine(); // 让右侧区域与左侧区域处于同一行。
    ImGui::BeginGroup(); // 将右侧控件组织成一个逻辑组。
    ImGui::Text("右侧内容"); // 显示右侧区域标题。
    ImGui::BeginTable("数据表", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg); // 创建三列数据表。
    ImGui::TableSetupColumn("名称"); // 设置第一列标题。
    ImGui::TableSetupColumn("状态"); // 设置第二列标题。
    ImGui::TableSetupColumn("数值"); // 设置第三列标题。
    ImGui::TableHeadersRow(); // 绘制表头行。
    const char* names[] = {"项目 A", "项目 B"}; // 准备表格中的名称数据。
    const char* states[] = {"完成", "进行中"}; // 准备表格中的状态数据。
    for (int row = 0; row < 2; ++row) { // 循环创建两行数据。
        ImGui::TableNextRow(); // 切换到下一行。
        ImGui::TableSetColumnIndex(0); // 选择第一列。
        ImGui::TextUnformatted(names[row]); // 写入名称文本。
        ImGui::TableSetColumnIndex(1); // 选择第二列。
        ImGui::TextUnformatted(states[row]); // 写入状态文本。
        ImGui::TableSetColumnIndex(2); // 选择第三列。
        ImGui::Text("%d", (row + 1) * 10); // 写入示例数值。
    } // 结束表格行循环。
    ImGui::EndTable(); // 结束数据表。
    ImGui::EndGroup(); // 结束右侧控件组。
    ImGui::End(); // 结束布局示例窗口。
    }); // 结束布局示例的帧绘制回调。
} // 结束程序入口。

