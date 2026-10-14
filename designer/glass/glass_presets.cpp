#include "designer/glass/glass_presets.hpp"

#include <cmath>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "bklv/core/logger.hpp"

namespace designer::glass
{

namespace
{

// json 用库里的 nlohmann（和 ui_state / 外观文档同一份，不引入新依赖）
using Json = nlohmann::json;

float round3(float value)
{
    return std::round(value * 1000.0f) / 1000.0f;
}

bool nearlyEqual(float a, float b)
{
    return std::fabs(a - b) < 0.02f;
}

bool sameTuning(const GlassTuning& a, const GlassTuning& b)
{
    return a.enabled == b.enabled && a.quality == b.quality
        && nearlyEqual(a.blurRadius, b.blurRadius)
        && nearlyEqual(a.tintR, b.tintR) && nearlyEqual(a.tintG, b.tintG)
        && nearlyEqual(a.tintB, b.tintB) && nearlyEqual(a.tintA, b.tintA)
        && nearlyEqual(a.refractionHeight, b.refractionHeight)
        && nearlyEqual(a.refractionAmount, b.refractionAmount)
        && nearlyEqual(a.depthEffect, b.depthEffect)
        && nearlyEqual(a.chromaticAberration, b.chromaticAberration)
        && nearlyEqual(a.contrast, b.contrast)
        && nearlyEqual(a.innerShadow, b.innerShadow) && nearlyEqual(a.rimGlow, b.rimGlow)
        && nearlyEqual(a.specular, b.specular) && nearlyEqual(a.outerShadow, b.outerShadow)
        && nearlyEqual(a.shadowExtent, b.shadowExtent)
        && nearlyEqual(a.barRadius, b.barRadius) && nearlyEqual(a.panelRadius, b.panelRadius)
        && a.barOuterShadow == b.barOuterShadow;
}

} // namespace

const std::vector<GlassPreset>& glassPresets()
{
    static const std::vector<GlassPreset> presets = [] {
        std::vector<GlassPreset> list;

        // 清透：模糊小、染色淡 —— 想要"看得见背景"的时候用
        GlassPreset clear;
        clear.name  = "清透";
        clear.hint  = "模糊小、染色淡，背景看得很清楚";
        clear.tuning.blurRadius = 18.0f;
        clear.tuning.quality    = 1;
        clear.tuning.tintA      = 0.14f;
        clear.tuning.refractionAmount = -80.0f;
        clear.tuning.specular   = 0.36f;
        clear.tuning.rimGlow    = 0.30f;
        clear.tuning.showPanel  = false;
        list.push_back(clear);

        // 磨砂：当前默认档 —— 面板要"像一块磨砂玻璃"，文字要好读
        GlassPreset frosted;
        frosted.name = "磨砂";
        frosted.hint = "默认档：模糊中等、染色中等，面板立得住";
        list.push_back(frosted); // tuning 用 GlassTuning 的默认值

        // 厚重：模糊大、染色重、折射强 —— 想要"实心玻璃板"的感觉
        GlassPreset heavy;
        heavy.name  = "厚重";
        heavy.hint  = "模糊大、染色重，面板更实、更像玻璃板";
        heavy.tuning.blurRadius = 44.0f;
        heavy.tuning.quality    = 2;
        heavy.tuning.tintA      = 0.46f;
        heavy.tuning.refractionHeight = 30.0f;
        heavy.tuning.refractionAmount = -130.0f;
        heavy.tuning.depthEffect      = 0.85f;
        heavy.tuning.innerShadow      = 0.20f;
        heavy.tuning.rimGlow          = 0.22f;
        heavy.tuning.panelRadius      = 14.0f;
        heavy.tuning.showPanel        = false;
        list.push_back(heavy);

        // 极简：几乎不模糊、不染色、不折射 —— 只留一层淡淡的底和描边
        GlassPreset minimal;
        minimal.name  = "极简";
        minimal.hint  = "几乎不模糊、不染色，只留一层淡底和描边";
        minimal.tuning.blurRadius     = 6.0f;
        minimal.tuning.quality        = 0;
        minimal.tuning.tintA          = 0.08f;
        minimal.tuning.refractionAmount = -30.0f;
        minimal.tuning.depthEffect      = 0.30f;
        minimal.tuning.chromaticAberration = 0.20f;
        minimal.tuning.innerShadow      = 0.06f;
        minimal.tuning.rimGlow          = 0.18f;
        minimal.tuning.specular         = 0.20f;
        minimal.tuning.outerShadow      = 0.06f;
        minimal.tuning.shadowExtent     = 3.0f;
        minimal.tuning.panelRadius      = 10.0f;
        minimal.tuning.showPanel        = false;
        list.push_back(minimal);

        return list;
    }();
    return presets;
}

int matchGlassPreset(const GlassTuning& tuning)
{
    const std::vector<GlassPreset>& presets = glassPresets();
    for (int i = 0; i < (int)presets.size(); i++)
    {
        GlassTuning candidate = presets[i].tuning;
        candidate.showPanel   = tuning.showPanel; // 面板展开与否不算观感
        if (sameTuning(candidate, tuning))
            return i;
    }
    return -1;
}

bool applyGlassPreset(GlassTuning& tuning, int index)
{
    const std::vector<GlassPreset>& presets = glassPresets();
    if (index < 0 || index >= (int)presets.size())
        return false;

    const bool showPanel = tuning.showPanel; // 只换观感，不动"面板开着没"
    tuning               = presets[index].tuning;
    tuning.showPanel     = showPanel;
    tuning.clampToValidRange();
    return true;
}

std::string glassTuningToJson(const GlassTuning& tuning)
{
    Json doc               = Json::object();
    doc["version"]         = 2;
    doc["enabled"]         = tuning.enabled;
    doc["blurRadius"]      = round3(tuning.blurRadius);
    doc["quality"]         = tuning.quality;
    doc["tint"]            = Json::array({ round3(tuning.tintR), round3(tuning.tintG),
        round3(tuning.tintB), round3(tuning.tintA) });
    doc["refractionHeight"]    = round3(tuning.refractionHeight);
    doc["refractionAmount"]    = round3(tuning.refractionAmount);
    doc["depthEffect"]         = round3(tuning.depthEffect);
    doc["chromaticAberration"] = round3(tuning.chromaticAberration);
    doc["contrast"]            = round3(tuning.contrast);
    doc["innerShadow"]     = round3(tuning.innerShadow);
    doc["rimGlow"]         = round3(tuning.rimGlow);
    doc["specular"]        = round3(tuning.specular);
    doc["outerShadow"]     = round3(tuning.outerShadow);
    doc["shadowExtent"]    = round3(tuning.shadowExtent);
    doc["barRadius"]       = round3(tuning.barRadius);
    doc["panelRadius"]     = round3(tuning.panelRadius);
    doc["barOuterShadow"]  = tuning.barOuterShadow;
    return doc.dump(2);
}

bool glassTuningFromJson(const std::string& text, GlassTuning& tuning, std::string& error)
{
    const Json doc = Json::parse(text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object())
    {
        error = "玻璃参数不是合法的 JSON 对象";
        return false;
    }

    GlassTuning parsed = tuning; // 缺字段就保持原值

    parsed.enabled    = doc.value("enabled", parsed.enabled);
    parsed.blurRadius = doc.value("blurRadius", parsed.blurRadius);
    parsed.quality    = doc.value("quality", parsed.quality);
    if (doc.contains("tint") && doc["tint"].is_array() && doc["tint"].size() == 4)
    {
        parsed.tintR = doc["tint"][0].get<float>();
        parsed.tintG = doc["tint"][1].get<float>();
        parsed.tintB = doc["tint"][2].get<float>();
        parsed.tintA = doc["tint"][3].get<float>();
    }
    parsed.refractionHeight    = doc.value("refractionHeight", parsed.refractionHeight);
    parsed.refractionAmount    = doc.value("refractionAmount", parsed.refractionAmount);
    parsed.depthEffect         = doc.value("depthEffect", parsed.depthEffect);
    parsed.chromaticAberration = doc.value("chromaticAberration", parsed.chromaticAberration);
    parsed.contrast            = doc.value("contrast", parsed.contrast);
    parsed.innerShadow         = doc.value("innerShadow", parsed.innerShadow);
    parsed.rimGlow             = doc.value("rimGlow", parsed.rimGlow);
    parsed.specular            = doc.value("specular", parsed.specular);
    parsed.outerShadow         = doc.value("outerShadow", parsed.outerShadow);
    parsed.shadowExtent        = doc.value("shadowExtent", parsed.shadowExtent);
    parsed.barRadius           = doc.value("barRadius", parsed.barRadius);
    parsed.panelRadius         = doc.value("panelRadius", parsed.panelRadius);
    parsed.barOuterShadow      = doc.value("barOuterShadow", parsed.barOuterShadow);
    parsed.clampToValidRange();

    tuning = parsed;
    return true;
}

bool saveGlassTuning(const std::string& path, const GlassTuning& tuning, std::string& error)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        error = "打不开文件（没有写权限？）：" + path;
        return false;
    }

    file << glassTuningToJson(tuning) << "\n";
    if (!file.good())
    {
        error = "写文件失败：" + path;
        return false;
    }
    return true;
}

bool loadGlassTuning(const std::string& path, GlassTuning& tuning, std::string& error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        error = "打不开文件（不存在？）：" + path;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return glassTuningFromJson(buffer.str(), tuning, error);
}

} // namespace designer::glass
