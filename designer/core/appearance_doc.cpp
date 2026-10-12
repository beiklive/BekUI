#include "designer/core/appearance_doc.hpp"

#include <algorithm>

namespace designer
{

// 数值写成 JSON 时：整数就写整数（64 而不是 64.0）——写回文件时少一堆无意义的改动
nlohmann::ordered_json numberValue(double value)
{
    if (value == (double)(long long)value && value > -1e15 && value < 1e15)
        return nlohmann::ordered_json((long long)value);
    return nlohmann::ordered_json(value);
}

bool AppearanceDoc::parse(const std::string& jsonText, std::string& error)
{
    const Json parsed = Json::parse(jsonText, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object())
    {
        error = "外观 JSON 不是合法对象";
        return false;
    }
    this->doc = parsed;
    return true;
}

std::string AppearanceDoc::serialize() const
{
    return this->doc.dump(2) + "\n";
}

bool AppearanceDoc::hasObject(const std::string& key) const
{
    return this->doc.contains(key) && this->doc[key].is_object();
}

bool AppearanceDoc::hasArray(const std::string& key) const
{
    return this->doc.contains(key) && this->doc[key].is_array();
}

// ---------------------------------------------------------------- box

bool AppearanceDoc::getBoxNumber(const std::string& key, double& out) const
{
    if (!this->hasObject("box") || !this->doc["box"].contains(key))
        return false;
    const Json& value = this->doc["box"][key];
    if (value.is_number())
    {
        out = value.get<double>();
        return true;
    }
    if (value.is_string())
    {
        // 允许 "auto" / "50%" / @theme 引用：原样留着，数值上取 0
        out = 0.0;
        return false;
    }
    return false;
}

void AppearanceDoc::setBoxNumber(const std::string& key, double value)
{
    if (!this->doc.contains("box") || !this->doc["box"].is_object())
        this->doc["box"] = Json::object();
    this->doc["box"][key] = numberValue(value);
}

bool AppearanceDoc::getBoxString(const std::string& key, std::string& out) const
{
    if (!this->hasObject("box") || !this->doc["box"].contains(key))
        return false;
    const Json& value = this->doc["box"][key];
    if (!value.is_string())
        return false;
    out = value.get<std::string>();
    return true;
}

void AppearanceDoc::setBoxString(const std::string& key, const std::string& value)
{
    if (!this->doc.contains("box") || !this->doc["box"].is_object())
        this->doc["box"] = Json::object();
    this->doc["box"][key] = value;
}

void AppearanceDoc::removeBoxKey(const std::string& key)
{
    if (this->hasObject("box"))
        this->doc["box"].erase(key);
}

std::vector<std::string> AppearanceDoc::boxKeys() const
{
    std::vector<std::string> keys;
    if (!this->hasObject("box"))
        return keys;
    for (auto it = this->doc["box"].begin(); it != this->doc["box"].end(); ++it)
        keys.push_back(it.key());
    return keys;
}

// ---------------------------------------------------------------- layers

int AppearanceDoc::layerCount() const
{
    if (!this->hasArray("layers"))
        return 0;
    return (int)this->doc["layers"].size();
}

const AppearanceDoc::Json* AppearanceDoc::layer(int index) const
{
    if (index < 0 || index >= this->layerCount())
        return nullptr;
    return &this->doc["layers"][(size_t)index];
}

AppearanceDoc::Json* AppearanceDoc::layer(int index)
{
    if (index < 0 || index >= this->layerCount())
        return nullptr;
    return &this->doc["layers"][(size_t)index];
}

std::string AppearanceDoc::layerId(int index) const
{
    const Json* item = this->layer(index);
    if (item == nullptr || !item->contains("id") || !(*item)["id"].is_string())
        return std::string();
    return (*item)["id"].get<std::string>();
}

std::string AppearanceDoc::layerOp(int index) const
{
    const Json* item = this->layer(index);
    if (item == nullptr || !item->contains("op") || !(*item)["op"].is_string())
        return "rect";
    return (*item)["op"].get<std::string>();
}

bool AppearanceDoc::getLayerValue(int index, const std::string& key, Json& out) const
{
    const Json* item = this->layer(index);
    if (item == nullptr || !item->contains(key))
        return false;
    out = (*item)[key];
    return true;
}

void AppearanceDoc::setLayerValue(int index, const std::string& key, const Json& value)
{
    Json* item = this->layer(index);
    if (item == nullptr)
        return;
    (*item)[key] = value;
}

void AppearanceDoc::removeLayerKey(int index, const std::string& key)
{
    Json* item = this->layer(index);
    if (item == nullptr)
        return;
    item->erase(key);
}

int AppearanceDoc::addLayer(const std::string& op)
{
    if (!this->doc.contains("layers") || !this->doc["layers"].is_array())
        this->doc["layers"] = Json::array();

    Json item = Json::object();
    item["op"] = op;
    item["id"] = op + std::to_string(this->doc["layers"].size() + 1);

    if (op == "rect")
    {
        item["fill"] = "#00000000";
        item["radius"] = 0;
        item["inset"] = Json::array({ 0, 0, 0, 0 });
    }
    else if (op == "text")
    {
        item["source"] = "text";
        item["color"] = "@theme/color.text.primary";
        item["fontSize"] = "@theme/metric.font.size.md";
        item["align"] = "left";
        item["valign"] = "center";
    }
    else if (op == "image")
    {
        item["source"] = "image";
        item["scaling"] = "fit";
    }
    else if (op == "shadow")
    {
        item["fill"] = "@theme/color.common.shadow";
        item["feather"] = "@theme/metric.shadow.feather";
        item["radius"] = "@theme/metric.shadow.radius";
        item["offset"] = Json::array({ 0, 4 });
        item["cutout"] = false;
    }
    else if (op == "custom")
    {
        item["fill"] = "#00000000";
    }

    this->doc["layers"].push_back(item);
    return this->layerCount() - 1;
}

void AppearanceDoc::removeLayer(int index)
{
    if (index < 0 || index >= this->layerCount())
        return;
    this->doc["layers"].erase(this->doc["layers"].begin() + index);
}

bool AppearanceDoc::moveLayer(int index, int delta)
{
    const int target = index + delta;
    if (index < 0 || index >= this->layerCount() || target < 0 || target >= this->layerCount())
        return false;
    std::swap(this->doc["layers"][(size_t)index], this->doc["layers"][(size_t)target]);
    return true;
}

int AppearanceDoc::duplicateLayer(int index)
{
    const Json* item = this->layer(index);
    if (item == nullptr)
        return -1;

    Json copy = *item;
    if (copy.contains("id") && copy["id"].is_string())
        copy["id"] = copy["id"].get<std::string>() + "Copy";
    this->doc["layers"].insert(this->doc["layers"].begin() + index + 1, copy);
    return index + 1;
}

// ---------------------------------------------------------------- states

std::vector<std::string> AppearanceDoc::stateNames() const
{
    std::vector<std::string> names;
    if (!this->hasObject("states"))
        return names;
    for (auto it = this->doc["states"].begin(); it != this->doc["states"].end(); ++it)
        names.push_back(it.key());
    return names;
}

int AppearanceDoc::channelCount(const std::string& state) const
{
    if (!this->hasObject("states") || !this->doc["states"].contains(state))
        return 0;
    const Json& node = this->doc["states"][state];
    if (!node.is_object())
        return 0;
    return (int)node.size();
}

void AppearanceDoc::addState(const std::string& state)
{
    if (!this->doc.contains("states") || !this->doc["states"].is_object())
        this->doc["states"] = Json::object();
    if (!this->doc["states"].contains(state))
        this->doc["states"][state] = Json::object();
}

void AppearanceDoc::removeState(const std::string& state)
{
    if (this->hasObject("states"))
        this->doc["states"].erase(state);
}

void AppearanceDoc::addChannel(const std::string& state, const std::string& channel)
{
    this->addState(state);
    if (!this->doc["states"][state].contains(channel))
    {
        Json transition = Json::object();
        transition["to"] = 0.0;
        transition["duration"] = 120;
        transition["easing"] = "quadraticOut";
        this->doc["states"][state][channel] = transition;
    }
}

void AppearanceDoc::removeChannel(const std::string& state, int index)
{
    if (!this->hasObject("states") || !this->doc["states"].contains(state))
        return;
    Json& node = this->doc["states"][state];
    if (!node.is_object() || index < 0 || index >= (int)node.size())
        return;

    // 对象迭代器不是随机访问的：不能 begin() + index，要自己往前走
    auto it = node.begin();
    for (int i = 0; i < index && it != node.end(); i++)
        ++it;
    if (it != node.end())
        node.erase(it);
}

bool AppearanceDoc::reorderLayersByConvention()
{
    if (this->layerCount() < 2)
        return false;

    const std::vector<Json> original = this->doc["layers"].get<std::vector<Json>>();

    // rank 越小越先画
    auto rankOf = [](const Json& layer) {
        const std::string op = layer.value("op", std::string("rect"));
        const std::string id = layer.value("id", std::string());
        if (op == "shadow")
            return 0;
        if (id == "hlBg")
            return 2;
        if (id == "hlGlow" || id == "hlBorder")
            return 5;
        if (op == "text" || op == "image" || op == "custom")
            return 3;
        if (op == "rect")
            return 1; // 自己的底色
        return 4;
    };

    std::vector<Json> sorted = original;
    std::stable_sort(sorted.begin(), sorted.end(),
        [&rankOf](const Json& a, const Json& b) { return rankOf(a) < rankOf(b); });

    if (sorted == original)
        return false;

    this->doc["layers"] = Json::array();
    for (const Json& layer : sorted)
        this->doc["layers"].push_back(layer);
    return true;
}

std::vector<std::string> AppearanceDoc::layerOrderWarnings() const
{
    std::vector<std::string> warnings;

    auto indexOfId = [this](const std::string& id) {
        for (int i = 0; i < this->layerCount(); i++)
            if (this->layerId(i) == id)
                return i;
        return -1;
    };
    auto firstOpIndex = [this](const std::vector<std::string>& ops) {
        for (int i = 0; i < this->layerCount(); i++)
            if (std::find(ops.begin(), ops.end(), this->layerOp(i)) != ops.end())
                return i;
        return -1;
    };

    const int hlBg     = indexOfId("hlBg");
    const int content  = firstOpIndex({ "text", "image", "custom" });
    if (hlBg >= 0 && content >= 0 && hlBg > content)
        warnings.push_back("hlBg 排在内容层之后：聚焦时高亮底会把内容盖住");

    const int shadow = firstOpIndex({ "shadow" });
    const int bg     = firstOpIndex({ "rect" });
    if (shadow >= 0 && bg >= 0 && shadow > bg)
        warnings.push_back("shadow 排在底色之后：阴影会被底色盖住");

    const int glow = indexOfId("hlGlow");
    if (glow >= 0 && content >= 0 && glow < content)
        warnings.push_back("hlGlow 排在内容层之前：焦点描边会被内容压住");

    return warnings;
}

} // namespace designer
