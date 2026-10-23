#include "bklv/core/sound_bank.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

#include "bklv/core/logger.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"

#include <nlohmann/json.hpp>

namespace bklv
{
namespace
{

uint32_t readU32(const unsigned char* data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16)
        | ((uint32_t)data[3] << 24);
}

uint16_t readU16(const unsigned char* data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

bool triggerFromName(const std::string& name, Sound& out)
{
    const std::string lower = [&] {
        std::string value = name;
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
        return value;
    }();

    if (lower == "click") out = Sound::CLICK;
    else if (lower == "focus") out = Sound::FOCUS_CHANGE;
    else if (lower == "focuserror" || lower == "focus_error") out = Sound::FOCUS_ERROR;
    else if (lower == "back") out = Sound::BACK;
    else if (lower == "clickerror" || lower == "click_error") out = Sound::CLICK_ERROR;
    else if (lower == "sidebarfocus" || lower == "sidebar_focus") out = Sound::FOCUS_SIDEBAR;
    else if (lower == "honk") out = Sound::HONK;
    else return false;
    return true;
}

std::string resolvePathWithBase(const std::string& path, const std::string& themeDir)
{
    if (path.rfind("@res/", 0) == 0)
        return resourcePath("resources/" + path.substr(5));
    if (path.rfind("resources/", 0) == 0)
        return resourcePath(path);
    if (path.rfind("@theme/", 0) == 0)
        return themeDir + path.substr(7);
    if (path.empty() || path[0] == '/')
        return path;
    // 相对路径默认相对**主题目录**（自定义路径主题也能带着自己的音效）
    return themeDir + path;
}

// 只支持 PCM（8/16/24/32-bit 整数或 32-bit float）；输出单声道 48kHz。
SoundClipPtr decodeWav(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        BKLV_LOG_WARNING("音效文件打不开：%s", path.c_str());
        return nullptr;
    }

    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    if (bytes.size() < 44 || std::memcmp(bytes.data(), "RIFF", 4) != 0
        || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
    {
        BKLV_LOG_WARNING("不是 WAV：%s", path.c_str());
        return nullptr;
    }

    uint16_t format = 0;
    uint16_t channels = 1;
    uint32_t sampleRate = 48000;
    uint16_t bits = 16;
    const unsigned char* data = nullptr;
    size_t dataSize = 0;

    size_t offset = 12;
    while (offset + 8 <= bytes.size())
    {
        const unsigned char* chunk = bytes.data() + offset;
        const uint32_t size = readU32(chunk + 4);
        if (std::memcmp(chunk, "fmt ", 4) == 0 && size >= 16 && offset + 8 + 16 <= bytes.size())
        {
            format     = readU16(chunk + 8);
            channels   = readU16(chunk + 10);
            sampleRate = readU32(chunk + 12);
            bits       = readU16(chunk + 22);
        }
        else if (std::memcmp(chunk, "data", 4) == 0)
        {
            data     = chunk + 8;
            dataSize = std::min<size_t>(size, bytes.size() - (offset + 8));
        }
        offset += 8 + size + (size & 1u);
    }

    if (data == nullptr || dataSize == 0 || channels == 0)
    {
        BKLV_LOG_WARNING("WAV 缺少 data/fmt：%s", path.c_str());
        return nullptr;
    }
    if (format != 1 && format != 3)
    {
        BKLV_LOG_WARNING("WAV 不是 PCM：%s（format=%u）", path.c_str(), format);
        return nullptr;
    }

    const int bytesPerSample = bits / 8;
    if (bytesPerSample <= 0 || bytesPerSample > 4)
        return nullptr;

    const size_t frames = dataSize / (size_t)(bytesPerSample * channels);
    auto clip = std::make_shared<SoundClip>();
    clip->samples.resize(frames, 0.0f);

    for (size_t i = 0; i < frames; i++)
    {
        float sum = 0.0f;
        for (uint16_t c = 0; c < channels; c++)
        {
            const unsigned char* sample = data + (i * channels + c) * bytesPerSample;
            float value = 0.0f;
            if (format == 3)
            {
                float f = 0.0f;
                std::memcpy(&f, sample, 4);
                value = f;
            }
            else if (bits == 8)
                value = ((float)sample[0] - 128.0f) / 128.0f;
            else if (bits == 16)
                value = (float)(int16_t)readU16(sample) / 32768.0f;
            else if (bits == 24)
            {
                int32_t v = (int32_t)(sample[0] | (sample[1] << 8) | (sample[2] << 16));
                if (v & 0x800000)
                    v |= ~0xFFFFFF;
                value = (float)v / 8388608.0f;
            }
            else
            {
                int32_t v = (int32_t)readU32(sample);
                value = (float)v / 2147483648.0f;
            }
            sum += value;
        }
        clip->samples[i] = sum / (float)channels;
    }

    // 重采样到 48kHz（线性）
    constexpr int targetRate = 48000;
    if ((int)sampleRate != targetRate && sampleRate > 0 && !clip->samples.empty())
    {
        const size_t outCount = (size_t)((double)frames * targetRate / sampleRate);
        std::vector<float> resampled(outCount);
        for (size_t i = 0; i < outCount; i++)
        {
            const double src = (double)i * sampleRate / targetRate;
            const size_t a = (size_t)src;
            const size_t b = std::min(a + 1, frames - 1);
            const float t = (float)(src - (double)a);
            resampled[i] = clip->samples[a] * (1.0f - t) + clip->samples[b] * t;
        }
        clip->samples.swap(resampled);
    }

    return clip;
}

} // namespace

SoundBank& SoundBank::instance()
{
    static SoundBank bank;
    return bank;
}

void SoundBank::reload(const std::string& theme)
{
    this->themeName.clear();
    this->ensureTheme(theme, resourcePath("resources/theme/" + theme + "/"));
}

void SoundBank::ensureTheme(const std::string& theme, const std::string& dir)
{
    if (this->themeName == theme && this->themeDir == dir)
        return;
    this->themeName = theme;
    this->themeDir  = dir;

    for (auto& path : this->paths)
        path.clear();
    for (auto& clip : this->clips)
        clip.reset();
    this->pathCache.clear();

    auto loadFrom = [this](const std::string& /*name*/, const std::string& directory) {
        const std::string file = directory + "sound.json";
        std::ifstream stream(file);
        if (!stream.is_open())
            return false;

        nlohmann::json doc;
        try
        {
            stream >> doc;
        }
        catch (...)
        {
            BKLV_LOG_WARNING("sound.json 解析失败：%s", file.c_str());
            return false;
        }
        if (!doc.is_object() || !doc.contains("triggers") || !doc["triggers"].is_object())
        {
            BKLV_LOG_WARNING("sound.json 缺少 triggers：%s", file.c_str());
            return false;
        }

        for (auto it = doc["triggers"].begin(); it != doc["triggers"].end(); ++it)
        {
            Sound sound = Sound::NONE;
            if (!triggerFromName(it.key(), sound))
            {
                BKLV_LOG_WARNING("sound.json 不认识的触发点 \"%s\"（%s）", it.key().c_str(), file.c_str());
                continue;
            }
            if (!it.value().is_string())
            {
                BKLV_LOG_WARNING("sound.json 的 triggers.%s 必须是路径字符串", it.key().c_str());
                continue;
            }
            this->paths[(size_t)sound] = resolvePathWithBase(it.value().get<std::string>(), directory);
        }
        return true;
    };

    if (!loadFrom(theme, dir))
        loadFrom("default", getTheme().getDefaultDirectory());
}

SoundClipPtr SoundBank::clip(Sound sound)
{
    this->ensureTheme(getTheme().getName(), getTheme().getDirectory());
    const size_t index = (size_t)sound;
    if (index >= (sizeof(this->clips) / sizeof(this->clips[0])))
        return nullptr;
    if (this->clips[index] != nullptr)
        return this->clips[index];
    if (this->paths[index].empty())
        return nullptr;

    this->clips[index] = this->clipFromPath(this->paths[index]);
    return this->clips[index];
}

SoundClipPtr SoundBank::clipFromPath(const std::string& path)
{
    if (path.empty())
        return nullptr;

    // 覆盖路径按**当前主题目录**解析（@theme/ 或相对路径都能跟着主题走）
    this->ensureTheme(getTheme().getName(), getTheme().getDirectory());
    const std::string resolved = resolvePathWithBase(path, this->themeDir);

    for (const auto& entry : this->pathCache)
        if (entry.first == resolved)
            return entry.second;

    SoundClipPtr clip = decodeWav(resolved);
    this->pathCache.emplace_back(resolved, clip);
    return clip;
}

std::string SoundBank::resolvePath(const std::string& path) const
{
    return resolvePathWithBase(path, this->themeDir);
}

const std::string& SoundBank::getPath(Sound sound) const
{
    static const std::string empty;
    const size_t index = (size_t)sound;
    if (index >= (sizeof(this->paths) / sizeof(this->paths[0])))
        return empty;
    return this->paths[index];
}

bool SoundBank::hasMapping(Sound sound) const
{
    return !this->getPath(sound).empty();
}

} // namespace bklv
