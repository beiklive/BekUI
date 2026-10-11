// 无窗口测试：JSON 视图描述（属性往返 / 畸形输入 / 模板复用 / include）。
//
// 布局用 YGNodeCalculateLayout 直接驱动（纯计算，不需要窗口/图形后端），
// 所以百分比、auto 这类尺寸语义能在没有窗口的情况下断言。
//
// 测试自己会在 <可执行文件目录>/views/ 下写几个 fixture 文件（include 与循环 include 用），
// 不往仓库里塞测试专用资源。

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <yoga/Yoga.h>

#include "bklv/core/box.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/json_view.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/views/image.hpp"
#include "bklv/views/label.hpp"

using namespace bklv;

static int g_failed = 0;
static int g_checks = 0;

static void check(bool ok, const std::string& what)
{
    g_checks++;
    if (ok)
        std::printf("  [通过] %s\n", what.c_str());
    else
    {
        g_failed++;
        std::printf("  [失败] %s\n", what.c_str());
    }
}

static void checkEqual(const std::string& got, const std::string& want, const std::string& what)
{
    g_checks++;
    if (got == want)
        std::printf("  [通过] %s  =>  \"%s\"\n", what.c_str(), got.c_str());
    else
    {
        g_failed++;
        std::printf("  [失败] %s\n         期望 \"%s\"\n         实际 \"%s\"\n", what.c_str(), want.c_str(),
            got.c_str());
    }
}

static void checkFloat(float got, float want, const std::string& what)
{
    g_checks++;
    if (got > want - 0.01f && got < want + 0.01f)
        std::printf("  [通过] %s  =>  %g\n", what.c_str(), got);
    else
    {
        g_failed++;
        std::printf("  [失败] %s  期望 %g 实际 %g\n", what.c_str(), want, got);
    }
}

// 没有 RTTI（Switch 上 -fno-rtti），类型判断走 getClassString()，
// 这也是 bklv 里 asBox() 那套虚函数的做法。
static Label* asLabel(View* view)
{
    return view != nullptr && view->getClassString() == "bklv::Label" ? static_cast<Label*>(view) : nullptr;
}

static Image* asImage(View* view)
{
    return view != nullptr && view->getClassString() == "bklv::Image" ? static_cast<Image*>(view) : nullptr;
}

// 从文本建树；失败返回 nullptr。
static View* build(const std::string& json)
{
    View* view = createFromJSONString(json);
    return view;
}

// 建树并强制跑一次布局（指定画布尺寸），用于断言尺寸类属性。
static View* buildAndLayout(const std::string& json, float width, float height)
{
    View* view = build(json);
    if (view == nullptr)
        return nullptr;
    YGNodeCalculateLayout(view->getYGNode(), width, height, YGDirectionLTR);
    return view;
}

static void writeFixture(const std::string& relativePath, const std::string& content)
{
    const std::string full = resourcePath(relativePath);
    // 目录要先建出来（资源目录里本来只有 resources/）——ofstream 不会自己建，
    // 静默失败会让人以为"JSON 装载坏了"。
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(full).parent_path(), ec);

    std::ofstream out(full, std::ios::binary);
    out << content;
    if (!out.good())
        std::printf("  [警告] fixture 写入失败：%s\n", full.c_str());
}

int main()
{
    // 文案要能解析 @i18n/
    setLocale("zh-Hans");

    std::printf("\n== 1. 属性往返（JSON -> 视图 -> getter）==\n");
    {
        const char* json = R"({
          "type": "Box",
          "axis": "row",
          "padding": 8,
          "paddingLeft": 12,
          "cornerRadius": 6,
          "borderThickness": 2,
          "clipsToBounds": true,
          "backgroundColor": "#336699",
          "children": [
            { "type": "Box", "width": 100, "height": 40, "children": [
                { "type": "Label", "text": "@i18n/demo/welcome", "fontSize": 18,
                  "lineHeight": 1.5, "singleLine": true, "horizontalAlign": "center" }
            ]},
            { "type": "Image", "src": "@res/testcard.png", "scalingType": "fill", "cornerRadius": 4 }
          ]
        })";

        View* root = buildAndLayout(json, 800, 600);
        check(root != nullptr, "整棵树建立成功");
        if (root != nullptr)
        {
            Box* box = root->asBox();
            check(box != nullptr, "根是 Box");
            check(box != nullptr && box->getAxis() == Axis::ROW, "axis=row");
            checkFloat(box != nullptr ? box->getPaddingTop() : -1.0f, 8.0f, "padding=8 作为四边默认值");
            checkFloat(box != nullptr ? box->getPaddingLeft() : -1.0f, 12.0f, "paddingLeft=12 覆盖默认值");
            checkFloat(root->getCornerRadius(), 6.0f, "cornerRadius=6");
            checkFloat(root->getBorderThickness(), 2.0f, "borderThickness=2");
            check(root->getClipsToBounds(), "clipsToBounds=true");

            check(box != nullptr && box->getChildrenCount() == 2, "两个子节点");
            if (box != nullptr && box->getChildrenCount() == 2)
            {
                View* first = box->getChildren()[0];
                checkFloat(first->getWidth(), 100.0f, "第一个子节点 width=100（布局后）");
                checkFloat(first->getHeight(), 40.0f, "第一个子节点 height=40（布局后）");

                if (first->asBox() != nullptr && first->asBox()->getChildrenCount() == 1)
                {
                    Label* label = asLabel(first->asBox()->getChildren()[0]);
                    check(label != nullptr, "孙节点是 Label");
                    if (label != nullptr)
                    {
                        checkEqual(label->getText(), "欢迎使用 bklv", "@i18n/ 在实例化时解析");
                        checkFloat(label->getFontSize(), 18.0f, "fontSize=18");
                        checkFloat(label->getLineHeight(), 1.5f, "lineHeight=1.5");
                    }
                }
                else
                {
                    check(false, "第一个子节点的结构不对");
                }

                Image* image = asImage(box->getChildren()[1]);
                check(image != nullptr, "第二个子节点是 Image");
                if (image != nullptr)
                {
                    checkFloat(image->getImageCornerRadius(), 4.0f, "Image 的 cornerRadius=4");
                    check(image->getScalingType() == ImageScalingType::FILL, "scalingType=fill");
                }
            }
        }
    }

    std::printf("\n== 2. 尺寸：auto / 百分比 / px ==\n");
    {
        View* root = buildAndLayout(R"({"type":"Box","width":200,"height":100,"children":[
            {"type":"Box","width":"50%","height":"25%"},
            {"type":"Box","width":"auto","height":"30px"},
            {"type":"Box","width":40,"height":20}
        ]})", 200.0f, 100.0f);
        check(root != nullptr, "建立成功");
        if (root != nullptr && root->asBox() != nullptr && root->asBox()->getChildrenCount() == 3)
        {
            const std::vector<View*>& kids = root->asBox()->getChildren();
            checkFloat(kids[0]->getWidth(), 100.0f, "\"50%\" -> 父宽 200 的 50%");
            checkFloat(kids[0]->getHeight(), 25.0f, "\"25%\" -> 父高 100 的 25%");
            checkFloat(kids[2]->getWidth(), 40.0f, "裸数字 40");
            checkFloat(kids[2]->getHeight(), 20.0f, "裸数字 20");
            checkFloat(kids[1]->getHeight(), 30.0f, "\"30px\" 当作 30");
        }
        else
        {
            check(false, "结构不对");
        }
    }

    std::printf("\n== 3. 畸形输入：一律返回 nullptr 且有错误信息 ==\n");
    {
        const char* cases[] = {
            R"({"type":"Box",})",                                        // 语法错误
            R"({"type":"NoSuchView"})",                                  // 未知类型
            R"({"type":"Label","fontSize":"abc"})",                      // 类型不符
            R"({"type":"Box","axis":"diagonal"})",                       // 枚举越界
            R"({"type":"Box","children":{}})",                           // children 不是数组
            R"({"width":10})",                                           // 缺 type
            R"({"type":"Label","text":"x","children":[{"type":"Box"}]})", // 非容器带 children
        };
        const char* names[] = {
            "语法错误", "未知类型", "类型不符（fontSize=\"abc\"）", "枚举越界（axis=\"diagonal\"）",
            "children 不是数组", "缺 type", "非容器带 children",
        };

        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            View* view = build(cases[i]);
            check(view == nullptr, std::string("拒绝：") + names[i]);
            check(getLastLoadErrorCount() > 0, std::string("有错误记录：") + names[i]);
            delete view;
        }
    }

    std::printf("\n== 4. 超深嵌套会被拦下 ==\n");
    {
        std::string deep;
        for (int i = 0; i < 40; i++)
            deep += R"({"type":"Box","children":[)";
        deep += R"({"type":"Box"})";
        for (int i = 0; i < 40; i++)
            deep += "]}";

        check(build(deep) == nullptr, "40 层嵌套被拒绝");
    }

    std::printf("\n== 5. 未知属性：只警告，不中断 ==\n");
    {
        View* view = build(R"({"type":"Box","colo":"#ffffff","width":10})");
        check(view != nullptr, "未知属性不影响建树");
        check(getLastLoadErrorCount() > 0, "未知属性被记下来了");
        check(getLastLoadErrors().find("未知属性") != std::string::npos, "错误文本里提到未知属性");
        delete view;
    }

    std::printf("\n== 6. include 与循环检测 ==\n");
    {
        writeFixture("views/_test_inner.json", R"({"type":"Box","children":[
            {"type":"Label","text":"来自被引入的文件"}]})");

        writeFixture("views/_test_outer.json", R"({"type":"Box","padding":4,"children":[
            {"include":"views/_test_inner.json"},
            {"type":"Label","text":"外层"}]})");

        View* outer = createFromJSONFile("views/_test_outer.json");
        check(outer != nullptr, "include 正常展开");
        if (outer != nullptr && outer->asBox() != nullptr)
        {
            check(outer->asBox()->getChildrenCount() == 2, "引入的子树被接进来");
            if (outer->asBox()->getChildrenCount() == 2)
            {
                Box* inner = outer->asBox()->getChildren()[0]->asBox();
                check(inner != nullptr && inner->getChildrenCount() == 1, "被引入文件内部的子节点也在");
                if (inner != nullptr && inner->getChildrenCount() == 1)
                {
                    Label* label = asLabel(inner->getChildren()[0]);
                    check(label != nullptr && label->getText() == "来自被引入的文件",
                        "被引入文件的内容正确");
                }
            }
        }
        delete outer;

        writeFixture("views/_test_cycle_a.json", R"({"include":"views/_test_cycle_b.json"})");
        writeFixture("views/_test_cycle_b.json", R"({"include":"views/_test_cycle_a.json"})");

        View* cycle = createFromJSONFile("views/_test_cycle_a.json");
        check(cycle == nullptr, "循环 include 被拒绝（不栈溢出）");
        check(getLastLoadErrors().find("循环 include") != std::string::npos, "错误信息指出是循环 include");
        delete cycle;
    }

    std::printf("\n== 7. 模板复用：解析一次，实例化多次 ==\n");
    {
        writeFixture("views/_test_reuse.json", R"({"type":"Box","padding":2,"children":[
            {"type":"Label","text":"复用"}]})");

        clearTemplateCache();
        const int before = getTemplateParseCount();

        int okCount = 0;
        for (int i = 0; i < 50; i++)
        {
            View* view = createFromJSONFile("views/_test_reuse.json");
            if (view != nullptr)
                okCount++;
            delete view;
        }
        check(okCount == 50, "50 次实例化全部成功");
        checkEqual(std::to_string(getTemplateParseCount() - before), "1", "50 次实例化只解析 1 次");

        // 负缓存：反复加载不存在的文件也只读一次盘
        const int afterFirst = getTemplateParseCount();
        int nullCount        = 0;
        for (int i = 0; i < 10; i++)
        {
            View* missing = createFromJSONFile("views/_test_missing.json");
            if (missing == nullptr)
                nullCount++;
            delete missing;
        }
        check(nullCount == 10, "不存在的文件每次都返回 nullptr");
        checkEqual(std::to_string(getTemplateParseCount() - afterFirst), "0", "失败结果被负缓存（不重复解析）");
        check(getTemplateCacheSize() > 0, "缓存里有条目");
    }

    std::printf("\n== 8. 模板与语言无关（同一个模板切语言后重实例化）==\n");
    {
        std::shared_ptr<const ViewTemplate> tmpl = ViewTemplate::fromString(
            R"({"type":"Label","text":"@i18n/demo/welcome"})", "<string>");
        check(tmpl != nullptr, "模板装载成功");

        setLocale("zh-Hans");
        View* zh = tmpl->instantiate();
        Label* zhLabel = asLabel(zh);
        checkEqual(zhLabel != nullptr ? zhLabel->getText() : "<null>", "欢迎使用 bklv",
            "模板实例化用当前语言（中文）");

        setLocale("en-US");
        View* en = tmpl->instantiate();
        Label* enLabel = asLabel(en);
        checkEqual(enLabel != nullptr ? enLabel->getText() : "<null>", "Welcome to bklv",
            "同一个模板切语言后重实例化拿到英文（缓存没烤死语言）");

        delete zh;
        delete en;
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
