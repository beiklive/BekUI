#pragma once

// 主题级音效表（<theme>/sound.json）+ WAV 解码缓存。
//
// 触发点名与 core/audio.hpp 的 Sound 一一对应；文件路径写 "@res/audio/x.wav"
// （相对 resources/）。找不到 sound.json / 文件解码失败都只记日志、返回空。

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "bklv/system/media/audio.hpp"

namespace bklv
{

class SoundBank
{
  public:
    static SoundBank& instance();

    // 当前主题的音效表；主题名变化时自动重载（缺文件回退 default）
    SoundClipPtr clip(Sound sound);
    // 外观里显式写的路径（states.click.sound）
    SoundClipPtr clipFromPath(const std::string& path);

    // 测试/换主题
    void reload(const std::string& themeName);
    const std::string& getPath(Sound sound) const;
    bool hasMapping(Sound sound) const;

  private:
    SoundBank() = default;
    void ensureTheme(const std::string& themeName, const std::string& themeDir);
    SoundClipPtr loadFile(const std::string& path);
    // 已经是解析过的绝对路径，只做解码与缓存（不要再解析一次）
    SoundClipPtr clipResolved(const std::string& resolved);

    std::string themeName;
    std::string themeDir;
    std::string resolvePath(const std::string& path) const;
    std::string paths[(size_t)Sound::HONK + 1];
    SoundClipPtr clips[(size_t)Sound::HONK + 1];
    std::vector<std::pair<std::string, SoundClipPtr>> pathCache;
};

} // namespace bklv
