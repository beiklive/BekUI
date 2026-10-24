#include "bklv/foundation/core/services.hpp"

#include <fstream>

#include "bklv/foundation/core/logger.hpp"
#include "bklv/foundation/core/resource.hpp"

namespace bklv
{

namespace
{
Services g_services;
}

void setServices(const Services& services)
{
    g_services = services;
    setResourceProvider(services.resources);
}

const Services& getServices()
{
    return g_services;
}

void clearServices()
{
    g_services = Services{};
    setResourceProvider(nullptr);
}

bool ResourceProvider::read(const std::string& relative, std::vector<unsigned char>& out,
    std::string& error) const
{
    out.clear();
    error.clear();

    const std::string path = this->resolve(relative);
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        error = "资源打不开：" + relative + "（" + path + "）";
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

bool ResourceProvider::hasResource(const std::string& relative) const
{
    std::ifstream file(this->resolve(relative), std::ios::binary);
    return file.is_open();
}

std::string DefaultResourceProvider::resolve(const std::string& relative) const
{
    return defaultResourcePath(relative);
}

} // namespace bklv
