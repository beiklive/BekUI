#include "bklv/widgets/section.hpp"

#include "bklv/widgets/appearance_view.hpp"
#include "bklv/widgets/content_ids.hpp"

namespace bklv
{

AppearanceView* makeSectionHeader(const std::string& title, const std::string& info,
    const std::string& appearance)
{
    AppearanceView* section = new AppearanceView(appearance, false);
    section->setText(content::TEXT, title);
    if (!info.empty())
        section->setText(content::INFO, info);
    return section;
}

} // namespace bklv
