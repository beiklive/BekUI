// include 必须放在守卫内：GL 构建里这个头（以及 vulkan.h）不该被拉进来。
#if !defined(BKLV_SWITCH) && defined(BKLV_VULKAN)

#include "bklv/backends/vulkan_backend.hpp"

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "bklv/core/logger.hpp"

#include "backends/imgui_impl_vulkan.h"

namespace bklv
{
namespace
{
// 活跃实例：给宿主（设计器）挂 UI 通道钩子用
VulkanBackend* gActiveBackend = nullptr;
} // namespace


namespace
{

constexpr uint32_t kFramesInFlight = 2;
constexpr uint32_t kDescriptorPoolSize = 64;

// ImGui 的 ImGui_ImplVulkan_InitInfo::CheckVkResultFn：任何 Vulkan 调用返回非成功都
// 当场停下，而不是带着半初始化的状态继续画。
void checkVkResult(VkResult result)
{
    if (result == VK_SUCCESS)
        return;

    std::fprintf(stderr, "[bklv][error] Vulkan 调用失败：VkResult = %d\n", (int)result);
    std::fflush(stderr);
    std::abort();
}

// 校验层只在显式打开 BKLV_VULKAN_VALIDATION=1 时才挂载，默认零开销。
bool validationRequested()
{
    const char* value = std::getenv("BKLV_VULKAN_VALIDATION");
    return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

bool hasLayer(const char* name)
{
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    if (count > 0)
        vkEnumerateInstanceLayerProperties(&count, layers.data());

    for (const VkLayerProperties& layer : layers)
        if (std::strcmp(layer.layerName, name) == 0)
            return true;
    return false;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void*)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::fprintf(stderr, "[vulkan] %s\n", data->pMessage);
    return VK_FALSE;
}

// 问加载器它能支持的最高实例版本，并封顶到 1.3（1.3 起动态渲染是核心功能）。
// vkEnumerateInstanceVersion 是 1.1 才有的入口，通过 vkGetInstanceProcAddr
// 取函数指针而不是直接调用：老加载器上拿不到就退回 1.0。
uint32_t requestedInstanceApiVersion()
{
    auto enumerateVersion = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(
        VK_NULL_HANDLE, "vkEnumerateInstanceVersion");

    uint32_t version = VK_API_VERSION_1_0;
    if (enumerateVersion != nullptr && enumerateVersion(&version) != VK_SUCCESS)
        version = VK_API_VERSION_1_0;

    if (version > VK_API_VERSION_1_3)
        version = VK_API_VERSION_1_3;
    return version;
}

VkExtent2D clampExtent(const VkSurfaceCapabilitiesKHR& caps, int width, int height){
    VkExtent2D result = {};
    if (caps.currentExtent.width != 0xFFFFFFFFu)
    {
        result = caps.currentExtent;
    }
    else
    {
        result.width  = (uint32_t)std::max(1, width);
        result.height = (uint32_t)std::max(1, height);
    }

    result.width  = std::min(std::max(result.width, caps.minImageExtent.width), caps.maxImageExtent.width);
    result.height = std::min(std::max(result.height, caps.minImageExtent.height), caps.maxImageExtent.height);
    return result;
}

} // namespace

// ---------------------------------------------------------------------------
// 生命周期
// ---------------------------------------------------------------------------

VulkanBackend::VulkanBackend()
{
    gActiveBackend = this;
}

VulkanBackend* activeVulkanBackend()
{
    return gActiveBackend;
}

VulkanBackend::~VulkanBackend()
{
    // 宿主钩子（设计器的玻璃）的资源也是用这个 device 建的：必须在销毁设备前让它释放，
    // 否则它自己的析构里会对着死设备调 vkDestroyImage（踩过：Abort trap: Invalid device）
    if (this->uiPassHook != nullptr)
    {
        this->uiPassHook->detach();
        this->uiPassHook = nullptr;
    }

    // 正常路径下 App 已经按 "纹理 -> ImGui 渲染后端 -> 平台" 的顺序清理过，
    // 这里兜底，避免异常退出时泄漏 Vulkan 对象。
    if (this->device == VK_NULL_HANDLE)
        return;

    vkDeviceWaitIdle(this->device);

    if (this->imguiRendererInited)
    {
        ImGui_ImplVulkan_Shutdown();
        this->imguiRendererInited = false;
    }

    // 正常情况下 destroyTexture 已经把纹理都收走了；这里只清记录表。
    for (TextureRecord* record : this->textures)
        delete record;
    this->textures.clear();

    this->destroySwapchainResources();
    this->destroyFrameResources();

    if (this->renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(this->device, this->renderPass, nullptr);
        this->renderPass = VK_NULL_HANDLE;
    }

    vkDestroyDevice(this->device, nullptr);
    this->device = VK_NULL_HANDLE;

    if (this->surface != VK_NULL_HANDLE && this->instance != VK_NULL_HANDLE)
    {
        SDL_Vulkan_DestroySurface(this->instance, this->surface, nullptr);
        this->surface = VK_NULL_HANDLE;
    }

    if (this->instance != VK_NULL_HANDLE)
    {
        if (this->debugMessenger != VK_NULL_HANDLE)
        {
            auto destroyMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                this->instance, "vkDestroyDebugUtilsMessengerEXT");
            if (destroyMessenger != nullptr)
                destroyMessenger(this->instance, this->debugMessenger, nullptr);
            this->debugMessenger = VK_NULL_HANDLE;
        }

        vkDestroyInstance(this->instance, nullptr);
        this->instance = VK_NULL_HANDLE;
    }

    this->ready = false;
}

unsigned long long VulkanBackend::getWindowFlags() const
{
    return SDL_WINDOW_VULKAN;
}

bool VulkanBackend::initContext(void* nativeWindow)
{
    if (nativeWindow == nullptr)
        return false;

    SDL_Window* target = static_cast<SDL_Window*>(nativeWindow);
    this->window       = target;

    // SDL 自己去加载 libvulkan（Windows 上是 vulkan-1.dll），
    // 之后 SDL_Vulkan_CreateSurface 才有的用。
    if (!SDL_Vulkan_LoadLibrary(nullptr))
    {
        Logger::error(std::string("加载 Vulkan loader 失败：") + SDL_GetError());
        return false;
    }

    if (!this->createInstance())
        return false;

    if (!this->createSurface(target))
        return false;

    if (!this->pickPhysicalDevice())
        return false;

    if (!this->createDevice())
        return false;

    if (!this->createSwapchain(VK_NULL_HANDLE))
        return false;

    if (!this->createFrameResources())
        return false;

    this->logDeviceInfo();

    // ready 是 "能不能创建纹理" 的闸门，放在设备与交换链都建好之后。
    this->ready = true;
    return true;
}

bool VulkanBackend::createInstance()
{
    this->instanceApiVersion = requestedInstanceApiVersion();
    if (this->instanceApiVersion < VK_API_VERSION_1_1)
        this->instanceApiVersion = VK_API_VERSION_1_0;

    uint32_t extensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (sdlExtensions == nullptr || extensionCount == 0)
    {
        Logger::error(std::string("取 Vulkan 实例扩展失败：") + SDL_GetError());
        return false;
    }

    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + extensionCount);

    // 查一遍实例扩展表，既用来去重，也用来判断 portability enumeration 在不在。
    uint32_t availableCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, nullptr);
    std::vector<VkExtensionProperties> available(availableCount);
    if (availableCount > 0)
        vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, available.data());

    const auto isAvailable = [&available](const char* name) {
        for (const VkExtensionProperties& property : available)
            if (std::strcmp(property.extensionName, name) == 0)
                return true;
        return false;
    };

    VkInstanceCreateFlags instanceFlags = 0;
    // macOS 上 MoltenVK 是"可移植"实现，需要显式打开枚举开关。
    if (isAvailable("VK_KHR_portability_enumeration"))
    {
        extensions.push_back("VK_KHR_portability_enumeration");
        instanceFlags |= 0x00000001; // VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
    }

    std::vector<const char*> layers;
    if (validationRequested() && hasLayer("VK_LAYER_KHRONOS_validation"))
    {
        layers.push_back("VK_LAYER_KHRONOS_validation");
        if (isAvailable(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        else
            layers.clear();
    }

    // 去重：SDL 给的和我们补的可能重复，重复的扩展名会让校验层报错。
    std::sort(extensions.begin(), extensions.end(),
        [](const char* a, const char* b) { return std::strcmp(a, b) < 0; });
    extensions.erase(std::unique(extensions.begin(), extensions.end(),
                         [](const char* a, const char* b) { return std::strcmp(a, b) == 0; }),
        extensions.end());

    VkApplicationInfo appInfo = {};
    appInfo.sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "bklv";
    appInfo.pEngineName      = "bklv";
    // 实例请求的 API 版本决定了 vkGetInstanceProcAddr / vkGetDeviceProcAddr
    // 能返回哪些核心函数。低于 1.3 时拿不到 vkCmdBeginRendering 这类 1.3 核心命令，
    // 动态渲染就只能走 KHR 扩展名。所以这里向加载器要它能给的最高版本（上限 1.3）。
    appInfo.apiVersion = this->instanceApiVersion;

    VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugInfo.pfnUserCallback = debugCallback;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.flags                   = instanceFlags;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledLayerCount       = (uint32_t)layers.size();
    createInfo.ppEnabledLayerNames     = layers.empty() ? nullptr : layers.data();
    createInfo.enabledExtensionCount   = (uint32_t)extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();
    if (!layers.empty())
        createInfo.pNext = &debugInfo;

    const VkResult result = vkCreateInstance(&createInfo, nullptr, &this->instance);
    if (result != VK_SUCCESS)
    {
        BKLV_LOG_ERROR("创建 Vulkan 实例失败：VkResult = %d", (int)result);
        return false;
    }

    if (!layers.empty())
    {
        auto createMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            this->instance, "vkCreateDebugUtilsMessengerEXT");
        if (createMessenger != nullptr)
        {
            createMessenger(this->instance, &debugInfo, nullptr, &this->debugMessenger);
            BKLV_LOG_INFO("已启用 Vulkan 校验层");
        }
    }

    return true;
}

bool VulkanBackend::createSurface(SDL_Window* target)
{
    if (!SDL_Vulkan_CreateSurface(target, this->instance, nullptr, &this->surface))
    {
        Logger::error(std::string("创建 Vulkan 表面失败：") + SDL_GetError());
        return false;
    }

    return true;
}

bool VulkanBackend::pickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(this->instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        Logger::error("没有找到支持 Vulkan 的物理设备");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(this->instance, &deviceCount, devices.data());

    uint32_t bestScore = 0;
    for (VkPhysicalDevice candidate : devices)
    {
        // 必须同时能画（graphics）和能显示（present 到我们的 surface）。
        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());

        bool hasQueue = false;
        for (uint32_t i = 0; i < familyCount; i++)
        {
            if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
                continue;

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, this->surface, &presentSupport);
            if (presentSupport == VK_TRUE)
            {
                hasQueue = true;
                break;
            }
        }
        if (!hasQueue)
            continue;

        // 必须支持交换链；MoltenVK 还要求显式启用 portability subset。
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        if (extensionCount > 0)
            vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, extensions.data());

        bool hasSwapchain = false;
        for (const VkExtensionProperties& property : extensions)
            if (std::strcmp(property.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
                hasSwapchain = true;
        if (!hasSwapchain)
            continue;

        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(candidate, &properties);

        uint32_t score = 1;
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            score = 3;
        else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            score = 2;

        if (score > bestScore)
        {
            bestScore      = score;
            this->physical = candidate;
        }
    }

    if (this->physical == VK_NULL_HANDLE)
    {
        Logger::error("没有找到同时支持图形与显示的 Vulkan 设备");
        return false;
    }

    return true;
}

bool VulkanBackend::createDevice()
{
    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(this->physical, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(this->physical, &familyCount, families.data());

    bool foundFamily = false;
    for (uint32_t i = 0; i < familyCount; i++)
    {
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
            continue;

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(this->physical, i, this->surface, &presentSupport);
        if (presentSupport == VK_TRUE)
        {
            this->queueFamily = i;
            foundFamily       = true;
            break;
        }
    }

    if (!foundFamily)
    {
        Logger::error("找不到同时支持图形与显示的队列族");
        return false;
    }

    // 动态渲染是可选的：驱动支持就少建一个 VkRenderPass，不支持就退回传统渲染通道。
    // 注意它有两种开启途径，取决于实例层拿到的 API 版本：
    //   1.3 及以上 —— 核心功能，用 VkPhysicalDeviceDynamicRenderingFeatures 开启；
    //   1.3 以下   —— 只能靠 VK_KHR_dynamic_rendering 扩展。
    const bool hasDynamicRenderingCore = this->instanceApiVersion >= VK_API_VERSION_1_3;

    VkPhysicalDeviceFeatures supportedFeatures = {};
    vkGetPhysicalDeviceFeatures(this->physical, &supportedFeatures);

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering = {};
    dynamicRendering.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRendering.dynamicRendering = VK_FALSE;

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = hasDynamicRenderingCore ? (void*)&dynamicRendering : nullptr;
    vkGetPhysicalDeviceFeatures2(this->physical, &features2);

    this->blendDynamic = hasDynamicRenderingCore && (dynamicRendering.dynamicRendering == VK_TRUE);
    dynamicRendering.dynamicRendering = this->blendDynamic ? VK_TRUE : VK_FALSE;

    VkPhysicalDeviceFeatures features = {};
    features.samplerAnisotropy  = supportedFeatures.samplerAnisotropy;
    features.fillModeNonSolid   = supportedFeatures.fillModeNonSolid;
    features.wideLines          = supportedFeatures.wideLines;
    features.independentBlend   = supportedFeatures.independentBlend;

    // 设备扩展：交换链必备；portability subset 存在就必须开；
    // 实例只拿到 1.3 以下时，动态渲染必须靠扩展才存在。
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(this->physical, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    if (extensionCount > 0)
        vkEnumerateDeviceExtensionProperties(this->physical, nullptr, &extensionCount, availableExtensions.data());

    bool portabilitySubset = false;
    bool dynamicRenderingExtension = false;
    bool hasSwapchain = false;
    for (const VkExtensionProperties& property : availableExtensions)
    {
        if (std::strcmp(property.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
            hasSwapchain = true;
        else if (std::strcmp(property.extensionName, "VK_KHR_portability_subset") == 0)
            portabilitySubset = true;
        else if (std::strcmp(property.extensionName, "VK_KHR_dynamic_rendering") == 0)
            dynamicRenderingExtension = true;
    }

    if (!hasDynamicRenderingCore && dynamicRenderingExtension)
        this->blendDynamic = true;

    std::vector<const char*> deviceExtensions;
    if (hasSwapchain)
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (portabilitySubset)
        deviceExtensions.push_back("VK_KHR_portability_subset");
    if (this->blendDynamic && !hasDynamicRenderingCore)
        deviceExtensions.push_back("VK_KHR_dynamic_rendering");

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {};
    queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = this->queueFamily;
    queueInfo.queueCount       = 1;
    queueInfo.pQueuePriorities = &priority;

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext                   = this->blendDynamic ? (const void*)&dynamicRendering : nullptr;
    deviceInfo.queueCreateInfoCount    = 1;
    deviceInfo.pQueueCreateInfos       = &queueInfo;
    deviceInfo.enabledExtensionCount   = (uint32_t)deviceExtensions.size();
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.empty() ? nullptr : deviceExtensions.data();
    deviceInfo.pEnabledFeatures        = &features;

    const VkResult result = vkCreateDevice(this->physical, &deviceInfo, nullptr, &this->device);
    if (result != VK_SUCCESS)
    {
        BKLV_LOG_ERROR("创建 Vulkan 逻辑设备失败：VkResult = %d", (int)result);
        return false;
    }

    vkGetDeviceQueue(this->device, this->queueFamily, 0, &this->queue);
    if (this->queue == VK_NULL_HANDLE)
    {
        Logger::error("取 Vulkan 图形队列失败");
        return false;
    }

    return true;
}

void VulkanBackend::logDeviceInfo() const
{
    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(this->physical, &properties);

    BKLV_LOG_INFO("Vulkan 设备：%s（API %u.%u.%u）", properties.deviceName,
        VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion),
        VK_API_VERSION_PATCH(properties.apiVersion));
    BKLV_LOG_INFO("Vulkan 渲染通道：%s", this->blendDynamic ? "动态渲染" : "传统 VkRenderPass");}

// ---------------------------------------------------------------------------
// 交换链
// ---------------------------------------------------------------------------

bool VulkanBackend::createSwapchain(VkSwapchainKHR oldSwapchain)
{
    VkSurfaceCapabilitiesKHR capabilities = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(this->physical, this->surface, &capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(this->physical, this->surface, &formatCount, nullptr);
    if (formatCount == 0)
    {
        Logger::error("Vulkan 表面没有可用的像素格式");
        return false;
    }
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(this->physical, this->surface, &formatCount, formats.data());

    // 必须挑非 sRGB 的 UNORM 格式，不能挑 _SRGB。
    // ImGui 给出的颜色分量就是"最终要显示的 8 位值"，相当于已经做过 sRGB 编码；
    // 目标若是 _SRGB 格式，硬件会再编码一次，整体被提亮（64 -> 137，128 -> 188，
    // 深色背景 (26,28,33) 被抬到 (89,92,100)），画面看起来发灰。
    // OpenGL 后端那张默认帧缓冲是 UNORM 且没开 GL_FRAMEBUFFER_SRGB，数值原样进屏幕，
    // 选 UNORM 才能让两个后端颜色一致。
    VkSurfaceFormatKHR chosenFormat = formats[0];
    bool foundUnorm = false;
    for (const VkSurfaceFormatKHR& candidate : formats)
    {
        if (candidate.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            continue;
        if (candidate.format == VK_FORMAT_B8G8R8A8_UNORM || candidate.format == VK_FORMAT_R8G8B8A8_UNORM)
        {
            chosenFormat = candidate;
            foundUnorm   = true;
            break;
        }
    }
    if (!foundUnorm)
    {
        // 只提供 sRGB 格式的表面（很少见）：颜色会偏亮，说明原因而不是悄悄变化。
        Logger::warning("Vulkan 表面只提供 sRGB 格式，颜色会比 OpenGL 后端偏亮");
    }

    // 优先 MAILBOX（低延迟），没有就 FIFO。FIFO 是规范保证一定支持的。
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(this->physical, this->surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    if (modeCount > 0)
    {
        vkGetPhysicalDeviceSurfacePresentModesKHR(this->physical, this->surface, &modeCount, modes.data());
        for (VkPresentModeKHR candidate : modes)
        {
            if (candidate == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                presentMode = candidate;
                break;
            }
        }
    }

    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GetWindowSizeInPixels(this->window, &drawableWidth, &drawableHeight);

    VkExtent2D newExtent = clampExtent(capabilities, drawableWidth, drawableHeight);
    if (newExtent.width == 0 || newExtent.height == 0)
        return false;

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
        imageCount = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = this->surface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = chosenFormat.format;
    createInfo.imageColorSpace  = chosenFormat.colorSpace;
    createInfo.imageExtent      = newExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.preTransform     = capabilities.currentTransform;
    createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode      = presentMode;
    createInfo.clipped          = VK_TRUE;
    createInfo.oldSwapchain     = oldSwapchain;

    // 图形与显示是同一个队列族，独占模式最省事。
    createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices   = nullptr;

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    VkResult result = vkCreateSwapchainKHR(this->device, &createInfo, nullptr, &newSwapchain);
    if (result != VK_SUCCESS)
    {
        BKLV_LOG_ERROR("创建 Vulkan 交换链失败：VkResult = %d", (int)result);
        return false;
    }

    // 新交换链建好了，旧的（如果有）现在可以销毁。
    if (oldSwapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(this->device, oldSwapchain, nullptr);

    this->swapchain   = newSwapchain;
    this->imageFormat = chosenFormat.format;
    BKLV_LOG_INFO("Vulkan 交换链格式：%d（%s）", (int)chosenFormat.format,
        chosenFormat.format == VK_FORMAT_B8G8R8A8_SRGB || chosenFormat.format == VK_FORMAT_R8G8B8A8_SRGB
            ? "sRGB" : "UNORM");
    this->extent      = newExtent;
    this->imageCount  = 0;
    result = vkGetSwapchainImagesKHR(this->device, this->swapchain, &this->imageCount, nullptr);
    if (result != VK_SUCCESS || this->imageCount == 0)
    {
        Logger::error("取 Vulkan 交换链图像失败");
        return false;
    }
    this->images.resize(this->imageCount);
    vkGetSwapchainImagesKHR(this->device, this->swapchain, &this->imageCount, this->images.data());

    // 图像视图
    this->imageViews.assign(this->imageCount, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < this->imageCount; i++)
    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = this->images[i];
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = this->imageFormat;
        viewInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;
        result = vkCreateImageView(this->device, &viewInfo, nullptr, &this->imageViews[i]);
        if (result != VK_SUCCESS)
        {
            Logger::error("创建 Vulkan 图像视图失败");
            return false;
        }
    }

    // 传统路径需要渲染通道 + 帧缓冲；动态渲染不需要。
    if (!this->blendDynamic)
    {
        if (this->renderPass == VK_NULL_HANDLE)
        {
            // 格式不会在重建中变，所以渲染通道只建一次。
            VkAttachmentDescription attachment = {};
            attachment.format         = this->imageFormat;
            attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentReference colorReference = {};
            colorReference.attachment = 0;
            colorReference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments    = &colorReference;

            VkSubpassDependency dependency = {};
            dependency.srcSubpass      = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass      = 0;
            dependency.srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask   = 0;
            dependency.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            VkRenderPassCreateInfo passInfo = {};
            passInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            passInfo.attachmentCount = 1;
            passInfo.pAttachments    = &attachment;
            passInfo.subpassCount    = 1;
            passInfo.pSubpasses      = &subpass;
            passInfo.dependencyCount = 1;
            passInfo.pDependencies   = &dependency;

            result = vkCreateRenderPass(this->device, &passInfo, nullptr, &this->renderPass);
            if (result != VK_SUCCESS)
            {
                Logger::error("创建 Vulkan 渲染通道失败");
                return false;
            }
        }

        this->framebuffers.assign(this->imageCount, VK_NULL_HANDLE);
        for (uint32_t i = 0; i < this->imageCount; i++)
        {
            VkImageView attachment = this->imageViews[i];

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass      = this->renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments    = &attachment;
            framebufferInfo.width           = this->extent.width;
            framebufferInfo.height          = this->extent.height;
            framebufferInfo.layers          = 1;
            result = vkCreateFramebuffer(this->device, &framebufferInfo, nullptr, &this->framebuffers[i]);
            if (result != VK_SUCCESS)
            {
                Logger::error("创建 Vulkan 帧缓冲失败");
                return false;
            }
        }
    }

    // 渲染完成信号量按"图像"分配，而不是按"帧"。
    // 它在提交时发出信号、在呈现时被等待，而呈现引擎释放信号量的时刻并不由帧栅栏决定：
    // 栅栏只保证提交执行完，不保证呈现已经读完了那张图。
    // 只有"这张图被重新取到"才证明上一次用到它的呈现真的结束了，
    // 所以信号量必须跟着图像索引走，才能保证复用时它一定未被占用。
    // 校验层对 VUID-vkQueueSubmit-pSignalSemaphores-00067 给出的首选方案正是这一条。
    this->renderFinished.assign(this->imageCount, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < this->imageCount; i++)
    {
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (vkCreateSemaphore(this->device, &semaphoreInfo, nullptr, &this->renderFinished[i]) != VK_SUCCESS)
        {
            Logger::error("创建 Vulkan 信号量失败");
            return false;
        }
    }

    this->frameIndex = 0;
    BKLV_LOG_INFO("Vulkan 交换链：%ux%u，%u 张图像，present mode %d", this->extent.width, this->extent.height,
        this->imageCount, (int)presentMode);
    return true;
}

bool VulkanBackend::createFrameResources()
{
    this->framesInFlight = std::min(kFramesInFlight, this->imageCount == 0 ? kFramesInFlight : this->imageCount);
    if (this->framesInFlight == 0)
        this->framesInFlight = 1;

    this->commandPools.assign(this->framesInFlight, VK_NULL_HANDLE);
    this->commandBuffers.assign(this->framesInFlight, VK_NULL_HANDLE);
    this->fences.assign(this->framesInFlight, VK_NULL_HANDLE);
    this->imageAvailable.assign(this->framesInFlight, VK_NULL_HANDLE);

    for (uint32_t i = 0; i < this->framesInFlight; i++)
    {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = this->queueFamily;
        VkResult result = vkCreateCommandPool(this->device, &poolInfo, nullptr, &this->commandPools[i]);
        if (result != VK_SUCCESS)
        {
            Logger::error("创建 Vulkan 命令池失败");
            return false;
        }

        VkCommandBufferAllocateInfo bufferInfo = {};
        bufferInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        bufferInfo.commandPool        = this->commandPools[i];
        bufferInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        bufferInfo.commandBufferCount = 1;
        result = vkAllocateCommandBuffers(this->device, &bufferInfo, &this->commandBuffers[i]);
        if (result != VK_SUCCESS)
        {
            Logger::error("分配 Vulkan 命令缓冲失败");
            return false;
        }

        // 一开始就置为已发信号，第一次等待才不会卡住。
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        result = vkCreateFence(this->device, &fenceInfo, nullptr, &this->fences[i]);
        if (result != VK_SUCCESS)
        {
            Logger::error("创建 Vulkan 栅栏失败");
            return false;
        }

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        result = vkCreateSemaphore(this->device, &semaphoreInfo, nullptr, &this->imageAvailable[i]);
        if (result != VK_SUCCESS)
        {
            Logger::error("创建 Vulkan 信号量失败");
            return false;
        }
    }

    return true;
}

void VulkanBackend::destroySwapchainResources()
{
    if (this->device == VK_NULL_HANDLE)
        return;

    for (VkFramebuffer framebuffer : this->framebuffers)
        if (framebuffer != VK_NULL_HANDLE)
            vkDestroyFramebuffer(this->device, framebuffer, nullptr);
    this->framebuffers.clear();

    for (VkImageView view : this->imageViews)
        if (view != VK_NULL_HANDLE)
            vkDestroyImageView(this->device, view, nullptr);
    this->imageViews.clear();
    this->images.clear();

    for (VkSemaphore semaphore : this->renderFinished)
        if (semaphore != VK_NULL_HANDLE)
            vkDestroySemaphore(this->device, semaphore, nullptr);
    this->renderFinished.clear();

    if (this->swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(this->device, this->swapchain, nullptr);
        this->swapchain = VK_NULL_HANDLE;
    }

    this->imageCount   = 0;
    this->imageAcquired = false;
}

void VulkanBackend::destroyFrameResources()
{
    if (this->device == VK_NULL_HANDLE)
        return;

    for (VkFence fence : this->fences)
        if (fence != VK_NULL_HANDLE)
            vkDestroyFence(this->device, fence, nullptr);
    this->fences.clear();

    for (VkSemaphore semaphore : this->imageAvailable)
        if (semaphore != VK_NULL_HANDLE)
            vkDestroySemaphore(this->device, semaphore, nullptr);
    this->imageAvailable.clear();

    for (VkCommandPool pool : this->commandPools)
        if (pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(this->device, pool, nullptr);
    this->commandPools.clear();
    this->commandBuffers.clear();
    this->framesInFlight = 0;
}

bool VulkanBackend::isDynamicRenderingEnabled() const
{
    return this->blendDynamic;
}

// ---------------------------------------------------------------------------
// ImGui 渲染后端
// ---------------------------------------------------------------------------

bool VulkanBackend::initImGuiRenderer()
{
    if (!this->ready || this->device == VK_NULL_HANDLE)
        return false;

    ImGuiIO& io = ImGui::GetIO();
    // 字体图集由渲染后端按需创建（ImGui 1.92+）。
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.BackendRendererName = "bklv_vulkan";

    ImGui_ImplVulkan_InitInfo info = {};
    // 告诉 ImGui 我们实际拿到的 API 版本：它据此决定
    // vkCmdBeginRendering 是先试核心名还是直接试 KHR 名。
    info.ApiVersion     = this->instanceApiVersion;
    info.Instance       = this->instance;
    info.PhysicalDevice = this->physical;
    info.Device         = this->device;
    info.QueueFamily    = this->queueFamily;
    info.Queue          = this->queue;
    // 让后端自己建描述符池：省掉一套池大小/上限的维护，
    // 它内部会带上 VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT。
    info.DescriptorPool     = VK_NULL_HANDLE;
    info.DescriptorPoolSize = kDescriptorPoolSize;
    info.MinImageCount      = this->imageCount;
    info.ImageCount         = this->imageCount;
    info.PipelineCache      = VK_NULL_HANDLE;
    info.Allocator          = nullptr;
    info.CheckVkResultFn    = checkVkResult;

    if (this->blendDynamic)
    {
        info.UseDynamicRendering                             = true;
        info.PipelineInfoMain.RenderPass                     = VK_NULL_HANDLE;
        info.PipelineInfoMain.Subpass                        = 0;
        info.PipelineInfoMain.MSAASamples                    = VK_SAMPLE_COUNT_1_BIT;
        info.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount    = 1;
        info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &this->imageFormat;
    }
    else
    {
        info.UseDynamicRendering         = false;
        info.PipelineInfoMain.RenderPass = this->renderPass;
        info.PipelineInfoMain.Subpass    = 0;
        info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    }

    if (!ImGui_ImplVulkan_Init(&info))
    {
        Logger::error("ImGui Vulkan 渲染后端初始化失败");
        return false;
    }

    this->imguiRendererInited = true;
    return true;
}

void VulkanBackend::shutdownImGuiRenderer()
{
    if (this->device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(this->device);

    if (this->imguiRendererInited)
    {
        ImGui_ImplVulkan_Shutdown();
        this->imguiRendererInited = false;
    }

    this->ready = false;
}

// ---------------------------------------------------------------------------
// 帧
// ---------------------------------------------------------------------------

bool VulkanBackend::prepareFrame()
{
    this->inRenderPass = false;

    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GetWindowSizeInPixels(this->window, &drawableWidth, &drawableHeight);
    if (drawableWidth <= 0 || drawableHeight <= 0)
    {
        // 最小化：没有可画的区域，跳过这一帧。
        this->imageAcquired = false;
        return false;
    }

    if ((uint32_t)drawableWidth != this->extent.width || (uint32_t)drawableHeight != this->extent.height)
        this->recreateSwapchain = true;

    // 上一帧这一槽的工作必须先结束，才能复用它的命令缓冲与信号量。
    VkResult result = vkWaitForFences(this->device, 1, &this->fences[this->frameIndex], VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS)
    {
        BKLV_LOG_ERROR("等待 Vulkan 栅栏失败：VkResult = %d", (int)result);
        this->imageAcquired = false;
        return false;
    }

    if (this->recreateSwapchain)
    {
        this->recreateSwapchain = false;

        vkDeviceWaitIdle(this->device);
        VkSwapchainKHR oldSwapchain = this->swapchain;
        this->swapchain = VK_NULL_HANDLE;
        this->destroySwapchainResources();

        if (!this->createSwapchain(oldSwapchain))
        {
            Logger::error("重建 Vulkan 交换链失败");
            this->recreateSwapchain = true;
            this->imageAcquired      = false;
            return false;
        }

        // 驱动会保证新的 minImageCount 至少和之前一样，所以帧资源还能继续用。
    }

    result = vkAcquireNextImageKHR(this->device, this->swapchain, UINT64_MAX,
        this->imageAvailable[this->frameIndex], VK_NULL_HANDLE, &this->imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        this->recreateSwapchain = true;
        this->imageAcquired      = false;
        return false;
    }
    if (result == VK_SUBOPTIMAL_KHR)
    {
        // 还能画，但下一帧重建。
        this->recreateSwapchain = true;
    }
    else if (result != VK_SUCCESS)
    {
        BKLV_LOG_ERROR("获取 Vulkan 交换链图像失败：VkResult = %d", (int)result);
        this->imageAcquired = false;
        return false;
    }

    this->imageAcquired = true;

    result = vkResetCommandBuffer(this->commandBuffers[this->frameIndex], 0);
    if (result != VK_SUCCESS)
    {
        BKLV_LOG_ERROR("重置 Vulkan 命令缓冲失败：VkResult = %d", (int)result);
        this->imageAcquired = false;
        return false;
    }

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vkBeginCommandBuffer(this->commandBuffers[this->frameIndex], &beginInfo);
    if (result != VK_SUCCESS)
    {
        BKLV_LOG_ERROR("开始录制 Vulkan 命令缓冲失败：VkResult = %d", (int)result);
        this->imageAcquired = false;
        return false;
    }

    return true;
}

void VulkanBackend::setUiPassHook(bklv::VulkanUiPassHook* hook)
{
    this->uiPassHook     = hook;
    this->uiPassPreserve = false;
    if (hook == nullptr)
        return;

    if (!this->blendDynamic)
    {
        // 传统 render pass 路径做不到"不清屏"，直接告诉宿主：钩子用不了
        BKLV_LOG_WARNING("[UI 钩子] 当前走的是传统 render pass，做不到保留画面，钩子不生效");
        this->uiPassHook = nullptr;
        return;
    }

    hook->attach(this->instance, this->physical, this->device, this->queue, this->queueFamily,
        this->commandPools.empty() ? VK_NULL_HANDLE : this->commandPools[0]);
    hook->configure(this->imageFormat, this->extent);
}

bool VulkanBackend::isUiPassHookActive() const
{
    return this->uiPassHook != nullptr;
}

bool VulkanBackend::beginRendering()
{
    VkCommandBuffer commandBuffer = this->commandBuffers[this->frameIndex];

    // 等这一帧真正开始录制时再传字体/纹理的新像素，避免和上一帧的提交抢资源。
    ImGui_ImplVulkan_NewFrame();

    if (this->blendDynamic)
    {
        // 动态渲染没有隐式布局转换，要自己把图像转成颜色附件。
        VkImageMemoryBarrier barrier = {};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask       = 0;
        barrier.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = this->images[this->imageIndex];
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkRenderingAttachmentInfo colorAttachment = {};
        colorAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView   = this->imageViews[this->imageIndex];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        // 钩子说"底我已经铺好了"时不能清屏，否则会把它画的东西擦掉
        colorAttachment.loadOp      = this->uiPassPreserve ? VK_ATTACHMENT_LOAD_OP_LOAD
                                                          : VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color.float32[0] = this->clearColor[0];
        colorAttachment.clearValue.color.float32[1] = this->clearColor[1];
        colorAttachment.clearValue.color.float32[2] = this->clearColor[2];
        colorAttachment.clearValue.color.float32[3] = this->clearColor[3];

        // 玻璃等宿主工作在通道外做：此时命令缓冲已开、图像已是颜色附件布局
        this->uiPassPreserve = this->uiPassHook != nullptr
            && this->uiPassHook->beforeUiPass(commandBuffer, this->images[this->imageIndex],
                this->imageViews[this->imageIndex], this->imageFormat, this->extent);

        VkRenderingInfo renderingInfo = {};
        renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset.x  = 0;
        renderingInfo.renderArea.offset.y  = 0;
        renderingInfo.renderArea.extent    = this->extent;
        renderingInfo.layerCount           = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments    = &colorAttachment;
        vkCmdBeginRendering(commandBuffer, &renderingInfo);
    }
    else
    {
        VkClearValue clearValue = {};
        clearValue.color.float32[0] = this->clearColor[0];
        clearValue.color.float32[1] = this->clearColor[1];
        clearValue.color.float32[2] = this->clearColor[2];
        clearValue.color.float32[3] = this->clearColor[3];

        VkRenderPassBeginInfo passInfo = {};
        passInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        passInfo.renderPass        = this->renderPass;
        passInfo.framebuffer       = this->framebuffers[this->imageIndex];
        passInfo.renderArea.offset.x = 0;
        passInfo.renderArea.offset.y = 0;
        passInfo.renderArea.extent   = this->extent;
        passInfo.clearValueCount     = 1;
        passInfo.pClearValues        = &clearValue;
        vkCmdBeginRenderPass(commandBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    this->inRenderPass = true;
    return true;
}

void VulkanBackend::newFrame()
{
    // ImGui 的平台后端那一半由 PlatformSDL3::mainLoopIteration 调用。
}

void VulkanBackend::clear(const float rgba[4])
{
    for (int i = 0; i < 4; i++)
        this->clearColor[i] = rgba[i];

    if (!this->prepareFrame())
        return;

    this->beginRendering();
}

void VulkanBackend::renderDrawData(ImDrawData* drawData)
{
    if (!this->inRenderPass || drawData == nullptr)
        return;

    this->recordImGui(drawData);
}

void VulkanBackend::recordImGui(ImDrawData* drawData)
{
    ImGui_ImplVulkan_RenderDrawData(drawData, this->commandBuffers[this->frameIndex]);
}

void VulkanBackend::endRendering()
{
    if (!this->inRenderPass)
        return;

    if (this->blendDynamic)
    {
        vkCmdEndRendering(this->commandBuffers[this->frameIndex]);

        // 通道之后、转 present 之前：钩子可以把成品拷走留给下一帧
        if (this->uiPassHook != nullptr)
            this->uiPassHook->afterUiPass(this->commandBuffers[this->frameIndex],
                this->images[this->imageIndex], this->imageFormat, this->extent);

        // 呈现前转成 PRESENT_SRC。
        VkImageMemoryBarrier barrier = {};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask       = 0;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = this->images[this->imageIndex];
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        vkCmdPipelineBarrier(this->commandBuffers[this->frameIndex], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
    else
    {
        vkCmdEndRenderPass(this->commandBuffers[this->frameIndex]);
    }

    this->inRenderPass = false;
}

void VulkanBackend::present()
{
    if (!this->imageAcquired)
        return;

    this->endRendering();

    VkCommandBuffer commandBuffer = this->commandBuffers[this->frameIndex];
    VkResult result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS)
    {
        BKLV_LOG_ERROR("结束 Vulkan 命令缓冲失败：VkResult = %d", (int)result);
        this->imageAcquired = false;
        return;
    }

    // 等待"取到图像"的信号量，提交后再由"渲染完成"的信号量交给呈现。
    // 取图像用的是本帧槽位的信号量：这个槽的栅栏刚刚等过，它一定空闲。
    // 渲染完成用的是本张图像的信号量：只有这张图被重新取到，才轮到它复用。
    VkSemaphore imageAvailable = this->imageAvailable[this->frameIndex];
    VkSemaphore renderFinished = this->renderFinished[this->imageIndex];

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo = {};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &imageAvailable;
    submitInfo.pWaitDstStageMask    = &waitStage;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &renderFinished;

    VkFence frameFence = this->fences[this->frameIndex];
    vkResetFences(this->device, 1, &frameFence);
    result = vkQueueSubmit(this->queue, 1, &submitInfo, frameFence);
    if (result != VK_SUCCESS)
    {
        BKLV_LOG_ERROR("提交 Vulkan 命令缓冲失败：VkResult = %d", (int)result);
        this->imageAcquired = false;
        return;
    }

    VkSwapchainKHR swapchain = this->swapchain;
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &renderFinished;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &swapchain;
    presentInfo.pImageIndices      = &this->imageIndex;

    result = vkQueuePresentKHR(this->queue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        this->recreateSwapchain = true;
    }
    else if (result != VK_SUCCESS)
    {
        BKLV_LOG_ERROR("呈现 Vulkan 图像失败：VkResult = %d", (int)result);
    }

    this->imageAcquired = false;
    this->frameIndex    = (this->frameIndex + 1) % this->framesInFlight;
}

// ---------------------------------------------------------------------------
// 纹理
// ---------------------------------------------------------------------------

TextureHandle VulkanBackend::createTexture(const unsigned char* pixels, int width, int height)
{
    TextureHandle handle;

    if (!this->ready)
    {
        Logger::warning("Vulkan 上下文未就绪，纹理创建被跳过");
        return handle;
    }

    if (pixels == nullptr || width <= 0 || height <= 0)
        return handle;

    const VkDeviceSize byteSize = (VkDeviceSize)width * (VkDeviceSize)height * 4;

    // 1) 暂存缓冲：CPU 可见，用来把像素喂给 vkCmdCopyBufferToImage。
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = byteSize;
    bufferInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(this->device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS)
    {
        Logger::error("创建 Vulkan 暂存缓冲失败");
        return handle;
    }

    VkMemoryRequirements bufferRequirements = {};
    vkGetBufferMemoryRequirements(this->device, stagingBuffer, &bufferRequirements);

    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(this->physical, &memoryProperties);

    uint32_t bufferType = UINT32_MAX;
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        const bool hasType = (bufferRequirements.memoryTypeBits & (1u << i)) != 0;
        const bool isHostVisible =
            (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
        if (hasType && isHostVisible)
        {
            bufferType = i;
            break;
        }
    }
    if (bufferType == UINT32_MAX)
    {
        Logger::error("找不到合适的 Vulkan 主机可见内存类型");
        vkDestroyBuffer(this->device, stagingBuffer, nullptr);
        return handle;
    }

    VkMemoryAllocateInfo bufferAllocate = {};
    bufferAllocate.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bufferAllocate.allocationSize  = bufferRequirements.size;
    bufferAllocate.memoryTypeIndex = bufferType;

    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (vkAllocateMemory(this->device, &bufferAllocate, nullptr, &stagingMemory) != VK_SUCCESS)
    {
        Logger::error("分配 Vulkan 暂存内存失败");
        vkDestroyBuffer(this->device, stagingBuffer, nullptr);
        return handle;
    }
    vkBindBufferMemory(this->device, stagingBuffer, stagingMemory, 0);

    void* mapped = nullptr;
    if (vkMapMemory(this->device, stagingMemory, 0, byteSize, 0, &mapped) != VK_SUCCESS)
    {
        Logger::error("映射 Vulkan 暂存内存失败");
        vkDestroyBuffer(this->device, stagingBuffer, nullptr);
        vkFreeMemory(this->device, stagingMemory, nullptr);
        return handle;
    }
    std::memcpy(mapped, pixels, (size_t)byteSize);
    vkUnmapMemory(this->device, stagingMemory);

    // 2) 设备本地图像。像素是 RGBA8，选 R8G8B8A8_UNORM，
    //    不管交换链是不是 sRGB：ImGui 的纹理采样只关心线性值。
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent.width  = (uint32_t)width;
    imageInfo.extent.height = (uint32_t)height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(this->device, &imageInfo, nullptr, &image) != VK_SUCCESS)
    {
        Logger::error("创建 Vulkan 图像失败");
        vkDestroyBuffer(this->device, stagingBuffer, nullptr);
        vkFreeMemory(this->device, stagingMemory, nullptr);
        return handle;
    }

    VkMemoryRequirements imageRequirements = {};
    vkGetImageMemoryRequirements(this->device, image, &imageRequirements);

    uint32_t imageType = UINT32_MAX;
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        const bool hasType     = (imageRequirements.memoryTypeBits & (1u << i)) != 0;
        const bool isDeviceLocal =
            (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
        if (hasType && isDeviceLocal)
        {
            imageType = i;
            break;
        }
    }
    if (imageType == UINT32_MAX)
        imageType = bufferType;

    VkMemoryAllocateInfo imageAllocate = {};
    imageAllocate.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imageAllocate.allocationSize  = imageRequirements.size;
    imageAllocate.memoryTypeIndex = imageType;

    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    if (vkAllocateMemory(this->device, &imageAllocate, nullptr, &imageMemory) != VK_SUCCESS)
    {
        Logger::error("分配 Vulkan 图像内存失败");
        vkDestroyImage(this->device, image, nullptr);
        vkDestroyBuffer(this->device, stagingBuffer, nullptr);
        vkFreeMemory(this->device, stagingMemory, nullptr);
        return handle;
    }
    vkBindImageMemory(this->device, image, imageMemory, 0);

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(this->device, &viewInfo, nullptr, &view) != VK_SUCCESS)
    {
        Logger::error("创建 Vulkan 纹理视图失败");
        vkDestroyImage(this->device, image, nullptr);
        vkFreeMemory(this->device, imageMemory, nullptr);
        vkDestroyBuffer(this->device, stagingBuffer, nullptr);
        vkFreeMemory(this->device, stagingMemory, nullptr);
        return handle;
    }

    // 3) 上传：布局转换 -> 拷贝 -> 转成着色器只读。
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = this->queueFamily;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    bool uploadOk = vkCreateCommandPool(this->device, &poolInfo, nullptr, &commandPool) == VK_SUCCESS;

    if (uploadOk)
    {
        VkCommandBufferAllocateInfo allocateInfo = {};
        allocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool        = commandPool;
        allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;
        uploadOk = vkAllocateCommandBuffers(this->device, &allocateInfo, &commandBuffer) == VK_SUCCESS;
    }
    if (uploadOk)
    {
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        uploadOk = vkCreateFence(this->device, &fenceInfo, nullptr, &fence) == VK_SUCCESS;
    }
    if (uploadOk)
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        uploadOk = vkBeginCommandBuffer(commandBuffer, &beginInfo) == VK_SUCCESS;
    }

    if (uploadOk)
    {
        VkImageMemoryBarrier toTransfer = {};
        toTransfer.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.srcAccessMask               = 0;
        toTransfer.dstAccessMask               = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout                   = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout                   = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image                       = image;
        toTransfer.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.baseMipLevel   = 0;
        toTransfer.subresourceRange.levelCount     = 1;
        toTransfer.subresourceRange.baseArrayLayer = 0;
        toTransfer.subresourceRange.layerCount     = 1;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
            nullptr, 0, nullptr, 1, &toTransfer);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset                    = 0;
        copyRegion.bufferRowLength                 = 0;
        copyRegion.bufferImageHeight               = 0;
        copyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel       = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount     = 1;
        copyRegion.imageOffset.x                   = 0;
        copyRegion.imageOffset.y                   = 0;
        copyRegion.imageOffset.z                   = 0;
        copyRegion.imageExtent.width               = (uint32_t)width;
        copyRegion.imageExtent.height              = (uint32_t)height;
        copyRegion.imageExtent.depth               = 1;
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
            &copyRegion);

        VkImageMemoryBarrier toShader = toTransfer;
        toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toShader.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toShader.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toShader);

        uploadOk = vkEndCommandBuffer(commandBuffer) == VK_SUCCESS;
    }

    if (uploadOk)
    {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &commandBuffer;
        uploadOk = vkQueueSubmit(this->queue, 1, &submitInfo, fence) == VK_SUCCESS;
    }
    if (uploadOk)
        uploadOk = vkWaitForFences(this->device, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;

    if (fence != VK_NULL_HANDLE)
        vkDestroyFence(this->device, fence, nullptr);
    if (commandPool != VK_NULL_HANDLE)
        vkDestroyCommandPool(this->device, commandPool, nullptr); // 顺带释放命令缓冲
    vkDestroyBuffer(this->device, stagingBuffer, nullptr);
    vkFreeMemory(this->device, stagingMemory, nullptr);

    if (!uploadOk)
    {
        Logger::error("上传 Vulkan 纹理失败");
        vkDestroyImageView(this->device, view, nullptr);
        vkDestroyImage(this->device, image, nullptr);
        vkFreeMemory(this->device, imageMemory, nullptr);
        return handle;
    }

    // 4) 注册给 ImGui，拿到 VkDescriptorSet —— 它就是 ImTextureID。
    VkDescriptorSet descriptorSet = ImGui_ImplVulkan_AddTexture(view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (descriptorSet == VK_NULL_HANDLE)
    {
        Logger::error("注册 Vulkan 纹理描述符失败");
        vkDestroyImageView(this->device, view, nullptr);
        vkDestroyImage(this->device, image, nullptr);
        vkFreeMemory(this->device, imageMemory, nullptr);
        return handle;
    }

    TextureRecord* record = new TextureRecord();
    record->image  = image;
    record->memory = imageMemory;
    record->view   = view;
    record->set    = descriptorSet;
    this->textures.push_back(record);

    handle.texture  = (ImTextureID)(uintptr_t)descriptorSet;
    handle.userData = record;
    handle.width    = width;
    handle.height   = height;
    return handle;
}

// 就地更新纹理像素（动图播放用）。
//
// 步骤和 createTexture 里的上传段同构（staging → 屏障 → 拷贝 → 屏障），但**故意不共用**：
// 那段是已经在真机上跑通的路径，为了这次需求去拆它会带来无谓的风险。这里只做"更新"，
// 不碰图像/视图/描述符集的生命周期。
bool VulkanBackend::updateTexture(TextureHandle& handle, const unsigned char* pixels, int width,
    int height)
{
    if (!this->ready || pixels == nullptr || width <= 0 || height <= 0)
        return false;

    if (handle.userData == nullptr || handle.width != width || handle.height != height)
        return false;

    TextureRecord* record = static_cast<TextureRecord*>(handle.userData);

    const VkDeviceSize byteSize = (VkDeviceSize)width * (VkDeviceSize)height * 4;

    // 1) 暂存缓冲
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = byteSize;
    bufferInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(this->device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS)
    {
        Logger::error("更新纹理：创建暂存缓冲失败");
        return false;
    }

    VkMemoryRequirements bufferRequirements = {};
    vkGetBufferMemoryRequirements(this->device, stagingBuffer, &bufferRequirements);

    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(this->physical, &memoryProperties);

    uint32_t bufferType = UINT32_MAX;
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        const bool hasType = (bufferRequirements.memoryTypeBits & (1u << i)) != 0;
        const bool hostVisible =
            (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
        if (hasType && hostVisible)
        {
            bufferType = i;
            break;
        }
    }
    if (bufferType == UINT32_MAX)
    {
        Logger::error("更新纹理：找不到主机可见内存");
        vkDestroyBuffer(this->device, stagingBuffer, nullptr);
        return false;
    }

    VkMemoryAllocateInfo bufferAllocate = {};
    bufferAllocate.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bufferAllocate.allocationSize  = bufferRequirements.size;
    bufferAllocate.memoryTypeIndex = bufferType;

    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (vkAllocateMemory(this->device, &bufferAllocate, nullptr, &stagingMemory) != VK_SUCCESS)
    {
        Logger::error("更新纹理：分配暂存内存失败");
        vkDestroyBuffer(this->device, stagingBuffer, nullptr);
        return false;
    }
    vkBindBufferMemory(this->device, stagingBuffer, stagingMemory, 0);

    void* mapped = nullptr;
    if (vkMapMemory(this->device, stagingMemory, 0, byteSize, 0, &mapped) != VK_SUCCESS)
    {
        Logger::error("更新纹理：映射暂存内存失败");
        vkDestroyBuffer(this->device, stagingBuffer, nullptr);
        vkFreeMemory(this->device, stagingMemory, nullptr);
        return false;
    }
    std::memcpy(mapped, pixels, (size_t)byteSize);
    vkUnmapMemory(this->device, stagingMemory);

    // 2) 一次性命令缓冲：SHADER_READ_ONLY -> TRANSFER_DST -> 拷贝 -> SHADER_READ_ONLY
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = this->queueFamily;

    VkCommandPool commandPool     = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence fence                 = VK_NULL_HANDLE;

    bool ok = vkCreateCommandPool(this->device, &poolInfo, nullptr, &commandPool) == VK_SUCCESS;
    if (ok)
    {
        VkCommandBufferAllocateInfo allocateInfo = {};
        allocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool        = commandPool;
        allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;
        ok = vkAllocateCommandBuffers(this->device, &allocateInfo, &commandBuffer) == VK_SUCCESS;
    }
    if (ok)
    {
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        ok = vkCreateFence(this->device, &fenceInfo, nullptr, &fence) == VK_SUCCESS;
    }
    if (ok)
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        ok = vkBeginCommandBuffer(commandBuffer, &beginInfo) == VK_SUCCESS;
    }

    if (ok)
    {
        VkImageMemoryBarrier toTransfer = {};
        toTransfer.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.srcAccessMask       = VK_ACCESS_SHADER_READ_BIT;
        toTransfer.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toTransfer.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image               = record->image;
        toTransfer.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.baseMipLevel   = 0;
        toTransfer.subresourceRange.levelCount     = 1;
        toTransfer.subresourceRange.baseArrayLayer = 0;
        toTransfer.subresourceRange.layerCount     = 1;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset                    = 0;
        copyRegion.bufferRowLength                 = 0;
        copyRegion.bufferImageHeight               = 0;
        copyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel       = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount     = 1;
        copyRegion.imageExtent.width               = (uint32_t)width;
        copyRegion.imageExtent.height              = (uint32_t)height;
        copyRegion.imageExtent.depth               = 1;
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, record->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        VkImageMemoryBarrier toShader = toTransfer;
        toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toShader.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toShader.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toShader);

        ok = vkEndCommandBuffer(commandBuffer) == VK_SUCCESS;
    }

    if (ok)
    {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &commandBuffer;
        ok = vkQueueSubmit(this->queue, 1, &submitInfo, fence) == VK_SUCCESS;
    }
    if (ok)
        ok = vkWaitForFences(this->device, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;

    if (fence != VK_NULL_HANDLE)
        vkDestroyFence(this->device, fence, nullptr);
    if (commandPool != VK_NULL_HANDLE)
        vkDestroyCommandPool(this->device, commandPool, nullptr);
    vkDestroyBuffer(this->device, stagingBuffer, nullptr);
    vkFreeMemory(this->device, stagingMemory, nullptr);

    if (!ok)
        Logger::error("纹理就地更新失败（Vulkan）");

    return ok;
}

void VulkanBackend::destroyTexture(TextureHandle& handle)
{
    if (handle.texture == ImTextureID_Invalid || handle.userData == nullptr || this->device == VK_NULL_HANDLE)
    {
        handle = TextureHandle();
        return;
    }

    // ImGui 可能刚提交过用到这张纹理的帧，先等设备空转再释放。
    vkDeviceWaitIdle(this->device);

    TextureRecord* record = static_cast<TextureRecord*>(handle.userData);
    ImGui_ImplVulkan_RemoveTexture(record->set);
    vkDestroyImageView(this->device, record->view, nullptr);
    vkDestroyImage(this->device, record->image, nullptr);
    vkFreeMemory(this->device, record->memory, nullptr);

    this->textures.erase(std::remove(this->textures.begin(), this->textures.end(), record), this->textures.end());
    delete record;

    handle = TextureHandle();
}

} // namespace bklv

#endif // !BKLV_SWITCH && BKLV_VULKAN
