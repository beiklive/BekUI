#include "app/pages.hpp"

#include <string>
#include <vector>

#include "app/ui.hpp"
#include "bklv/bklv.hpp"
#include "bklv/widgets/appearance_view.hpp"
#include "bklv/views/item_binder.hpp"
#include "bklv/views/data_source.hpp"

using namespace bklv;

namespace demo
{

namespace
{

// 一个"图鉴条目"。
//
// 素材说明：borealis demo 用的是宝可梦精灵图（任天堂 IP，不能随包分发）。
// 这里改成**自绘占位**（类型色圆 + 名称首字）+ **自造数据**，
// 演示的是"分段回收列表 + 详情页导航"这套能力本身。
struct DexEntry
{
    std::string name;
    std::string type;
    int hp    = 0;
    int atk   = 0;
    int def   = 0;
    std::string description;
};

const std::vector<DexEntry>& starters()
{
    static const std::vector<DexEntry> entries = {
        { "草芽兽", "草", 45, 49, 49, "背上的嫩芽会随心情张开；晒太阳时长得最快。" },
        { "火尾兽", "火", 39, 52, 43, "尾巴的火焰是体温计，睡着时会变小。" },
        { "水泡兽", "水", 44, 48, 65, "喜欢待在浅水里，用泡泡把食物包起来带走。" },
    };
    return entries;
}

const std::vector<DexEntry>& wildOnes()
{
    static const std::vector<DexEntry> entries = {
        { "电纹鼠", "电", 35, 55, 40, "脸颊的纹路会积电，跑起来会噼啪响。" },
        { "石甲虫", "岩", 50, 40, 70, "受到攻击就缩成石头，硬得连钻头都发愁。" },
        { "风羽鸟", "风", 40, 45, 40, "顺风时能一口气飞过整片林子。" },
        { "霜角兽", "冰", 55, 45, 55, "角上的霜越厚，说明它心情越平静。" },
        { "焰甲蜥", "火", 58, 64, 58, "晒太阳时背甲会变红，是它最强的状态。" },
        { "潮汐贝", "水", 60, 40, 75, "涨潮时张开，退潮时闭上，靠潮水认时间。" },
        { "雷云雀", "电", 45, 60, 35, "成群飞过时会带起一小片雷雨。" },
        { "苔藓羊", "草", 65, 38, 60, "毛里长着苔藓，摸起来总是潮潮的。" },
        { "影砂蛇", "地", 48, 58, 45, "藏在沙下只露眼睛，等风把猎物吹过来。" },
    };
    return entries;
}

// 类型 -> 主题色 key
const char* typeColorKey(const std::string& type)
{
    if (type == "火")
        return "color.pokedex.typeFire";
    if (type == "水")
        return "color.pokedex.typeWater";
    return "color.pokedex.typeGrass"; // 草/岩/风/冰/电 等都归到主色，演示够用
}

const std::vector<DexEntry>& entriesOfSection(size_t section)
{
    return section == 0 ? starters() : wildOnes();
}

// ---------------------------------------------------------------- 段头

class DexHeaderCell : public RecyclerSectionHeader
{
  public:
    DexHeaderCell()
    {
        this->setHeight(getTheme().number("metric.list.sectionHeaderHeight", 36.0f));
        this->setAlignItems(AlignItems::CENTER);
        this->setGap(getTheme().number("metric.spacing.sm", 8.0f));

        this->accent = new Rectangle();
        this->accent->setWidth(4.0f);
        this->accent->setHeight(18.0f);
        this->accent->setCornerRadius(2.0f);
        this->accent->setBackgroundColor(getTheme().color("color.accent.base"));
        this->addView(this->accent);

        this->title = makeText("", getTheme().number("metric.font.size.md", 18.0f),
            getTheme().color("color.text.primary"), true);
        this->addView(this->title);
    }

    bool isFocusable() const override { return false; } // 段头不吃焦点

    void onSectionTitle(size_t, const std::string& text) override
    {
        if (this->title != nullptr)
            this->title->setText(text);
    }

  private:
    Rectangle* accent = nullptr;
    Label* title      = nullptr;
};

// ---------------------------------------------------------------- 条目

class DexCell : public RecyclerCell
{
  public:
    DexCell()
    {
        this->setAlignItems(AlignItems::CENTER);
        this->setGap(getTheme().number("metric.spacing.md", 12.0f));
        this->setPadding(getTheme().number("metric.spacing.sm", 8.0f));
        this->setCornerRadius(getTheme().number("metric.radius.md", 8.0f));

        // 自绘头像占位：类型色圆 + 名称首字
        this->avatar = new Rectangle();
        this->avatar->setWidth(48.0f);
        this->avatar->setHeight(48.0f);
        this->avatar->setCornerRadius(24.0f);
        this->avatar->setJustifyContent(JustifyContent::CENTER);
        this->avatar->setAlignItems(AlignItems::CENTER);
        this->avatarText = makeText("", getTheme().number("metric.font.size.md", 18.0f),
            getTheme().color("color.text.primary"), true);
        this->avatar->addView(this->avatarText);
        this->addView(this->avatar);

        Box* column = new Box(Axis::COLUMN);
        column->setGrow(1.0f);
        column->setGap(2.0f);
        this->name = makeText("", getTheme().number("metric.font.size.md", 18.0f),
            getTheme().color("color.text.primary"), true);
        column->addView(this->name);
        this->type = makeText("", getTheme().number("metric.font.size.sm", 14.0f),
            getTheme().color("color.text.dim"), true);
        column->addView(this->type);
        this->addView(column);

        // 右侧一个能力条，让行看起来有信息量
        this->barTrack = new Rectangle();
        this->barTrack->setWidth(120.0f);
        this->barTrack->setHeight(getTheme().number("metric.pokedex.statBarHeight", 10.0f));
        this->barTrack->setCornerRadius(5.0f);
        this->barTrack->setBackgroundColor(getTheme().color("color.pokedex.statTrack"));
        this->barFill = new Rectangle();
        this->barFill->setHeight(getTheme().number("metric.pokedex.statBarHeight", 10.0f));
        this->barFill->setCornerRadius(5.0f);
        this->barTrack->addView(this->barFill);
        this->addView(this->barTrack);

        this->applyIdleStyle();
    }

    bool isFocusable() const override { return true; }

    void onFocusGained() override { this->applyFocusStyle(); }
    void onFocusLost() override { this->applyIdleStyle(); }

    void setSectionIndex(size_t section, size_t index) override
    {
        RecyclerCell::setSectionIndex(section, index);

        const std::vector<DexEntry>& list = entriesOfSection(section);
        if (index >= list.size())
            return;

        const DexEntry& entry = list[index];
        const ImU32 color     = getTheme().color(typeColorKey(entry.type));

        this->name->setText(entry.name);
        this->type->setText(entry.type + "   HP " + std::to_string(entry.hp) + " / 攻 "
            + std::to_string(entry.atk) + " / 防 " + std::to_string(entry.def));
        this->avatar->setBackgroundColor(color);

        // 首字（UTF-8 第一个字符）
        this->avatarText->setText(entry.name.substr(0, 3));

        const float ratio = std::min(1.0f, (float)entry.atk / 100.0f);
        this->barFill->setWidth(120.0f * ratio);
        this->barFill->setBackgroundColor(color);
    }

  private:
    void applyIdleStyle()
    {
        this->setBackgroundColor(getTheme().color("color.surface.card"));
    }

    void applyFocusStyle()
    {
        this->setBackgroundColor(getTheme().color("color.highlight.background"));
    }

    Rectangle* avatar   = nullptr;
    Label* avatarText   = nullptr;
    Label* name         = nullptr;
    Label* type         = nullptr;
    Rectangle* barTrack = nullptr;
    Rectangle* barFill  = nullptr;
};

// ---------------------------------------------------------------- 数据源

class DexSource : public RecyclerDataSource
{
  public:
    size_t getSectionCount() override { return 2; }

    size_t getItemCountInSection(size_t section) override { return entriesOfSection(section).size(); }

    std::string getSectionTitle(size_t section) override
    {
        return getStr(section == 0 ? "pokedex/section_starters" : "pokedex/section_wild");
    }

    std::string getItemIdentifierInSection(size_t section, size_t) override
    {
        return section == 0 ? "starter" : "wild";
    }
};

} // namespace

// 详情页：内容栈上推一层（B / 返回键由 AppletFrame 负责弹回）
View* buildPokedexDetail(size_t section, size_t index)
{
    const std::vector<DexEntry>& list = entriesOfSection(section);
    if (index >= list.size())
        return nullptr;

    const DexEntry& entry = list[index];
    const ImU32 color     = getTheme().color(typeColorKey(entry.type));

    Box* page = makeColumn(getTheme().number("metric.spacing.md", 12.0f));
    page->addView(makeHint(getStr("pokedex/detail_hint")));

    Rectangle* card = makeCard();
    card->addView(makeText(entry.name, getTheme().number("metric.font.size.lg", 24.0f),
        getTheme().color("color.text.primary"), true));

    Box* head = makeRow(getTheme().number("metric.spacing.md", 12.0f));
    head->setAlignItems(AlignItems::CENTER);

    const float avatarSize = getTheme().number("metric.pokedex.detailAvatarSize", 120.0f);
    Rectangle* avatar      = new Rectangle();
    avatar->setWidth(avatarSize);
    avatar->setHeight(avatarSize);
    avatar->setCornerRadius(avatarSize * 0.5f);
    avatar->setBackgroundColor(color);
    avatar->setJustifyContent(JustifyContent::CENTER);
    avatar->setAlignItems(AlignItems::CENTER);
    avatar->addView(makeText(entry.name.substr(0, 3),
        getTheme().number("metric.font.size.lg", 24.0f), getTheme().color("color.text.primary"), true));
    head->addView(avatar);

    Box* stats = makeColumn(getTheme().number("metric.pokedex.statRowGap", 6.0f));
    stats->setGrow(1.0f);
    const std::pair<const char*, int> rows[] = { { "pokedex/stat_hp", entry.hp },
        { "pokedex/stat_atk", entry.atk }, { "pokedex/stat_def", entry.def } };
    for (const auto& row : rows)
    {
        Box* line = makeRow(getTheme().number("metric.spacing.sm", 8.0f));
        Label* label = makeText(getStr(row.first), getTheme().number("metric.font.size.sm", 14.0f),
            getTheme().color("color.text.dim"), true);
        label->setWidth(70.0f);
        line->addView(label);

        Rectangle* track = new Rectangle();
        track->setGrow(1.0f);
        track->setHeight(getTheme().number("metric.pokedex.statBarHeight", 10.0f));
        track->setCornerRadius(5.0f);
        track->setBackgroundColor(getTheme().color("color.pokedex.statTrack"));
        Rectangle* fill = new Rectangle();
        fill->setWidthPercentage(std::min(100.0f, (float)row.second));
        fill->setHeight(getTheme().number("metric.pokedex.statBarHeight", 10.0f));
        fill->setCornerRadius(5.0f);
        fill->setBackgroundColor(color);
        track->addView(fill);
        line->addView(track);

        Label* value = makeText(std::to_string(row.second),
            getTheme().number("metric.font.size.sm", 14.0f),
            getTheme().color("color.text.primary"), true);
        value->setWidth(40.0f);
        value->setHorizontalAlign(HorizontalAlign::RIGHT);
        line->addView(value);
        stats->addView(line);
    }
    head->addView(stats);
    card->addView(head);

    card->addView(makeText(entry.description, getTheme().number("metric.font.size.sm", 14.0f),
        getTheme().color("color.text.dim")));
    page->addView(card);

    return page;
}

// 详情页目标外壳：demo 里就是那个 Shell（页头 + 底栏 + 内容层的下钻栈），
// 从宿主下发的 PageDeps 里取（页面本身不持有外壳类型）。
static Shell* detailShell()
{
    return getPageDeps().shell;
}

Box* buildPokedexPage()
{
    Box* page = makeColumn(getTheme().number("metric.spacing.sm", 8.0f));
    // 和列表页一样：页面撑满内容区，自身不再套一层滚动（RecyclerFrame 就是滚动器）
    page->setWidthPercentage(100.0f);
    page->setHeightPercentage(100.0f);
    page->addView(makeHint(getStr("pokedex/hint")));

    // 数据源要活到列表销毁
    static DexSource source;

    // 列表整块交给 **JSON 描述**（容器 + 一份 item 模板 + 数据源名字 + 绑定函数名字）
    // 行为（点 A 下钻）在 demo 注册的绑定函数里，页面不再感知 item 的具体类型
    bklv::View* list = bklv::createViewFromTheme("page_pokedex_json");

    // 选中条目 → 下钻一层（B 返回：Shell 的 onBackRequested → popContentView）

    page->addView(list);
    return page;
}

} // namespace demo

namespace demo
{

// 把图鉴的数据源注册进"名字 -> 工厂"表：JSON 里就能写 "data": "@data/pokedex"
void registerPokedexDataSource()
{
    std::string error;
    if (!bklv::data::registerSource("pokedex",
            [] { return (bklv::RecyclerDataSource*)new DexSource(); }, error))
        BKLV_LOG_WARNING("pokedex 数据源注册失败：%s", error.c_str());
}

} // namespace demo

namespace demo
{

namespace
{

// 单段版（图鉴原有的"starter / wild 分段"目前不在 Recycler 的 JSON 表达里，见 13.1 的记录）：
// JSON 只描述容器与 item 组织方式，这里提供"第 N 项是谁"。
class FlatDexSource : public bklv::RecyclerDataSource
{
  public:
    // 「分页」演示：初始一条不给，滚动到底时由回收器自动要下一页
    size_t getItemCount() override { return this->visible; }
    size_t getLoadedCount() override { return this->visible; }
    bool hasMore() override { return this->visible < starters().size(); }
    float getItemHeight(size_t) override { return 70.0f; }

    void requestMore(size_t want, size_t from) override
    {
        (void)from;
        if (want == 0)
            want = 2;
        this->visible += want;
        if (this->visible > starters().size())
            this->visible = starters().size();
        // demo 同步模拟"数据到了"；真实项目里在 task::run* 的 done 回调里调它
        if (this->listener)
            this->listener();
    }

    void setLoadListener(std::function<void()> value) override { this->listener = value; }

  private:
    size_t visible = 0;
    std::function<void()> listener;
};

} // namespace

// 注册 JSON 用得到的两样东西：数据源名字 + item 绑定函数名字
void registerPokedexJsonBindings()
{
    std::string error;
    if (!bklv::data::registerSource("pokedex_flat",
            [] { return (bklv::RecyclerDataSource*)new FlatDexSource(); }, error))
        BKLV_LOG_WARNING("pokedex_flat 数据源注册失败：%s", error.c_str());

    if (!bklv::binding::registerItemBinder("pokedexCell",
            [](bklv::View& item, size_t index) {
                const std::vector<DexEntry>& list = starters();
                if (index >= list.size())
                    return;
                // JSON 的 item 是用 itemAppearance 建的 -> 就是一个 AppearanceView
                if (auto* view = static_cast<bklv::AppearanceView*>(&item))
                    view->setText(list[index].name);

                // 行为放进绑定函数（不依赖 item 是哪个 C++ 类）：只用 index 就能下钻
                item.registerAction(bklv::ControllerButton::A, bklv::getStr("pokedex/open"),
                    [index](bklv::View*) {
                        bklv::View* detail = buildPokedexDetail(0, index);
                        if (detail == nullptr)
                            return false;
                        if (bklv::Shell* shell = detailShell())
                        {
                            shell->pushContentView(detail);
                            return true;
                        }
                        return false;
                    });
            },
            error))
        BKLV_LOG_WARNING("pokedexCell 绑定函数注册失败：%s", error.c_str());
}

} // namespace demo
