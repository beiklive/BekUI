// 设计器逻辑层（designer/core/*）的无窗口测试。
//
// 这一层刻意不含任何界面代码，所以能直接测：主题登记、外观文档的"读→改→写"往返、
// 层的增删排序、diff、模板新建/直接编辑、原子写盘与 .bak、写盘前的编译校验拦截。
//
// 资源按可执行文件目录解析，所以开头必须 initResources()；测试产物写进
// dist/designer_test_work/ 下，不落到仓库树里。

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

#include "bklv/bklv.hpp"
#include "designer/core/appearance_doc.hpp"
#include "bklv/core/image_writer.hpp"
#include "bklv/widgets/widget_types.hpp"
#include "designer/core/diff.hpp"
#include "designer/core/image_diff.hpp"
#include "designer/core/json_patch.hpp"
#include "designer/core/project.hpp"
#include "designer/core/theme_edit.hpp"
#include "designer/core/theme_usage.hpp"
#include "designer/core/file_browser.hpp"
#include "designer/core/file_kind.hpp"
#include "designer/core/json_cache.hpp"
#include "designer/core/theme_key.hpp"
#include "designer/core/ui_state.hpp"
#include "designer/core/undo_stack.hpp"
#include "designer/core/theme_files.hpp"
#include "test_util.hpp"

namespace fs = std::filesystem;
using designer::AppearanceDoc;
using designer::Project;
using designer::ProjectMode;
using Json = nlohmann::ordered_json;

namespace
{

std::string workRoot()
{
    return bklv::resourcePath("designer_test_work");
}

void writeFile(const std::string& path, const std::string& text)
{
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
}

// 造一份最小的主题目录：theme.json / colors.json / metrics.json / 两个外观
std::string makeTheme(const std::string& root, const std::string& name)
{
    const std::string dir = root + "/" + name;
    fs::create_directories(dir + "/widgets");

    writeFile(dir + "/theme.json",
        R"({"name": ")" + name + R"(", "widgets": ["widgets/rectangle.json", "widgets/button.json"]})");
    writeFile(dir + "/colors.json", R"({"color": {"accent": {"base": "#00ffcc"}, "text": {"primary": "#ffffff"}}})");
    writeFile(dir + "/metrics.json", R"({"metric": {"font": {"size": {"md": 18}}}})");
    // 故意用 @theme 引用（反向引用那节测试要用；也顺便是真实用法）
    writeFile(dir + "/widgets/rectangle.json",
        R"({"box": {"height": "auto"}, "layers": [{"op": "rect", "id": "bg", "fill": "@theme/color.accent.base", "radius": "@theme/metric.radius.md"}], "states": {}})");
    writeFile(dir + "/widgets/button.json",
        R"({"box": {"height": 48}, "layers": [{"op": "text", "id": "title", "color": "#ffffff"}], "states": {"focused": {"title.color": {"to": "#00ffcc", "duration": 120, "easing": "linear"}}}})");
    return dir;
}

} // namespace

int main()
{
    bklv::initResources();
    bklv::registerWidgetViewTypes(); // 不建 App 的场合要自己注册（App::App() 会自动注册）

    const std::string work = workRoot();
    std::error_code code;
    fs::remove_all(work, code);
    fs::create_directories(work + "/resources/theme");

    const std::string themeRoot = work + "/resources/theme";
    writeFile(themeRoot + "/themes.json", R"({"default": "default", "available": ["default"]})");
    makeTheme(themeRoot, "default");

    // ---------------------------------------------------------------- 外观文档往返
    {
        AppearanceDoc doc;
        std::string error;
        const std::string text
            = R"({"box": {"height": 48, "minWidth": 160}, "layers": [{"op": "rect", "id": "bg", "fill": "#111111"}, {"op": "text", "id": "title", "color": "#ffffff"}], "states": {"focused": {"bg.fill": {"to": "#222222", "duration": 100, "easing": "linear"}}}, "extra": {"keep": "me"}})";
        check(doc.parse(text, error), "解析外观成功");

        // 没碰过的键必须原样写回去（未知键 extra 也要留着）
        check(Json::parse(doc.serialize()) == Json::parse(text), "往返不改内容");

        double height = 0.0;
        check(doc.getBoxNumber("height", height) && height == 48.0, "读到 box.height");
        doc.setBoxNumber("height", 56.0);
        check(Json::parse(doc.serialize())["box"]["height"] == 56, "改 box.height");

        checkEqualInt(doc.layerCount(), 2, "层数");
        checkEqual(doc.layerId(0), "bg", "第 1 层 id");
        checkEqual(doc.layerOp(1), "text", "第 2 层 op");

        const int added = doc.addLayer("shadow");
        checkEqualInt(doc.layerCount(), 3, "新增一层");
        checkEqual(doc.layerOp(added), "shadow", "新层 op");
        check(doc.layer(added)->contains("cutout"), "新层默认带 cutout 字段");

        // shadow 必须能移到 rect 之前（层序约定）
        check(doc.moveLayer(2, -1), "上移成功");
        checkEqual(doc.layerOp(1), "shadow", "移动后第 2 层是 shadow");
        check(!doc.moveLayer(0, -1), "越界上移被拒绝");

        const int copy = doc.duplicateLayer(0);
        checkEqualInt(copy, 1, "复制层插在下一格");
        checkEqual(doc.layerId(1), "bgCopy", "复制层 id 带后缀");

        doc.removeLayer(1);
        checkEqualInt(doc.layerCount(), 3, "删层");

        // 状态与通道
        checkEqualInt((int)doc.stateNames().size(), 1, "状态数");
        checkEqualInt(doc.channelCount("focused"), 1, "通道数");
        doc.addChannel("focused", "title.color");
        checkEqualInt(doc.channelCount("focused"), 2, "加通道");
        doc.removeChannel("focused", 0);
        checkEqualInt(doc.channelCount("focused"), 1, "删通道");

        doc.addState("selected");
        doc.addChannel("selected", "bg.fill");
        checkEqualInt((int)doc.stateNames().size(), 2, "加状态");
        doc.removeState("selected");
        checkEqualInt((int)doc.stateNames().size(), 1, "删状态");
    }

    // ---------------------------------------------------------------- 层序提醒
    {
        AppearanceDoc doc;
        std::string error;
        doc.parse(
            R"({"layers": [{"op": "text", "id": "title"}, {"op": "rect", "id": "hlBg"}, {"op": "shadow", "id": "s"}]})",
            error);
        const std::vector<std::string> warnings = doc.layerOrderWarnings();
        checkEqualInt((int)warnings.size(), 2, "层序提醒 2 条");
    }

    // ---------------------------------------------------------------- diff
    {
        const Json before = Json::parse(
            R"({"box": {"height": 48, "gap": 8}, "layers": [{"op": "rect", "fill": "#111111"}]})");
        const Json after = Json::parse(
            R"({"box": {"height": 56, "gap": 8}, "layers": [{"op": "rect", "fill": "#222222"}]})");
        const std::vector<designer::JsonChange> changes = designer::diffJson(before, after);
        checkEqualInt((int)changes.size(), 2, "差异条数");
        checkEqual(changes[0].path, "box.height", "差异路径");
        checkEqual(changes[0].before, "48", "差异旧值");
        checkEqual(changes[0].after, "56", "差异新值");
        checkEqual(changes[1].path, "layers[0].fill", "层内路径带下标");
        check(designer::changeSummary(changes[0]).find("48 → 56") != std::string::npos, "摘要可读");
    }

    // ---------------------------------------------------------------- 主题文件读写
    {
        std::vector<std::string> available;
        std::string defaultName;
        std::string error;
        check(designer::readThemesFile(themeRoot, available, defaultName, error), "读 themes.json");
        checkEqual(defaultName, "default", "默认主题");
        checkEqualInt((int)available.size(), 1, "可用主题数");

        std::vector<std::string> widgets;
        check(designer::readWidgetFileList(themeRoot + "/default", widgets, error), "读 theme.json 的 widgets");
        checkEqualInt((int)widgets.size(), 2, "外观文件数");
        checkEqual(designer::widgetNameFromPath(widgets[1]), "button", "外观短名");
        checkEqual(designer::widgetPathFromName("button"), "widgets/button.json", "短名转路径");
    }

    // ---------------------------------------------------------------- 模板新建 / 直接编辑
    {
        std::string error;
        Project project;
        check(designer::createProjectFromTemplate(themeRoot, "default", "my-skin", project, error), "模板新建");
        checkEqual(project.mode == ProjectMode::FORK ? "fork" : "?", "fork", "模式是 fork");
        check(designer::directoryExists(themeRoot + "/my-skin"), "新主题目录存在");
        check(designer::directoryExists(themeRoot + "/default"), "原主题还在");

        // 目标仓库的 themes.json 要登记新主题
        std::vector<std::string> available;
        std::string defaultName;
        designer::readThemesFile(themeRoot, available, defaultName, error);
        checkEqualInt((int)available.size(), 2, "登记后数量");
        check(available[1] == "my-skin", "登记了新名");

        // theme.json 里的 name 要跟着改
        std::string themeText;
        designer::readTextFile(themeRoot + "/my-skin/theme.json", themeText, error);
        checkEqual(Json::parse(themeText)["name"].get<std::string>(), "my-skin", "主题名已改");

        check(!designer::createProjectFromTemplate(themeRoot, "default", "my-skin", project, error), "重名会被拒绝");
        check(!designer::createProjectFromTemplate(themeRoot, "default", "ocean", project, error), "不能占用基准名");
        check(!designer::openProjectInPlace(themeRoot, "default", project, error), "基准主题不许就地编辑");
        check(designer::openProjectInPlace(themeRoot, "my-skin", project, error), "自建主题可以就地编辑");

        // 项目记录往返
        const std::string projectFile = work + "/project.json";
        check(designer::saveProjectFile(project, projectFile, error), "存项目记录");
        Project loaded;
        check(designer::loadProjectFile(projectFile, loaded, error), "读项目记录");
        checkEqual(loaded.name, project.name, "项目名一致");
        checkEqual(loaded.themeDir(), project.themeDir(), "主题目录一致");
    }

    // ---------------------------------------------------------------- 原子写盘 + .bak + 校验拦截
    {
        const std::string file = themeRoot + "/my-skin/widgets/rectangle.json";
        std::string error;

        std::string before;
        check(designer::readTextFile(file, before, error), "读原文件");

        check(designer::writeTextFileAtomic(file, std::string("{\"box\": {}, \"layers\": []}\n"), true,
                error), "写盘成功");
        check(fs::exists(file + ".bak"), "留了 .bak");
        check(!fs::exists(file + ".tmp"), "临时文件不残留");

        std::string backup;
        designer::readTextFile(file + ".bak", backup, error);
        check(backup == before, "备份是改动前的内容");

        std::string after;
        designer::readTextFile(file, after, error);
        check(after.find("\"layers\": []") != std::string::npos, "内容已替换");

        // 语法校验：坏 JSON 必须被拦下
        check(!designer::validateJsonText("{ \"a\": }", error), "坏 JSON 被拦");
        check(designer::validateJsonText("{\"a\": 1}", error), "好 JSON 通过");

        // 编译校验：引用不存在的主题 key 要失败（写盘前的那道闸）
        std::string compileError;
        check(bklv::Appearance::compile(R"({"layers": [{"op": "rect", "fill": "@theme/color.nope"}]})",
                bklv::getTheme(), compileError) == nullptr, "不存在的 @theme key 编译失败");
        check(!compileError.empty(), "错误信息带得出原因");

        // 整主题校验：把上面那份写坏的外观也算进去（改回来后再校验通过）
        designer::writeTextFileAtomic(file, before, false, error);
        std::string themeError;
        check(bklv::validateThemeAppearances(themeError), "整主题校验通过");
    }

    // ---------------------------------------------------------------- 新建/删除外观（core 层）
    {
        std::string error;

        // 新建：文件 + theme.json 登记都要有
        check(designer::createWidgetAppearance(themeRoot + "/my-skin", "my_widget",
                                R"({"box": {}, "layers": [], "states": {}})", error), "新建外观");
        check(fs::exists(themeRoot + "/my-skin/widgets/my_widget.json"), "文件已建");
        std::vector<std::string> widgets;
        designer::readWidgetFileList(themeRoot + "/my-skin", widgets, error);
        check(std::find(widgets.begin(), widgets.end(), std::string("widgets/my_widget.json"))
                != widgets.end(), "已登记进 theme.json");

        // 同名必须被拒绝（不能覆盖用户已有的外观）
        check(!designer::createWidgetAppearance(themeRoot + "/my-skin", "my_widget", "{}", error), "同名新建被拒");
        check(error.find("已经存在") != std::string::npos, "拒绝时给了原因");
        check(!designer::createWidgetAppearance(themeRoot + "/my-skin", "../evil", "{}", error), "带路径分隔符被拒");

        // 删除：登记摘掉，文件改名成 .deleted（不是直接删）
        check(designer::deleteWidgetAppearance(themeRoot + "/my-skin", "my_widget", error), "删除外观");
        designer::readWidgetFileList(themeRoot + "/my-skin", widgets, error);
        check(std::find(widgets.begin(), widgets.end(), std::string("widgets/my_widget.json"))
                == widgets.end(), "登记已摘掉");
        check(fs::exists(themeRoot + "/my-skin/widgets/my_widget.json.deleted"), "文件进了 .deleted");
    }

    // ---------------------------------------------------------------- 指纹与带冲突检测的写
    {
        const std::string path = themeRoot + "/my-skin/widgets/rectangle.json";
        std::string text;
        std::string error;
        designer::readTextFile(path, text, error);

        check(designer::fnvHash(text) == designer::fnvHash(text) && designer::fnvHash(text).size() == 16, "指纹稳定（16 位十六进制）");
        check(designer::fnvHash("a") != designer::fnvHash("b"), "不同内容指纹不同");

        check(designer::writeFileChecked(path, designer::fnvHash(text), "{\"box\": {}}", false, true,
                error), "指纹一致时能写");
        designer::readTextFile(path, text, error);

        // 磁盘内容变了：不带 force 必须拒绝
        check(!designer::writeFileChecked(path, "deadbeefdeadbeef", "{\"box\": {}}", false, true,
                error), "被外部改动时拒绝写");
        check(error.find("外部改动") != std::string::npos, "拒绝时给了原因");
        check(designer::writeFileChecked(path, "deadbeefdeadbeef", text, true, true, error), "force 可以覆盖");
    }

    // ---------------------------------------------------------------- 缩进沿用
    {
        check(designer::detectIndentChar("{\n  \"a\": 1\n}") == ' ', "认出空格缩进");
        check(designer::detectIndentChar("{\n\t\"a\": 1\n}") == '\t', "认出制表符缩进");
        checkEqualInt(designer::detectIndentWidth("{\n    \"a\": 1\n}"), 4, "认出 4 空格");
        checkEqualInt(designer::detectIndentWidth("{\n  \"a\": 1\n}"), 2, "认出 2 空格");
        checkEqualInt(designer::detectIndentWidth("{\"a\":1}"), 2, "单行 JSON 用默认 2");

        Json doc              = Json::parse(R"({"box": {"height": 48}})");
        const std::string four = designer::serializeJson(doc, ' ', 4);
        check(four.find("\n    \"box\"") != std::string::npos, "按 4 空格输出");
        const std::string tabbed = designer::serializeJson(doc, '\t', 1);
        check(tabbed.find("\n\t\"box\"") != std::string::npos, "按制表符输出");
        check(!tabbed.empty() && tabbed.back() == '\n', "输出带尾部换行");
    }

    {
        // 造两张小图：一张纯色、一张中心多一个方块
        const int w = 16;
        const int h = 16;
        std::vector<unsigned char> first((size_t)w * h * 4, 0);
        for (size_t i = 0; i < first.size(); i += 4)
        {
            first[i + 0] = 30;
            first[i + 1] = 30;
            first[i + 2] = 30;
            first[i + 3] = 255;
        }
        std::vector<unsigned char> second = first;
        for (int y = 4; y < 12; y++)
            for (int x = 4; x < 12; x++)
            {
                const size_t index = ((size_t)y * w + x) * 4;
                second[index + 0] = 255;
            }

        const std::string pathA = work + "/shot_a.png";
        const std::string pathB = work + "/shot_b.png";
        check(bklv::writePngFile(pathA, first.data(), w, h, 4), "写出基线图");
        check(bklv::writePngFile(pathB, second.data(), w, h, 4), "写出当前图");

        std::string error;
        const designer::ImageDiff same = designer::compareImages(pathA, pathA, error);
        check(same.sameSize, "同尺寸被认出");
        checkFloat(same.meanDiff, 0.0f, "同一张图平均差 0");
        checkFloat(same.changedRatio, 0.0f, "同一张图没有变化像素");

        const designer::ImageDiff diff = designer::compareImages(pathA, pathB, error);
        check(diff.sameSize, "两张图尺寸一致");
        check(diff.maxDiff >= 200, "最大差能看出白色的块");
        check(diff.changedRatio > 0.1f && diff.changedRatio < 0.3f, "变化像素占比在预期区间");

        // 尺寸不同只报尺寸，不算差异
        std::vector<unsigned char> small((size_t)8 * 8 * 4, 200);
        const std::string pathSmall = work + "/shot_small.png";
        bklv::writePngFile(pathSmall, small.data(), 8, 8, 4);
        const designer::ImageDiff mismatched = designer::compareImages(pathA, pathSmall, error);
        check(!mismatched.sameSize, "尺寸不同被认出来");

        // 差异图能写出来
        const std::string diffPath = work + "/shot_diff.png";
        check(designer::writeDiffImage(diffPath, pathA, pathB, error), "差异图写出成功");
        check(fs::exists(diffPath), "差异图文件存在");
    }

    // ---------------------------------------------------------------- 控件类型对 JSON 可见
    {
        bklv::View* button = bklv::createFromJSONString(
            R"({"type": "Button", "text": "确定", "variant": "primary"})");
        check(button != nullptr, "JSON 里能用 Button");

        bklv::View* rectangle = bklv::createFromJSONString(
            R"({"type": "Rectangle", "variant": "card", "cornerRadius": 12})");
        check(rectangle != nullptr, "JSON 里能用 Rectangle");

        bklv::View* frame = bklv::createFromJSONString(
            R"({"type": "AppletFrame", "children": [
                 {"type": "Header", "title": "页面"},
                 {"type": "Box", "grow": 1},
                 {"type": "BottomBar"}]})");
        check(frame != nullptr, "JSON 里能搭 AppletFrame 整页");

        delete button;
        delete rectangle;
        delete frame;
    }

    // ---------------------------------------------------------------- 层序自动重排
    {
        AppearanceDoc doc;
        std::string error;
        doc.parse(
            R"({"layers": [{"op": "text", "id": "title"}, {"op": "rect", "id": "bg"}, {"op": "rect", "id": "hlBg"}, {"op": "shadow", "id": "shadow"}, {"op": "rect", "id": "hlGlow"}]})",
            error);
        checkEqualInt((int)doc.layerOrderWarnings().size(), 2, "重排前 2 条层序提醒");

        check(doc.reorderLayersByConvention(), "执行重排");
        checkEqual(doc.layerId(0), "shadow", "第 1 层是 shadow");
        checkEqual(doc.layerId(1), "bg", "第 2 层是底色");
        checkEqual(doc.layerId(2), "hlBg", "第 3 层是 hlBg");
        checkEqual(doc.layerId(3), "title", "第 4 层是内容");
        checkEqual(doc.layerId(4), "hlGlow", "最后是 hlGlow");
        checkEqualInt((int)doc.layerOrderWarnings().size(), 0, "重排后没有提醒");
        check(!doc.reorderLayersByConvention(), "已经合规时不再动");
    }

    // ---------------------------------------------------------------- 新建页面视图
    {
        std::string error;
        const std::string viewsDir = work + "/resources/views";

        check(designer::createViewFile(viewsDir, "my_page", "empty", error), "新建空页面");
        check(fs::exists(viewsDir + "/my_page.json"), "文件已建");

        check(designer::createViewFile(viewsDir, "full_page", "page", error), "新建整页模板");
        std::string text;
        designer::readTextFile(viewsDir + "/full_page.json", text, error);
        check(text.find("AppletFrame") != std::string::npos, "整页模板带 AppletFrame");
        check(text.find("BottomBar") != std::string::npos, "整页模板带底栏");

        check(designer::createViewFile(viewsDir, "card_page", "card", error), "新建卡片模板");

        // 重名/带路径分隔符都要拒绝
        check(!designer::createViewFile(viewsDir, "my_page", "empty", error), "重名被拒");
        check(!designer::createViewFile(viewsDir, "../evil", "empty", error), "带路径被拒");

        // 生成的页面必须能实例化（模板写错就不该落盘）
        std::string json;
        designer::readTextFile(viewsDir + "/card_page.json", json, error);
        bklv::View* view = bklv::createFromJSONString(json);
        check(view != nullptr, "生成的页面能实例化");
        delete view;
    }

    // ---------------------------------------------------------------- 数值写法
    {
        check(designer::numberValue(64.0).dump() == "64", "整数写成整数");
        check(designer::numberValue(12.5).dump() == "12.5", "小数保留小数");
        check(designer::numberValue(0.0).dump() == "0", "0 写成 0");
    }

    // ---------------------------------------------------------------- 增量写盘（只动变化了的顶层段）
    {
        // 原文带手写排版与空行：只有 box 变了，其余字节必须原样保留
        const std::string original =
            "{\n"
            "  \"$comment\": \"手写的说明，别动我\",\n"
            "\n"
            "  \"box\": { \"height\": 48 },\n"
            "  \"layers\": [\n"
            "    { \"op\": \"rect\", \"id\": \"bg\" }\n"
            "  ]\n"
            "}\n";

        const std::string updated =
            "{\n"
            "  \"$comment\": \"手写的说明，别动我\",\n"
            "  \"box\": {\n"
            "    \"height\": 56\n"
            "  },\n"
            "  \"layers\": [\n"
            "    { \"op\": \"rect\", \"id\": \"bg\" }\n"
            "  ]\n"
            "}\n";

        std::string patched;
        std::string error;
        check(designer::patchTopLevel(original, updated, patched, error), "增量写盘成功");
        check(patched.find("\"height\": 56") != std::string::npos, "改动落进去了");
        check(patched.find("\"layers\": [\n    { \"op\": \"rect\", \"id\": \"bg\" }\n  ]")
                != std::string::npos,
            "没被改动的段逐字节保留（含手写缩进）");
        check(patched.find("\n\n  \"box\"") != std::string::npos
                || patched.find("\"box\"") != std::string::npos,
            "段间空行还在");
        check(patched.find("手写的说明") != std::string::npos, "元信息没丢");

        // 键数量/名字变了就放弃（让调用方退回整文件重写）
        check(!designer::patchTopLevel(original, R"({"box": {}})", patched, error),
            "顶层键少了要拒绝");
        check(!designer::patchTopLevel(original, R"({"other": 1, "box": {}})", patched, error),
            "顶层键变了要拒绝");

        // 完全一样时输出等于原文
        check(designer::patchTopLevel(original, original, patched, error) && patched == original,
            "没有改动时逐字节相同");
    }

    // ---------------------------------------------------------------- 主题 key 的反向引用
    {
        const std::string themeDir = themeRoot + "/default";
        std::string error;

        const std::vector<std::string> keys = designer::collectThemeKeysInText(
            R"({"a": "@theme/color.accent.base", "b": "@theme/accent.base", "c": ["@theme/metric.radius.md", "@theme/color.accent.base"]})");
        checkEqualSize(keys.size(), 2, "同一段的重复引用去重（两种写法归一到同一个 key）");
        check(std::find(keys.begin(), keys.end(), "color.accent.base") != keys.end(),
            "省掉 color. 的写法也能归一");
        check(std::find(keys.begin(), keys.end(), "metric.radius.md") != keys.end(),
            "metric 引用也在");

        const auto usage = designer::collectThemeKeyUsage(themeDir, error);
        check(!usage.empty(), "扫出引用关系");

        // 造的 fixture 里 rectangle.json 用 accent.base + radius.md，button.json 只用 text.primary
        const auto rectangleUses = usage.find("color.accent.base");
        check(rectangleUses != usage.end(), "accent.base 有引用者");
        if (rectangleUses != usage.end())
        {
            check(rectangleUses->second.size() >= 1, "至少一个外观用它");
            check(std::find(rectangleUses->second.begin(), rectangleUses->second.end(), "rectangle")
                    != rectangleUses->second.end(),
                "rectangle 在里面");
        }

        std::string unusedText = R"({"x": "@theme/color.nobody.uses.this"})";
        const std::vector<std::string> unusedKeys = designer::collectThemeKeysInText(unusedText);
        checkEqualSize(unusedKeys.size(), 1, "没被引用的 key 也能被抽出来");
    }

    // ---------------------------------------------------------------- 界面状态持久化
    {
        const std::string statePath = work + "/designer_state.json";
        designer::UiState defaults;
        check(!designer::loadUiState(statePath, defaults), "没有状态文件时返回 false（用默认值）");
        checkEqualInt(defaults.leftWidth, 320, "默认左栏宽度");

        designer::UiState state;
        state.leftWidth        = 411;
        state.rightWidth       = 512;
        state.leftCollapsed    = true;
        state.canvasPreset     = 1;
        state.canvasChecker    = false;
        state.canvasZoom       = 0;
        state.showGrid         = true;
        state.previewCompare   = true;
        state.lastAppearance   = "button.primary";
        state.lastViewFile     = "page_json.json";

        std::string error;
        check(designer::saveUiState(statePath, state, error), "写出状态文件");
        check(fs::exists(statePath), "状态文件在");

        designer::UiState loaded;
        check(designer::loadUiState(statePath, loaded), "读回状态文件");
        checkEqualInt(loaded.leftWidth, 411, "左栏宽度回来了");
        checkEqualInt(loaded.rightWidth, 512, "右栏宽度回来了");
        check(loaded.leftCollapsed, "折叠状态回来了");
        checkEqualInt(loaded.canvasPreset, 1, "画布预设回来了");
        check(!loaded.canvasChecker, "网格底开关回来了");
        checkEqualInt(loaded.canvasZoom, 0, "缩放回来了");
        check(loaded.showGrid, "对齐网格开关回来了");
        check(loaded.previewCompare, "并排对比开关回来了");
        checkEqual(loaded.lastAppearance, "button.primary", "上次看的外观回来了");
        checkEqual(loaded.lastViewFile, "page_json.json", "上次看的页面回来了");

        // 坏 JSON 不能崩，也不该覆盖已有值
        designer::writeTextFileAtomic(statePath, "{ 这不是 JSON", false, error);
        designer::UiState broken = state;
        check(!designer::loadUiState(statePath, broken), "坏 JSON 返回 false");
        checkEqualInt(broken.leftWidth, 411, "坏 JSON 不动已有值");
    }

    // ---------------------------------------------------------------- 状态通道的默认值
    {
        check(designer::defaultChannelValue("fill").is_string(), "颜色属性的默认值是颜色串");
        check(designer::defaultChannelValue("color").is_string(), "textColor 类同理");
        check(designer::defaultChannelValue("opacity").is_number(), "透明度是数字");
        check(designer::defaultChannelValue("radius").is_number(), "圆角是数字");

        AppearanceDoc doc;
        std::string error;
        doc.parse("{\"layers\": [{\"op\": \"rect\", \"id\": \"bg\"}], \"states\": {}}", error);
        doc.addChannel("focused", "bg.fill");
        doc.addChannel("focused", "bg.opacity");

        const auto& node = doc.root()["states"]["focused"];
        check(node["bg.fill"]["to"].is_string(), "加出来的颜色通道给的是颜色（给 0.0 会编译失败）");
        check(node["bg.opacity"]["to"].is_number(), "加出来的数值通道给的是数字");
        checkEqualInt(node["bg.fill"]["duration"], 120, "默认时长");
        checkEqual(node["bg.fill"]["easing"], "quadraticOut", "默认缓动");
    }

    // ---------------------------------------------------------------- 主题引用归一
    {
        // 主题加载时把 colors.json 展平成**带 color. 前缀**的键；
        // 外观里可以省前缀写 @theme/accent.base。两边必须归一到同一个名字，
        // 否则查表查空、值悄悄退化成 fallback（色板与对比度检查就这么失效过）
        checkEqual(designer::themeKeyFromReference("@theme/color.accent.base"), "color.accent.base",
            "带前缀的引用原样");
        checkEqual(designer::themeKeyFromReference("@theme/accent.base"), "color.accent.base",
            "省前缀的颜色引用补上 color.");
        checkEqual(designer::themeKeyFromReference("@theme/metric.radius.md"), "metric.radius.md",
            "metric 引用保持 metric.");
        checkEqual(designer::themeKeyFromReference("@theme/color/button/primaryText"),
            "color.button.primaryText", "写成斜杠也认");
        checkEqual(designer::themeKeyFromReference("#00ffcc"), "", "字面量颜色不算主题引用");
        checkEqual(designer::themeKeyFromReference("@theme/"), "", "空引用返回空串");
    }

    // ---------------------------------------------------------------- 撤销栈
    {
        designer::UndoStack history;
        history.reset("A");
        check(!history.canUndo(), "刚 reset 时没有可撤销的");

        history.push("A");
        checkEqualSize(history.size(), 1, "连续相同的内容不重复入栈");

        history.push("B");
        history.push("C");
        check(history.canUndo(), "有可撤销的");

        std::string out;
        check(history.undo(out), "撤销成功");
        checkEqual(out, "B", "撤销回到上一步");
        check(history.canRedo(), "有可重做的");
        check(history.undo(out) && out == "A", "再撤销回到开头");
        check(!history.canUndo(), "到头了不能继续撤销");

        check(history.redo(out) && out == "B", "重做回到 B");
        check(history.redo(out) && out == "C", "重做回到 C");
        check(!history.canRedo(), "重做到头");

        // 撤销之后再做新改动：重做链要断
        history.undo(out);
        history.push("D");
        check(!history.canRedo(), "新改动之后不能再重做旧分支");

        // 超过上限丢最老的
        designer::UndoStack capped;
        capped.reset("0");
        for (int i = 1; i <= 80; i++)
            capped.push(std::to_string(i));
        checkEqualSize(capped.size(), designer::UndoStack::MAX_STEPS, "步数被上限截住");
    }

    // ---------------------------------------------------------------- 草稿是哪种文件
    {
        const std::string themeDir = themeRoot + "/default";
        const std::string resDir   = themeRoot.substr(0, themeRoot.find_last_of('/'));

        check(designer::classifyThemeFile(themeDir + "/widgets/button.json", themeDir, resDir)
                == designer::FileKind::APPEARANCE,
            "widgets/*.json 是外观");
        check(designer::classifyThemeFile(themeDir + "/colors.json", themeDir, resDir)
                == designer::FileKind::TABLE,
            "colors.json 是主题表");
        check(designer::classifyThemeFile(themeDir + "/metrics.json", themeDir, resDir)
                == designer::FileKind::TABLE,
            "metrics.json 是主题表");
        check(designer::classifyThemeFile(resDir + "/views/page.json", themeDir, resDir)
                == designer::FileKind::VIEW,
            "views/*.json 是页面视图");
        check(designer::classifyThemeFile("/tmp/别人的东西.json", themeDir, resDir)
                == designer::FileKind::UNKNOWN,
            "既不是主题文件也不是视图 → UNKNOWN（保存时会被拒）");
        checkEqual(designer::fileKindName(designer::FileKind::TABLE), "主题表", "类型名字");
        // 结尾带斜杠也要认
        check(designer::classifyThemeFile(themeDir + "/widgets/x.json", themeDir + "/", resDir + "/")
                == designer::FileKind::APPEARANCE,
            "目录结尾带斜杠也认");
    }

    // ---------------------------------------------------------------- 最近修改的文件
    {
        const std::string dir = work + "/recent";
        fs::create_directories(dir);
        std::string writeError;
        designer::writeTextFileAtomic(dir + "/old.json", "{}", false, writeError);
        // 让 old.json 明显更早（有的文件系统时间精度只有秒）
        fs::last_write_time(dir + "/old.json",
            fs::last_write_time(dir + "/old.json") - std::chrono::seconds(3600));
        designer::writeTextFileAtomic(dir + "/new.json", "{}", false, writeError);
        designer::writeTextFileAtomic(dir + "/notes.txt", "不是 json", false, writeError);

        std::vector<designer::DirEntry> recent;
        std::string error;
        check(designer::listFilesByMtime(dir, 5, recent, error), "列最近修改成功");
        checkEqualSize(recent.size(), 2, "只列 .json（txt 不算）");
        checkEqual(recent[0].name, "new.json", "最新的排最前");
        checkEqual(recent[1].name, "old.json", "旧的排后面");

        std::vector<designer::DirEntry> limited;
        check(designer::listFilesByMtime(dir, 1, limited, error), "限制条数也能跑");
        checkEqualSize(limited.size(), 1, "只取 1 个");
        checkEqual(limited[0].name, "new.json", "取到的还是最新的");

        check(!designer::listFilesByMtime(work + "/不存在的目录", 5, recent, error),
            "目录不存在时返回 false");
    }

    // ---------------------------------------------------------------- JSON 文件缓存
    {
        const std::string path = work + "/cache_probe.json";
        std::string writeError;
        designer::writeTextFileAtomic(path, R"({"a": 1})", false, writeError);

        designer::JsonCache cache;
        const auto* first = cache.get(path);
        check(first != nullptr, "首次读到了");
        checkEqualInt((*first)["a"].get<int>(), 1, "内容对");

        // 改盘上的内容：缓存没清之前必须还是老的值（说明真的没重复读盘）
        designer::writeTextFileAtomic(path, R"({"a": 2})", false, writeError);
        const auto* cached = cache.get(path);
        check(cached == first, "第二次返回同一个对象（没重复读盘解析）");
        checkEqualInt((*cached)["a"].get<int>(), 1, "缓存里还是老值");
        checkEqualSize(cache.size(), 1, "只缓存了一条");

        cache.clear();
        const auto* reloaded = cache.get(path);
        checkEqualInt((*reloaded)["a"].get<int>(), 2, "清缓存后读到新值");

        // 读不到的文件返回 nullptr，而且不会每次都去读盘
        designer::JsonCache missing;
        check(missing.get(work + "/不存在的文件.json") == nullptr, "读不到返回 nullptr");
        checkEqualSize(missing.size(), 1, "失败也缓存（避免每帧重试）");
    }

    // ---------------------------------------------------------------- 打开项目用的文件浏览
    {
        const std::string browseRoot = work + "/browse";
        fs::create_directories(browseRoot + "/theme/a-skin");
        fs::create_directories(browseRoot + "/theme/b-skin");
        fs::create_directories(browseRoot + "/not-a-theme");
        fs::create_directories(browseRoot + "/.hidden");
        std::string writeError;
        designer::writeTextFileAtomic(browseRoot + "/theme/a-skin/theme.json",
            R"({"name": "a-skin"})", false, writeError);
        designer::writeTextFileAtomic(browseRoot + "/theme/b-skin/theme.json",
            R"({"name": "b-skin"})", false, writeError);

        std::vector<designer::DirEntry> entries;
        std::string error;
        check(designer::listDirectory(browseRoot, entries, error), "列目录成功");
        checkEqualSize(entries.size(), 2, "跳过隐藏项，剩 2 个目录（theme / not-a-theme）");

        // 目录在前、按名字排序
        check(entries[0].isDirectory, "第一项是目录");
        checkEqual(entries[0].name, "not-a-theme", "目录按名字排序的第一个");
        check(!entries[0].hasThemeManifest, "not-a-theme 里没有 theme.json");
        checkEqual(entries[1].name, "theme", "第二个目录");

        // 主题目录能被认出来
        std::vector<designer::DirEntry> themeEntries;
        designer::listDirectory(browseRoot + "/theme", themeEntries, error);
        checkEqualSize(themeEntries.size(), 2, "theme/ 下两个主题目录");
        check(themeEntries[0].hasThemeManifest, "a-skin 有 theme.json（浏览器里要标出来）");
        check(themeEntries[1].hasThemeManifest, "b-skin 也有");
        checkEqual(designer::findThemeManifest(browseRoot + "/theme/a-skin"),
            browseRoot + "/theme/a-skin/theme.json", "找到 theme.json");
        check(designer::findThemeManifest(browseRoot + "/not-a-theme").empty(),
            "没有 theme.json 时返回空");

        // 从 theme.json（或主题目录）反推主题根与主题名
        std::string themeRoot;
        std::string themeName;
        check(designer::themeInfoFromManifest(browseRoot + "/theme/a-skin/theme.json", themeRoot,
                  themeName, error),
            "从 theme.json 推出信息");
        checkEqual(themeRoot, browseRoot + "/theme", "主题根 = theme.json 的父目录的父目录");
        checkEqual(themeName, "a-skin", "主题名 = theme.json 的父目录名");

        check(designer::themeInfoFromManifest(browseRoot + "/theme/b-skin", themeRoot, themeName,
                  error),
            "直接给主题目录也认");
        checkEqual(themeName, "b-skin", "主题名对");
        check(!designer::themeInfoFromManifest(browseRoot + "/not-a-theme", themeRoot, themeName,
                  error),
            "没有 theme.json 的目录要拒绝");
        check(error.find("没有 theme.json") != std::string::npos, "拒绝时给得出原因");

        // 路径工具
        checkEqual(designer::joinPath("/a", "b"), "/a/b", "拼路径");
        checkEqual(designer::joinPath("/a/", "b"), "/a/b", "结尾有斜杠也不重复");
        checkEqual(designer::parentDirectory("/a/b/c"), "/a/b", "父目录");
        checkEqual(designer::parentDirectory("/a/"), "/", "根目录的父目录还是自己");
        check(!designer::homeDirectory().empty(), "能拿到 home（浏览器的起点）");
    }

    fs::remove_all(work, code);

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}
