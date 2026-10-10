#include "demo_common.h" // 引入项目的 SDL 窗口和 ImGui 帧循环。
#include <tweeny/tweeny.h> // 引入 Tweeny 的头文件动画 API。
#include <cstdio> // 引入 printf，用于展示 Tweeny 完成事件。

static void DrawTweenyDemo() { // 绘制 Tweeny 缓动动画示例。
    ImGui::Begin("Demo 05 - Tweeny Animation"); // 创建 Tweeny 示例窗口。
    ImGui::Text("Tweeny 插值数值；每帧把结果交给 ImGui 绘制。"); // 说明 Tweeny 与 ImGui 的职责分工。
    ImGui::Text("两段关键帧：0 -> 1 -> 0，使用不同缓动曲线。"); // 描述动画关键帧和缓动类型。
    static auto animation = tweeny::from(0.0f) // 创建从 0 开始的浮点动画。
        .to(1.0f).via(tweeny::easing::quadraticInOut).during(90U) // 添加 90 帧的二次缓入缓出关键帧。
        .to(0.0f).via(tweeny::easing::bounceOut).during(45U) // 添加 45 帧的弹跳回程关键帧。
        .build(); // 完成构建并得到可运行的 Tweeny 动画对象。
    static bool callback_registered = false; // 保存完成事件是否已经注册。
    if (!callback_registered) { // 首帧时只注册一次动画事件。
        animation.on(tweeny::event::complete, [](auto& tween) { // 注册动画完成时触发的回调。
            std::printf("Tweeny 动画完成，最终值：%.3f\n", tween.peek()); // 在终端输出动画完成信息。
            return tweeny::event::response::ok; // 通知 Tweeny 正常保留该事件监听器。
        }); // 结束完成事件回调注册。
        callback_registered = true; // 标记完成事件已注册。
    } // 结束一次性事件注册。
    static float frame_remainder = 0.0f; // 保存帧时间换算后的不足一帧余数。
    frame_remainder += ImGui::GetIO().DeltaTime * 60.0f; // 将秒换算成以 60 FPS 为基准的 Tweeny 帧数。
    const int frames = static_cast<int>(frame_remainder); // 提取本次需要推进的整数帧数。
    if (frames > 0) { // 只有累计出完整帧时才推进 Tweeny。
        animation.step(frames); // 按本帧经过的时间推进动画。
        frame_remainder -= static_cast<float>(frames); // 保留尚未凑成一帧的小数时间。
        if (animation.progress() >= 1.0f) { // 检查整个多段动画是否播放完成。
            animation.seek(0U); // 完成后跳回起点，让示例循环播放。
        } // 结束动画循环判断。
    } // 结束帧推进判断。
    const float value = animation.peek(); // 读取当前动画插值结果。
    ImGui::ProgressBar(value, ImVec2(-1.0f, 0.0f)); // 将当前插值显示为进度条。
    ImGui::Text("当前值：%.3f    总进度：%.0f%%", value, animation.progress() * 100.0f); // 显示插值值和总动画进度。
    const float ball_x = 60.0f + value * 560.0f; // 将插值映射到窗口里的水平位置。
    const ImVec2 origin = ImGui::GetCursorScreenPos(); // 记录动画绘制区域左上角的屏幕坐标。
    ImDrawList* draw_list = ImGui::GetWindowDrawList(); // 取得当前窗口的底层绘制列表。
    draw_list->AddLine(ImVec2(origin.x + 30.0f, origin.y + 28.0f), ImVec2(origin.x + 630.0f, origin.y + 28.0f), IM_COL32(100, 110, 125, 255), 2.0f); // 绘制小球运动轨道。
    draw_list->AddCircleFilled(ImVec2(origin.x + ball_x, origin.y + 28.0f), 14.0f, IM_COL32(90, 170, 245, 255)); // 根据插值绘制运动小球。
    ImGui::Dummy(ImVec2(660.0f, 58.0f)); // 为动画轨道占据 ImGui 布局空间。
    ImGui::Separator(); // 分隔 API 学习说明。
    ImGui::BulletText("from(...).to(...).via(...).during(...).build() 创建动画"); // 展示 Tweeny 流式构建 API。
    ImGui::BulletText("step(frames) 推进，peek() 读取当前值，progress() 读取进度"); // 展示动画更新和查询 API。
    ImGui::BulletText("多次调用 to() 添加关键帧，via() 选择每段的 easing"); // 展示多段关键帧和缓动曲线 API。
    ImGui::BulletText("on(event::complete, callback) 监听动画完成事件"); // 展示 Tweeny 事件监听 API。
    ImGui::End(); // 结束 Tweeny 示例窗口。
} // 结束 Tweeny 示例绘制函数。

int main() { // 程序入口。
    return learn_imgui::RunDemo("Demo 05 - Tweeny Animation", DrawTweenyDemo); // 创建窗口并持续运行 Tweeny 示例。
} // 结束程序入口。

