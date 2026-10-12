#include "designer/core/ui_state.hpp"

#include <cmath>

#include <nlohmann/json.hpp>

#include "designer/core/theme_files.hpp"

using Json = nlohmann::ordered_json;

namespace designer
{

int clampPanelWidth(int width, int minWidth, int maxWidth)
{
    if (maxWidth < minWidth)
        return minWidth; // 窗口太窄时优先保证下限，不然面板会被压成负数
    if (width < minWidth)
        return minWidth;
    if (width > maxWidth)
        return maxWidth;
    return width;
}

float clampUiScale(float scale)
{
    if (scale < 0.7f)
        return 0.7f;
    if (scale > 1.6f)
        return 1.6f;
    return scale;
}

bool loadUiState(const std::string& path, UiState& out)
{
    std::string text;
    std::string error;
    if (!readTextFile(path, text, error))
        return false;

    const Json doc = Json::parse(text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object())
        return false;

    out.leftWidth        = doc.value("leftWidth", out.leftWidth);
    out.rightWidth       = doc.value("rightWidth", out.rightWidth);
    out.fileListHeight   = doc.value("fileListHeight", out.fileListHeight);
    out.uiScale          = clampUiScale(doc.value("uiScale", out.uiScale));
    out.leftCollapsed    = doc.value("leftCollapsed", out.leftCollapsed);
    out.canvasPreset     = doc.value("canvasPreset", out.canvasPreset);
    out.canvasChecker    = doc.value("canvasChecker", out.canvasChecker);
    out.canvasZoom       = doc.value("canvasZoom", out.canvasZoom);
    out.showGrid         = doc.value("showGrid", out.showGrid);
    out.onlyLayers       = doc.value("onlyLayers", out.onlyLayers);
    out.previewContext   = doc.value("previewContext", out.previewContext);
    out.previewCompare   = doc.value("previewCompare", out.previewCompare);
    out.previewMaximized = doc.value("previewMaximized", out.previewMaximized);
    out.previewSubtree   = doc.value("previewSubtree", out.previewSubtree);
    out.lastAppearance   = doc.value("lastAppearance", out.lastAppearance);
    out.lastViewFile     = doc.value("lastViewFile", out.lastViewFile);
    return true;
}

bool saveUiState(const std::string& path, const UiState& state, std::string& error)
{
    Json doc = Json::object();
    doc["leftWidth"]        = state.leftWidth;
    doc["rightWidth"]       = state.rightWidth;
    doc["fileListHeight"]   = state.fileListHeight;
    doc["uiScale"]          = std::round(state.uiScale * 100.0f) / 100.0f;
    doc["leftCollapsed"]    = state.leftCollapsed;
    doc["canvasPreset"]     = state.canvasPreset;
    doc["canvasChecker"]    = state.canvasChecker;
    doc["canvasZoom"]       = state.canvasZoom;
    doc["showGrid"]         = state.showGrid;
    doc["onlyLayers"]       = state.onlyLayers;
    doc["previewContext"]   = state.previewContext;
    doc["previewCompare"]   = state.previewCompare;
    doc["previewMaximized"] = state.previewMaximized;
    doc["previewSubtree"]   = state.previewSubtree;
    doc["lastAppearance"]   = state.lastAppearance;
    doc["lastViewFile"]     = state.lastViewFile;
    return writeTextFileAtomic(path, doc.dump(2) + "\n", false, error);
}

} // namespace designer
