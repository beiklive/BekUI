#pragma once

// 玻璃观感的"预设"与存取。
//
// 为什么单独一份：观感是要反复调的（清透一点 / 磨砂一点），把几套调好的参数固化下来
// 才能一键切换、才能分享给别人。纯数据 + 纯函数，不碰 ImGui，所以能无窗口测
// （tests/designer_core_test.cpp 里有往返断言）。

#include <string>
#include <vector>

#include "designer/glass/glass_lib.hpp"
#include "bklv/core/glass/glass_tuning.hpp"

namespace designer::glass
{

struct GlassPreset
{
    const char* name;
    const char* hint;    // 一句话说明这档的观感
    GlassTuning tuning;
};

// 内置预设（顺序就是界面上的按钮顺序）
const std::vector<GlassPreset>& glassPresets();

// 当前参数最接近哪一档；都不接近时返回 -1（说明是手调出来的）
int matchGlassPreset(const GlassTuning& tuning);

// 套用某一档（越界返回 false，不改动入参）
bool applyGlassPreset(GlassTuning& tuning, int index);

// 存 / 取：一份 json（默认放可执行文件目录的 designer_glass.json），
// 方便"调好一套 → 存下来 → 以后载入"或者贴给别人。
// 失败时 error 里是人话，返回 false。
bool saveGlassTuning(const std::string& path, const GlassTuning& tuning, std::string& error);
bool loadGlassTuning(const std::string& path, GlassTuning& tuning, std::string& error);

// 序列化（测试与上面两个函数共用）
std::string glassTuningToJson(const GlassTuning& tuning);
bool glassTuningFromJson(const std::string& text, GlassTuning& tuning, std::string& error);

} // namespace designer::glass
