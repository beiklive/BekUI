#include "designer/core/appearance_doc.hpp"

#include <algorithm>

namespace designer
{

using designer::Json;

std::vector<LayerInfo> AppearanceDoc::getLayers() const
{
    std::vector<LayerInfo> out;
    const Json* layers = this->get("/layers");
    if (layers == nullptr || !layers->is_array())
        return out;

    for (size_t i = 0; i < layers->size(); i++)
    {
        const Json& layer = (*layers)[i];
        LayerInfo info;
        info.index = i;
        if (layer.is_object())
        {
            if (layer.contains("id") && layer["id"].is_string())
                info.id = layer["id"].get<std::string>();
            if (layer.contains("op") && layer["op"].is_string())
                info.op = layer["op"].get<std::string>();
        }
        out.push_back(std::move(info));
    }
    return out;
}

int AppearanceDoc::findLayer(const std::string& id) const
{
    const std::vector<LayerInfo> layers = this->getLayers();
    for (const LayerInfo& info : layers)
        if (info.id == id)
            return (int)info.index;
    return -1;
}

bool AppearanceDoc::layerPointer(const std::string& id, std::string& outPointer) const
{
    const int index = this->findLayer(id);
    if (index < 0)
        return false;
    outPointer = "/layers/" + std::to_string(index);
    return true;
}

bool AppearanceDoc::addLayer(const std::string& op, std::string& outId, std::string& error,
    const std::string& id)
{
    const designer::Json* layersConst = this->get("/layers");
    if (layersConst == nullptr || !layersConst->is_array())
    {
        error = "这份外观没有 layers 数组（文件本身可能不合法）";
        return false;
    }
    designer::Json layers = *layersConst;

    std::string wanted = id;
    if (wanted.empty())
    {
        int n = 0;
        do
        {
            wanted = op + std::to_string(n++);
        } while (this->findLayer(wanted) >= 0);
    }
    else if (this->findLayer(wanted) >= 0)
    {
        error = "层 id 已存在：" + wanted;
        return false;
    }

    designer::Json layer = designer::Json::object();
    layer["op"]      = op;
    layer["id"]      = wanted;
    layers.push_back(std::move(layer));
    if (!this->set("/layers", std::move(layers), error))
        return false;

    outId = wanted;
    return true;
}

bool AppearanceDoc::removeLayer(const std::string& id, std::string& error)
{
    std::string pointer;
    if (!this->layerPointer(id, pointer))
    {
        error = "没有这一层：" + id;
        return false;
    }
    return this->remove(pointer, error);
}

bool AppearanceDoc::moveLayer(const std::string& id, size_t toIndex, std::string& error)
{
    const int from = this->findLayer(id);
    if (from < 0)
    {
        error = "没有这一层：" + id;
        return false;
    }

    const designer::Json* layersConst = this->get("/layers");
    if (layersConst == nullptr || !layersConst->is_array())
    {
        error = "这份外观没有 layers 数组";
        return false;
    }
    designer::Json layers = *layersConst;
    if (toIndex >= layers.size())
    {
        error = "目标位置越界：" + std::to_string(toIndex);
        return false;
    }
    if ((size_t)from == toIndex)
        return true;

    designer::Json moved = std::move(layers[(size_t)from]);
    layers.erase(layers.begin() + from);
    layers.insert(layers.begin() + (std::ptrdiff_t)toIndex, std::move(moved));
    return this->set("/layers", std::move(layers), error);
}

bool AppearanceDoc::duplicateLayer(const std::string& id, std::string& outId, std::string& error)
{
    const int from = this->findLayer(id);
    if (from < 0)
    {
        error = "没有这一层：" + id;
        return false;
    }
    const designer::Json* layersConst = this->get("/layers");
    if (layersConst == nullptr || !layersConst->is_array())
    {
        error = "这份外观没有 layers 数组";
        return false;
    }
    designer::Json layers = *layersConst;

    std::string newId;
    int         n = 1;
    do
    {
        newId = id + "_copy" + (n == 1 ? "" : std::to_string(n));
        n++;
    } while (this->findLayer(newId) >= 0);

    designer::Json copy = layers[(size_t)from];
    if (copy.is_object())
        copy["id"] = newId;
    layers.push_back(std::move(copy));
    if (!this->set("/layers", std::move(layers), error))
        return false;

    outId = newId;
    return true;
}

bool AppearanceDoc::setLayerField(const std::string& id, const std::string& key, Json value,
    std::string& error)
{
    std::string pointer;
    if (!this->layerPointer(id, pointer))
    {
        error = "没有这一层：" + id;
        return false;
    }
    return this->set(pointer + "/" + key, std::move(value), error);
}

bool AppearanceDoc::removeLayerField(const std::string& id, const std::string& key,
    std::string& error)
{
    std::string pointer;
    if (!this->layerPointer(id, pointer))
    {
        error = "没有这一层：" + id;
        return false;
    }
    return this->remove(pointer + "/" + key, error);
}

std::vector<std::string> AppearanceDoc::getStateNames() const
{
    std::vector<std::string> out;
    const Json* states = this->get("/states");
    if (states == nullptr || !states->is_object())
        return out;
    for (auto it = states->begin(); it != states->end(); ++it)
        out.push_back(it.key());
    return out;
}

} // namespace designer
