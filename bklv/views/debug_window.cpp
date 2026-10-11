#include "bklv/views/debug_window.hpp"

#include "bklv/core/app.hpp"

#include <cstdio>
#include <vector>

#include "imgui.h"

#include "bklv/core/graphics.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/view.hpp"

namespace bklv
{

namespace
{

constexpr const char* WINDOW_TITLE = "bklv 日志";
constexpr size_t MAX_VISIBLE_HINT  = 2000; // 一次最多渲染多少行（配合 ListClipper 裁剪）

ImVec4 levelColor(LogLevel level)
{
    switch (level)
    {
        case LogLevel::DEBUG: return ImVec4(0.55f, 0.72f, 0.45f, 1.0f);
        case LogLevel::INFO: return ImVec4(0.45f, 0.68f, 0.92f, 1.0f);
        case LogLevel::WARNING: return ImVec4(0.90f, 0.76f, 0.30f, 1.0f);
        case LogLevel::ERROR: return ImVec4(0.92f, 0.42f, 0.38f, 1.0f);
    }
    return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
}

const char* levelTag(LogLevel level)
{
    switch (level)
    {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO ";
        case LogLevel::WARNING: return "WARN ";
        case LogLevel::ERROR: return "ERROR";
    }
    return "INFO ";
}

bool levelVisible(LogLevel level, bool debug, bool info, bool warning, bool error)
{
    switch (level)
    {
        case LogLevel::DEBUG: return debug;
        case LogLevel::INFO: return info;
        case LogLevel::WARNING: return warning;
        case LogLevel::ERROR: return error;
    }
    return true;
}

} // namespace

void DebugWindow::drawToolbar()
{
    if (ImGui::SmallButton("清空"))
        Logger::clear();

    ImGui::SameLine();
    ImGui::Checkbox("自动滚动", &this->autoScroll);

    ImGui::SameLine();
    ImGui::Checkbox("位置", &this->showLocation);

    ImGui::SameLine();
    ImGui::TextUnformatted("|");

    ImGui::SameLine();
    ImGui::Checkbox("D", &this->showDebug);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("显示 DEBUG");

    ImGui::SameLine();
    ImGui::Checkbox("I", &this->showInfo);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("显示 INFO");

    ImGui::SameLine();
    ImGui::Checkbox("W", &this->showWarning);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("显示 WARNING");

    ImGui::SameLine();
    ImGui::Checkbox("E", &this->showError);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("显示 ERROR");
}

void DebugWindow::draw()
{
    if (!this->enabled)
        return;

    // 日志窗口不该参与主界面的布局，浮在右上角，可拖可缩放。
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 20.0f, viewport->WorkPos.y + 20.0f),
        ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(560.0f, 320.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f, 160.0f), ImVec2(FLT_MAX, FLT_MAX));

    bool open = true;
    if (!ImGui::Begin(WINDOW_TITLE, &open, ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::End();
        if (!open)
            this->enabled = false;
        return;
    }

    // 标题栏的关闭按钮
    if (!open)
        this->enabled = false;

    // ---- 运行状态 ----
    const InputManager& input = getInputManager();
    View* focus               = input.getCurrentFocus();
    const Point pointer       = input.getPointerPosition();

    std::string focusName = "(none)";
    if (focus != nullptr)
    {
        focusName = focus->getClassString();
        const size_t pos = focusName.find("::");
        if (pos != std::string::npos)
            focusName = focusName.substr(pos + 2);
        if (!focus->getId().empty())
            focusName = focus->getId() + " " + focusName;
    }

    // 布局次数是累计值：静态页面不再每帧重算，所以这个数静止时应该停住不长。
    ImGui::Text("fps %.0f   布局 %u   后端 %s   纹理 %d", ImGui::GetIO().Framerate, App::getLayoutCount(),
        getGraphicsBackendName(),
        getLiveTextureCount());
    ImGui::Text("指针 %.0f,%.0f   焦点 %s", pointer.x, pointer.y, focusName.c_str());

    const std::deque<LogEntry>& logs = Logger::getLogs();
    ImGui::Text("日志 %zu 条", logs.size());
    ImGui::Separator();

    this->drawToolbar();
    ImGui::Separator();

    // ---- 日志列表 ----
    const float footer = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("##bklv_logs", ImVec2(0.0f, -footer), ImGuiChildFlags_Borders,
        ImGuiWindowFlags_HorizontalScrollbar);

    // 先筛出可见的行号，再用 ListClipper 只渲染可见区间。
    std::vector<size_t> visible;
    visible.reserve(logs.size() < MAX_VISIBLE_HINT ? logs.size() : MAX_VISIBLE_HINT);
    for (size_t i = 0; i < logs.size(); ++i)
    {
        if (levelVisible(logs[i].level, this->showDebug, this->showInfo, this->showWarning, this->showError))
            visible.push_back(i);
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(visible.size()));
    while (clipper.Step())
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
        {
            const LogEntry& entry = logs[visible[static_cast<size_t>(row)]];

            ImGui::TextDisabled("%s", entry.time.c_str());
            ImGui::SameLine();

            ImGui::TextColored(levelColor(entry.level), "%s", levelTag(entry.level));
            ImGui::SameLine();

            if (this->showLocation && !entry.location.empty())
            {
                ImGui::TextDisabled("%s", entry.location.c_str());
                ImGui::SameLine();
            }

            ImGui::TextUnformatted(entry.message.c_str());
        }
    }

    // 有新日志时滚到底
    const uint64_t version = Logger::getVersion();
    if (this->autoScroll && version != this->lastVersion)
    {
        ImGui::SetScrollHereY(1.0f);
        this->lastVersion = version;
    }

    ImGui::EndChild();

    ImGui::End();
}

} // namespace bklv
