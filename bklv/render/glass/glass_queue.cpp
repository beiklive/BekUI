#include "bklv/render/glass/glass_queue.hpp"

namespace bklv::glass
{

void GlassRequestQueue::beginFrame(GlassBackdropStyle style, float frameDensity)
{
    this->requests.clear();
    this->backdropStyle = style;
    this->density       = frameDensity > 0.0f ? frameDensity : 1.0f;
    this->sealed        = false;
}

void GlassRequestQueue::submit(GlassRequest request)
{
    if (this->sealed)
        return;
    this->requests.push_back(request);
}

void GlassRequestQueue::seal()
{
    this->sealed = true;
}

} // namespace bklv::glass
