#pragma once

// 宿主可替换的服务（M1）。
//
// 目前只覆盖两个最影响嵌入的点：
//   · ResourceProvider：把 resources/ 的路径解析交给宿主（自定义根、打包资源、内存资源）；
//   · AudioService：宿主接管音效（自己的混音器 / 平台音频 / 静音）。
//
// 默认都是 nullptr = 用库内现有实现。服务由宿主拥有，bklv 只借用；App 销毁前
// 宿主应 clearServices() 或保证服务活得更久。

#include <cstdint>
#include <string>
#include <vector>

#include "bklv/system/media/audio.hpp"
#include "bklv/ui/input/ime.hpp"

namespace bklv
{

class ResourceProvider
{
  public:
    virtual ~ResourceProvider() = default;

    // 逻辑路径（如 "resources/font/ui.ttf"）→ 实际路径/标识
    virtual std::string resolve(const std::string& relative) const = 0;

    // 读取资源；默认按 resolve() 的文件路径读。内存/打包资源可重写。
    virtual bool read(const std::string& relative, std::vector<unsigned char>& out,
        std::string& error) const;

    virtual bool hasResource(const std::string& relative) const;
};

// 字体来源：宿主可提供字体字节或路径。空 = 用库默认的系统字体探测。
class FontProvider
{
  public:
    virtual ~FontProvider() = default;

    // 返回正文字体文件路径（空 = 继续默认探测）
    virtual std::string resolveBodyFont() const { return {}; }

    // 直接提供字体字节（优先级高于路径）；返回 false = 没提供
    virtual bool readBodyFont(std::vector<unsigned char>& out, std::string& error) const
    {
        (void)out;
        (void)error;
        return false;
    }
};

// 输入覆盖：宿主已经采集输入时，用这个接口提供"方向键按住 / 确认 / 返回"，
// 不走游戏手柄或 ImGui 键盘。返回 false 表示不接管，继续用库内来源。
class InputService
{
  public:
    virtual ~InputService() = default;

    // direction: 0=上 1=下 2=左 3=右
    virtual bool isDirectionHeld(int direction) const
    {
        (void)direction;
        return false;
    }
    virtual bool isConfirmDown() const { return false; }
    virtual bool isBackDown() const { return false; }
};

class AudioService
{
  public:
    virtual ~AudioService() = default;

    virtual void play(Sound sound) = 0;
    virtual void playFile(const std::string& path) = 0;

    virtual void setEnabled(bool) { }
    virtual void setVolume(float) { }
};

struct Services
{
    AudioService* audio       = nullptr;
    ResourceProvider* resources = nullptr;
    FontProvider* fonts       = nullptr;
    ImeManager* ime           = nullptr;
    InputService* input       = nullptr;
};

// 全局服务表（M1 仍是单实例；多实例在 Context 阶段迁移）
void setServices(const Services& services);
const Services& getServices();
void clearServices();

// 默认实现：不改变现有行为（资源走 resourcePath，音效走内置混音器）
class DefaultResourceProvider : public ResourceProvider
{
  public:
    std::string resolve(const std::string& relative) const override;
};

} // namespace bklv
