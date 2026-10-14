#include "designer/core/ui_state.hpp"

namespace
{
// 玻璃默认参数的版本：改了默认观感就 +1，让老状态文件里的旧参数失效
constexpr int kGlassTuningVersion = 2;

}

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
    out.uiScale = clampUiScale(doc.value("uiScale", out.uiScale));
    out.leftCollapsed    = doc.value("leftCollapsed", out.leftCollapsed);
    out.canvasPreset     = doc.value("canvasPreset", out.canvasPreset);
    out.canvasChecker    = doc.value("canvasChecker", out.canvasChecker);
    out.canvasBackdrop   = doc.value("canvasBackdrop", out.canvasChecker ? 0 : 1);
    out.canvasZoom       = doc.value("canvasZoom", out.canvasZoom);
    out.showGrid         = doc.value("showGrid", out.showGrid);
    out.onlyLayers       = doc.value("onlyLayers", out.onlyLayers);
    out.previewContext   = doc.value("previewContext", out.previewContext);
    out.previewCompare   = doc.value("previewCompare", out.previewCompare);
    out.previewMaximized = doc.value("previewMaximized", out.previewMaximized);
    out.previewSubtree   = doc.value("previewSubtree", out.previewSubtree);
    // 玻璃参数：老状态文件里没有这一节就保持默认。
    // version 用来作废旧参数 —— 默认值调过一次（面板曾经是黑的），旧文件里的
    // tintA=0.30 会把新默认盖掉，界面上又变回暗面板
    if (doc.contains("glass") && doc["glass"].is_object()
        && doc["glass"].value("version", 0) == kGlassTuningVersion)
    {
        const Json& g = doc["glass"];
        glass::GlassTuning& t = out.glass;
        t.enabled             = g.value("enabled", t.enabled);
        t.blurRadius          = g.value("blurRadius", t.blurRadius);
        t.quality             = g.value("quality", t.quality);
        t.tintR               = g.value("tintR", t.tintR);
        t.tintG               = g.value("tintG", t.tintG);
        t.tintB               = g.value("tintB", t.tintB);
        t.tintA               = g.value("tintA", t.tintA);
        t.refractionHeight    = g.value("refractionHeight", t.refractionHeight);
        t.refractionAmount    = g.value("refractionAmount", t.refractionAmount);
        t.depthEffect         = g.value("depthEffect", t.depthEffect);
        t.chromaticAberration = g.value("chromaticAberration", t.chromaticAberration);
        t.contrast            = g.value("contrast", t.contrast);
        t.innerShadow         = g.value("innerShadow", t.innerShadow);
        t.rimGlow             = g.value("rimGlow", t.rimGlow);
        t.specular            = g.value("specular", t.specular);
        t.outerShadow         = g.value("outerShadow", t.outerShadow);
        t.shadowExtent        = g.value("shadowExtent", t.shadowExtent);
        t.barRadius           = g.value("barRadius", t.barRadius);
        t.panelRadius         = g.value("panelRadius", t.panelRadius);
        t.barOuterShadow      = g.value("barOuterShadow", t.barOuterShadow);
        t.showPanel           = g.value("showPanel", t.showPanel);
        t.clampToValidRange();
    }

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
    doc["canvasBackdrop"]   = state.canvasBackdrop;
    doc["canvasZoom"]       = state.canvasZoom;
    doc["showGrid"]         = state.showGrid;
    doc["onlyLayers"]       = state.onlyLayers;
    doc["previewContext"]   = state.previewContext;
    doc["previewCompare"]   = state.previewCompare;
    doc["previewMaximized"] = state.previewMaximized;
    doc["previewSubtree"]   = state.previewSubtree;
    {
        const glass::GlassTuning& t = state.glass;
        Json g                      = Json::object();
        g["version"]             = kGlassTuningVersion;
        g["enabled"]             = t.enabled;
        g["blurRadius"]          = t.blurRadius;
        g["quality"]             = t.quality;
        g["tintR"]               = t.tintR;
        g["tintG"]               = t.tintG;
        g["tintB"]               = t.tintB;
        g["tintA"]               = t.tintA;
        g["refractionHeight"]    = t.refractionHeight;
        g["refractionAmount"]    = t.refractionAmount;
        g["depthEffect"]         = t.depthEffect;
        g["chromaticAberration"] = t.chromaticAberration;
        g["contrast"]            = t.contrast;
        g["innerShadow"]         = t.innerShadow;
        g["rimGlow"]             = t.rimGlow;
        g["specular"]            = t.specular;
        g["outerShadow"]         = t.outerShadow;
        g["shadowExtent"]        = t.shadowExtent;
        g["barRadius"]           = t.barRadius;
        g["panelRadius"]         = t.panelRadius;
        g["barOuterShadow"]      = t.barOuterShadow;
        g["showPanel"]           = t.showPanel;
        doc["glass"]             = g;
    }

    doc["lastAppearance"]   = state.lastAppearance;
    doc["lastViewFile"]     = state.lastViewFile;
    return writeTextFileAtomic(path, doc.dump(2) + "\n", false, error);
}

} // namespace designer
