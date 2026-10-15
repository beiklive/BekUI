#include <memory>
#include <string>

#include "app/pages.hpp"

#include "app/ui.hpp"
#include "bklv/bklv.hpp"

using namespace bklv;

namespace demo
{

// JSON 子页面：**逐层追加**地把视图描述 JSON 的能力演示一遍。
//
// 每一步是一节：「步骤 header」+「本步新追加的片段」+「渲染结果」。
// 关键在于**每步的 JSON 都是上一步加上一小段** —— 所以看到的是"加一行就多一个能力"，
// 而不是一堆互不相干的例子。渲染结果不是截图，就是这份 JSON 现场建出来的视图
//（`createFromJSONString`：按内存里的文本装载 + 实例化）。

namespace
{

// 固定几行的数据源：最后一节的重点是"模板来自 JSON + 绑定填值"
constexpr size_t JSON_ROW_COUNT = 8;

class JsonListSource : public RecyclerDataSource
{
  public:
    size_t getItemCount() override { return JSON_ROW_COUNT; }
    float getItemHeight(size_t) override
    {
        return getTheme().number("metric.list.itemHeight", 64.0f);
    }
};

// 绑定回调：JSON 只声明字段名，取值在 C++ 这边给；换数据源不用改 JSON
std::string binderFor(const std::string& field, size_t index)
{
    const int row = (int)index + 1;

    if (field == "thumb")
        return "resources/testcard.png";
    if (field == "title")
        return getStr("json/row_title", row);
    if (field == "subtitle")
        return getStr("json/row_subtitle", row);

    return {};
}

// 步骤 header：一张卡片 + 大一号的标题（和基础控件页的小节标题同一套观感）
Rectangle* makeStepHeader(const char* titleKey)
{
    Rectangle* card = makeCard();
    card->addView(makeText(getStr(titleKey), getTheme().number("metric.font.size.md", 18.0f),
        getTheme().color("color.accent.base", 0)));
    return card;
}

// 片段文本按列宽硬换行：Label 只在空格处折行，长片段不换会把整页撑宽
//（实测：卡片右侧被切掉）。这里自己折，稳定可预期。
std::string wrapSnippet(const std::string& text, size_t maxColumns = 46)
{
    std::string out;
    size_t lineLength = 0;
    size_t lastBreak  = std::string::npos;

    for (size_t i = 0; i < text.size(); i++)
    {
        const char c = text[i];
        if (c == '\n')
        {
            out += c;
            lineLength = 0;
            lastBreak  = std::string::npos;
            continue;
        }
        out += c;
        lineLength++;
        if (c == ' ' || c == ',' )
            lastBreak = out.size();
        if (lineLength >= maxColumns)
        {
            // 优先在最近的空格/逗号处折，找不到就硬折
            if (lastBreak != std::string::npos)
            {
                out.insert(lastBreak, "\n");
                lineLength = out.size() - lastBreak - 1;
            }
            else
            {
                out += '\n';
                lineLength = 0;
            }
            lastBreak = std::string::npos;
        }
    }
    return out;
}

// 「本步追加的片段」：小一号、次色，允许换行 —— 真机上也能读
Label* makeSnippet(const std::string& json)
{
    Label* label = makeText(wrapSnippet(json), getTheme().number("metric.font.size.sm", 15.0f),
        getTheme().color("color.text.dim"), false);
    label->setLineHeight(1.25f);
    return label;
}

// 一步：header + 新增片段 + 说明 + 渲染结果
struct JsonStep
{
    const char* titleKey;
    const char* noteKey;
    const char* added; // 本步**新追加**的那几行（给人看"加的是什么"）
    const char* json;  // 累积到本步为止的完整 JSON（拿去实例化）
};

// 每一步 = 上一步 + 一小段。整页就是这条阶梯。
const JsonStep kSteps[] = {
    {
        "json/step1_title",
        "json/step1_note",
        R"({ "type": "Label", "text": "Hello, JSON" })",
        R"({ "type": "Label", "text": "Hello, JSON" })",
    },
    {
        "json/step2_title",
        "json/step2_note",
        R"("type": "Box", "axis": "row", "gap": 10, "children": [ … ])",
        R"({
            "type": "Box", "axis": "row", "gap": 10,
            "children": [
                { "type": "Label", "text": "Hello, JSON" },
                { "type": "Label", "text": "第二个兄弟节点" }
            ]
        })",
    },
    {
        "json/step3_title",
        "json/step3_note",
        R"("padding": 12, "cornerRadius": 10,
  "backgroundColor": "#252a33", "grow": 1)",
        R"({
            "type": "Box", "axis": "row", "gap": 10,
            "padding": 12, "cornerRadius": 10, "backgroundColor": "#252a33",
            "children": [
                { "type": "Label", "text": "Hello, JSON" },
                { "type": "Label", "text": "第二个兄弟节点" }
            ]
        })",
    },
    {
        "json/step4_title",
        "json/step4_note",
        R"("backgroundColor": "@theme/color.surface.card", "width": "100%",
  "textColor": "@theme/color.accent.base")",
        R"({
            "type": "Box", "axis": "row", "gap": 10, "grow": 1,
            "padding": 12, "width": "100%",
            "cornerRadius": "@style/metric.component.radius",
            "backgroundColor": "@theme/color.surface.card",
            "children": [
                { "type": "Label", "text": "Hello, JSON" },
                { "type": "Label", "text": "主题色", "textColor": "@theme/color.accent.base" }
            ]
        })",
    },
    {
        "json/step5_title",
        "json/step5_note",
        R"({ "type": "Image", "src": "@res/testcard.png",
  "width": 96, "height": 54, "scalingType": "fill" })",
        R"({
            "type": "Box", "axis": "row", "gap": 12,
            "padding": 12, "width": "100%",
            "cornerRadius": "@style/metric.component.radius",
            "backgroundColor": "@theme/color.surface.card",
            "children": [
                { "type": "Image", "src": "@res/testcard.png",
                  "width": 96, "height": 54, "cornerRadius": 8, "scalingType": "fill" },
                { "type": "Label", "text": "图片来自 @res/ 资源引用" }
            ]
        })",
    },
    {
        "json/step6_title",
        "json/step6_note",
        R"("text": "@i18n/json/row_title",
  "fontSize": "@style/metric.font.size.md", "singleLine": true)",
        R"({
            "type": "Box", "axis": "row", "gap": 12,
            "padding": 12, "width": "100%",
            "cornerRadius": "@style/metric.component.radius",
            "backgroundColor": "@theme/color.surface.card",
            "children": [
                { "type": "Image", "src": "@res/testcard.png",
                  "width": 96, "height": 54, "cornerRadius": 8, "scalingType": "fill" },
                { "type": "Box", "axis": "column", "gap": 4, "grow": 1,
                  "children": [
                    { "type": "Label", "text": "@i18n/json/row_title",
                      "fontSize": "@style/metric.font.size.md", "singleLine": true },
                    { "type": "Label", "text": "@i18n/json/row_subtitle",
                      "textColor": "@theme/color.text.dim", "singleLine": true }
                  ] }
            ]
        })",
    },
};

} // namespace

Box* buildJsonPage()
{
    Box* page = makeColumn(getTheme().number("metric.spacing.sm", 8.0f));

    page->addView(makeHint(getStr("json/intro")));

    // ---- 第 1~6 步：逐层追加（每步的 JSON 都是上一步加一小段）----
    for (const JsonStep& step : kSteps)
    {
        page->addView(makeStepHeader(step.titleKey));
        page->addView(makeSnippet(step.added));
        page->addView(makeHint(getStr(step.noteKey)));

        View* rendered = createFromJSONString(step.json);
        if (rendered != nullptr)
            page->addView(rendered);
        else
            page->addView(makeHint(getStr("json/failed")));
    }

    // ---- 第 7 步：拆文件（include）----
    // include 的路径是**相对 resources 根**解析的（`resourcePath(raw)`），
    // 所以这里写全路径；主文件里就一句 include，内容在另一份文件里。
    {
        page->addView(makeStepHeader("json/step7_title"));
        page->addView(makeSnippet(
            R"({ "type": "Box", "include": "resources/theme/default/view/json_include_part.json" })"));
        page->addView(makeHint(getStr("json/step7_note")));

        View* rendered = createViewFromTheme("json_include_main");
        if (rendered != nullptr)
            page->addView(rendered);
        else
            page->addView(makeHint(getStr("json/failed")));
    }

    // ---- 第 8 步：模板 + 数据绑定（同一份模板实例化很多次）----
    {
        page->addView(makeStepHeader("json/step8_title"));
        page->addView(makeSnippet(
            R"({ "type": "Image", "bind": "thumb" }, { "type": "Label", "bind": "title" })"));
        page->addView(makeHint(getStr("json/step8_note")));

        RecyclerFrame* list = new RecyclerFrame();
        list->setWidthPercentage(100.0f);
        list->setHeight(280.0f);
        list->setItemHeight(getTheme().number("metric.list.itemHeight", 64.0f));
        list->setShowScrollingIndicator(true);

        std::shared_ptr<const ViewTemplate> itemTemplate
            = loadViewTemplateFromTheme("list_item");
        if (itemTemplate != nullptr)
        {
            list->registerCell(RecyclerDataSource::DEFAULT_CELL_ID,
                [itemTemplate] { return JSONRecyclerCell::create(itemTemplate, binderFor); });
        }

        static JsonListSource source;
        list->setDataSource(&source);
        page->addView(list);
    }

    return page;
}

} // namespace demo
