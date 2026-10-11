#include "bklv/core/ime.hpp"

#include "bklv/core/logger.hpp"

namespace bklv
{

namespace
{

InlineImeManager g_inlineIme;
ImeManager* g_ime = &g_inlineIme;

} // namespace

ImeManager& getImeManager()
{
    return *g_ime;
}

void setImeManager(ImeManager* manager)
{
    g_ime = (manager != nullptr) ? manager : &g_inlineIme;
}

bool hasPlatformIme()
{
    return g_ime != &g_inlineIme;
}

} // namespace bklv
