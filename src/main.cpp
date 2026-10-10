#include "../demo/demo_common.h" // 引入 ImGui 初始化和收尾辅助函数。

int i = 100;

static void DrawLearningWindow() { // 绘制综合学习窗口。
    ImGui::Begin("ImGui Learning Main"); // 创建主窗口。
    ImGui::Text("这是 src/main.cpp 的基础入口"); // 显示入口说明。
    ImGui::Separator(); // 绘制分隔线。
    static bool show_details = true; // 保存详情区域的显示状态。
    ImGui::Checkbox("显示详情", &show_details); // 创建控制详情区域的复选框。
    if (show_details) { // 仅在状态开启时绘制详情。
        ImGui::BulletText("控件：Button、Checkbox、Slider、Combo"); // 列出控件学习内容。
        ImGui::BulletText("布局：Child、Group、Table、SameLine"); // 列出布局学习内容。
        ImGui::BulletText("自定义：InvisibleButton、ImDrawList"); // 列出自定义控件学习内容。
    } // 结束详情区域。
    ImGui::End(); // 结束主窗口。
} // 结束综合窗口绘制。

int main() { // 程序入口。
    return learn_imgui::RunDemo("ImGui Learning Main", DrawLearningWindow); // 创建窗口并持续绘制综合学习界面。
} // 结束程序入口。

