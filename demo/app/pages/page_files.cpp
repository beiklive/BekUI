// 文件浏览器：RecyclerFrame + 自定义行 cell，跨平台根路径 / 进入目录 / 返回上级。
//
// 根路径由 core/file_system 按平台给出（Switch: sdmc:/ 与 romfs:/；桌面: 主目录 + 当前目录）。

#include "app/pages.hpp"
#include "app/pages/showcase.hpp"

#include <functional>
#include <string>
#include <vector>

#include "bklv/core/file_system.hpp"
#include "bklv/core/icon_font.hpp"
#include "bklv/views/label.hpp"
#include "bklv/views/recycler.hpp"
#include "bklv/widgets/appearance_view.hpp"
#include "bklv/widgets/content_ids.hpp"

using namespace bklv;

namespace
{

std::string humanSize(uint64_t bytes)
{
    if (bytes >= 1024ull * 1024ull * 1024ull)
        return std::to_string(bytes / (1024ull * 1024ull * 1024ull)) + " GB";
    if (bytes >= 1024ull * 1024ull)
        return std::to_string(bytes / (1024ull * 1024ull)) + " MB";
    if (bytes >= 1024ull)
        return std::to_string(bytes / 1024ull) + " KB";
    return std::to_string(bytes) + " B";
}

} // namespace

namespace demo
{

namespace
{

class FileBrowserSource : public RecyclerDataSource
{
  public:
    std::string path;
    std::string error;
    std::vector<fsx::Entry> entries;
    bool rootsMode = true;

    void showRoots()
    {
        rootsMode = true;
        path.clear();
        entries.clear();
        for (const fsx::Root& root : fsx::roots())
        {
            fsx::Entry entry;
            entry.name      = root.label;
            entry.path      = root.path;
            entry.directory = true;
            entries.push_back(entry);
        }
    }

    bool openDirectory(const std::string& directory)
    {
        std::vector<fsx::Entry> listed;
        std::string message;
        if (!fsx::list(directory, listed, message))
        {
            error = message;
            return false;
        }
        error.clear();
        path      = directory;
        entries   = std::move(listed);
        rootsMode = false;
        return true;
    }

    bool goUp()
    {
        if (rootsMode)
            return false;

        const std::string up = fsx::parent(path);
        if (up.empty())
        {
            showRoots();
            return true;
        }
        return openDirectory(up);
    }

    size_t getItemCount() override { return entries.size(); }
    std::string getItemIdentifier(size_t) override { return "file"; }
};

class FileRow : public AppearanceView
{
  public:
    FileRow()
        : AppearanceView("demo/file_cell", true)
    {
        this->onPointerAction = true;
    }
};

class FileCell : public RecyclerCell
{
  public:
    FileCell(FileBrowserSource* source, std::function<void(size_t)> onOpen)
        : source(source)
        , onOpen(std::move(onOpen))
    {
        this->row = new FileRow();
        this->row->setGrow(1.0f);
        this->row->setWidthPercentage(100.0f);
        this->row->setHeightPercentage(100.0f);
        this->row->registerAction(ControllerButton::A, "打开", [this](View*) {
            if (this->onOpen)
                this->onOpen(this->index);
        });
        this->addView(this->row);
    }

    void setSectionIndex(size_t, size_t index) override
    {
        this->index = index;
        this->refresh();
    }

    bool isFocusable() const override { return false; } // 焦点给内部 row（能拿到 hlBg 状态）

  private:
    void refresh()
    {
        if (this->source == nullptr || this->index >= this->source->entries.size())
            return;

        const fsx::Entry& entry = this->source->entries[this->index];
        this->row->setIcon(content::ICON,
            bklv::icon::glyphByName(entry.directory ? "folder_open" : "description"));
        this->row->setText("title", entry.name);
        this->row->setText(content::INFO, entry.directory ? "文件夹" : humanSize(entry.size));
    }

    FileBrowserSource* source = nullptr;
    std::function<void(size_t)> onOpen;
    FileRow* row = nullptr;
    size_t index = 0;
};

} // namespace

Box* buildFilesPage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setGap(getTheme().number("metric.spacing.md", 12.0f));

    FileBrowserSource* source = new FileBrowserSource();
    source->showRoots();

    RecyclerFrame* frame = new RecyclerFrame();
    frame->setWidthPercentage(100.0f);
    frame->setHeight(460.0f);
    frame->setMinHeight(460.0f);
    frame->setShrink(0.0f);
    frame->setFlexBasis(460.0f);
    frame->setItemHeight(56.0f);
    frame->setDataSource(source);
    frame->reloadData();

    Label* pathLabel = new Label();
    pathLabel->setGrow(1.0f);
    pathLabel->setFontSize(15.0f);
    pathLabel->setTextColor(getTheme().color("color.text.dim"));

    Label* statusLabel = new Label();
    statusLabel->setFontSize(13.0f);
    statusLabel->setTextColor(getTheme().color("color.common.red"));

    Button* up = new Button("上级");
    up->setIcon(bklv::icon::glyphByName("menu"));

    auto refreshHeader = [source, pathLabel, statusLabel, up] {
        pathLabel->setText(source->rootsMode ? "选择根目录" : source->path);
        statusLabel->setText(source->error);
        up->setEnabled(!source->rootsMode);
        if (!source->rootsMode)
        {
            // 已经在文件系统的根（parent 为空）时，返回上级会回到根列表
            up->setEnabled(true);
        }
    };

    auto openIndex = [source, frame, refreshHeader](size_t index) {
        if (index >= source->entries.size())
            return;
        const fsx::Entry entry = source->entries[index];
        if (entry.directory)
        {
            source->openDirectory(entry.path);
            frame->reloadData();
            refreshHeader();
        }
        else
        {
            toastManager()->showInfo("文件：" + entry.name);
        }
    };

    frame->registerCell("file", [source, openIndex] {
        return new FileCell(source, openIndex);
    });

    up->setOnClick([source, frame, refreshHeader] {
        source->goUp();
        frame->reloadData();
        refreshHeader();
    });

    Box* header = new Box(Axis::ROW);
    header->setWidthPercentage(100.0f);
    header->setGap(getTheme().number("metric.spacing.sm", 8.0f));
    header->setAlignItems(AlignItems::CENTER);
    header->addView(up);
    header->addView(pathLabel);

    page->addView(header);
    page->addView(statusLabel);
    page->addView(frame);

    refreshHeader();
    frame->reloadData();
    return page;
}

} // namespace demo
