# bklv —— 界面库本身的构建
#
# 这个目录是**自包含**的：库代码（core / views / backends / platforms）与它需要的
# 第三方（3rd/）都在这里。把整个 bklv/ 拷到别的项目里 add_subdirectory 就能用：
#
#     add_subdirectory(third_party/bklv)
#     target_link_libraries(my_app PRIVATE bklv::bklv)
#
# 包含路径由 bklv::bklv 自带（把 bklv/ 的父目录也带上了），所以调用方可以直接写
#     #include "bklv/core/app.hpp"
# 不需要自己加 include 目录。
#
# 两个影响构建的变量，独立使用（没有外层工程）时本文件会自己给默认值：
#   BKLV_HOST      平台：macos / linux / windows。决定图片解码方案与链接参数。
#   BKLV_GRAPHICS  图形后端：gl / vk。编译期决定，约定见 AGENTS.md。

# ---------------------------------------------------------------------------
# 路径与变量
# ---------------------------------------------------------------------------

set(BKLV_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
# 含 bklv/ 的那一层目录：调用方靠它写 #include "bklv/..."
get_filename_component(BKLV_INCLUDE_ROOT "${BKLV_DIR}/.." ABSOLUTE)
set(BKLV_3RD "${BKLV_DIR}/3rd")

# 独立配置时（直接在 bklv/ 里跑 cmake）需要自己的 project()。被 add_subdirectory
# 引入时 CMAKE_SOURCE_DIR 指向外层工程，就不会重复调用。
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # 版本要求与仓库根一致，理由见根 CMakeLists.txt（SDL3 需要 3.24）
    cmake_minimum_required(VERSION 3.16)
    if(CMAKE_VERSION VERSION_LESS 3.24)
        message(FATAL_ERROR "bklv 需要 CMake ≥ 3.24，当前是 ${CMAKE_VERSION}")
    endif()
    project(bklv LANGUAGES C CXX)
endif()

if(NOT DEFINED BKLV_HOST)
    if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "MSYS" OR CMAKE_SYSTEM_NAME STREQUAL "MINGW")
        set(BKLV_HOST "windows")
    elseif(APPLE)
        set(BKLV_HOST "macos")
    else()
        set(BKLV_HOST "linux")
    endif()
endif()

if(NOT DEFINED BKLV_GRAPHICS)
    set(BKLV_GRAPHICS "gl" CACHE STRING "图形后端：gl 或 vk")
endif()
set_property(CACHE BKLV_GRAPHICS PROPERTY STRINGS gl vk)

# ---------------------------------------------------------------------------
# 子模块完整性
# ---------------------------------------------------------------------------

foreach(required IN ITEMS
        "imgui/imgui.h"
        "yoga/yoga/Yoga.h"
        "tweeny/include/tweeny/tweeny.h"
        "SDL/include/SDL3/SDL.h")
    if(NOT EXISTS "${BKLV_3RD}/${required}")
        message(FATAL_ERROR
            "bklv/3rd/${required} 不存在，子模块不完整。请先执行：\n"
            "  git submodule update --init --recursive")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# SDL3：用 SDK 自己的 CMake，编成静态库随仓库一起构建
# ---------------------------------------------------------------------------

set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL_TESTS OFF CACHE BOOL "" FORCE)

if(BKLV_HOST STREQUAL "linux")
    # SDL3 有一堆可选系统依赖（X11 扩展、各类音频后端…）。
    # 缺哪个就关哪个，这样在任何 Linux 上都能编过，而不是要求先装一长串 -dev 包。
    foreach(ext IN ITEMS XCURSOR XDBE XFIXES XINPUT XRANDR XSCRNSAVER XSHAPE XSYNC XTEST)
        set(SDL_X11_${ext} OFF CACHE BOOL "" FORCE)
    endforeach()

    find_package(PkgConfig QUIET)
    foreach(backend IN ITEMS ALSA PULSEAUDIO PIPEWIRE JACK SNDIO)
        set(pkg_name "")
        if(backend STREQUAL "ALSA")
            set(pkg_name "alsa")
        elseif(backend STREQUAL "PULSEAUDIO")
            set(pkg_name "libpulse")
        elseif(backend STREQUAL "PIPEWIRE")
            set(pkg_name "libpipewire-0.3")
        elseif(backend STREQUAL "JACK")
            set(pkg_name "jack")
        else()
            set(pkg_name "sndio")
        endif()

        set(have_backend OFF)
        if(PkgConfig_FOUND)
            pkg_check_modules(BKLV_PC_${backend} QUIET ${pkg_name})
            if(BKLV_PC_${backend}_FOUND)
                set(have_backend ON)
            endif()
        endif()
        set(SDL_${backend} ${have_backend} CACHE BOOL "" FORCE)
    endforeach()

    foreach(feature IN ITEMS SDL_CAMERA SDL_LIBUSB SDL_IBUS SDL_FCITX SDL_WAYLAND_LIBDECOR)
        set(${feature} OFF CACHE BOOL "" FORCE)
    endforeach()
endif()

add_subdirectory("${BKLV_3RD}/SDL" EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# 公共编译选项
# ---------------------------------------------------------------------------

add_library(bklv_opts INTERFACE)

target_include_directories(bklv_opts INTERFACE
    "${BKLV_INCLUDE_ROOT}"          # 让调用方能写 #include "bklv/..."
    "${BKLV_3RD}/imgui"
    "${BKLV_3RD}/yoga"
    "${BKLV_3RD}/tweeny/include"
    "${BKLV_3RD}/stb"
    "${BKLV_3RD}/json/single_include"   # nlohmann/json：单头文件、全 inline，不需要编源文件
)

target_compile_options(bklv_opts INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-DGL_SILENCE_DEPRECATION>
)

# C++17 必须在这里声明，不能只写在可执行目标上。
# bklv_core / bklv_imgui 是 OBJECT 库，不受可执行目标的 target_compile_features
# 影响；不声明的话它们用编译器默认标准——Apple clang 21 的默认是 C++14
# （__cplusplus = 201402L），std::clamp 这类 C++17 的东西会直接编不过。
# bklv_yoga 另外要求 cxx_std_20，CMake 取较高者，不冲突。
target_compile_features(bklv_opts INTERFACE cxx_std_17)

# ImGui 的 OpenGL3 后端由 bklv/backends/imgui_impl_opengl3_wrapper.cpp 提供，
# 它自己先包含 bklv 的 GL 头，再包含 ImGui 的实现文件，这里只告诉 ImGui
# 别用它自带的 loader。
target_compile_definitions(bklv_opts INTERFACE IMGUI_IMPL_OPENGL_LOADER_CUSTOM)

# macOS 用系统 ImageIO 解码图片；其余平台用 stb_image（随仓库，无外部依赖）。
if(NOT BKLV_HOST STREQUAL "macos")
    target_compile_definitions(bklv_opts INTERFACE BKLV_USE_STB_IMAGE)
else()
    # ImageIO / CoreGraphics 是 macOS 的系统框架，必须显式链接：
    # 不链的话 image_decoder.cpp 里的 CGImageSource* 会以 undefined symbol 形式
    # 在链接期才暴露（编译期完全正常）。SDL3 的 target 只带它自己需要的框架。
    target_link_libraries(bklv_opts INTERFACE
        "-framework ImageIO"
        "-framework CoreGraphics"
        "-framework CoreFoundation"
        "-framework CoreMedia"
        "-framework CoreVideo"
    )
endif()

# OpenGL 库：两种后端都要链。
# macOS 是 OpenGL.framework，Linux 是 libGL（FindOpenGL 还会带上 GLX 的依赖）。
# vk 构建也要，因为 ImGui 的 OpenGL3 后端（imgui_impl_opengl3_wrapper.cpp）与
# Windows 的 GL 运行时加载都在库里，不区分后端。
find_package(OpenGL REQUIRED)
target_link_libraries(bklv_opts INTERFACE OpenGL::GL)

# 图形后端选择（编译期）。
if(BKLV_GRAPHICS STREQUAL "vk")
    target_compile_definitions(bklv_opts INTERFACE BKLV_VULKAN)
elseif(BKLV_GRAPHICS STREQUAL "gl")
    target_compile_definitions(bklv_opts INTERFACE BKLV_OPENGL)
else()
    message(FATAL_ERROR "BKLV_GRAPHICS 只能是 gl 或 vk（当前：${BKLV_GRAPHICS}）")
endif()

if(BKLV_HOST STREQUAL "windows")
    # 静态链接 libgcc / libstdc++，产物不带 MinGW 运行时 DLL 依赖。
    target_compile_options(bklv_opts INTERFACE -static-libgcc -static-libstdc++)
    target_link_options(bklv_opts INTERFACE -static-libgcc -static-libstdc++)
endif()

# ---------------------------------------------------------------------------
# Vulkan：头文件 + loader
# ---------------------------------------------------------------------------

if(BKLV_GRAPHICS STREQUAL "vk")
    # 用 CMake 自带的查找模块，它已经认识 LunarG SDK 与 MSYS2 两种布局
    # （含 MinGW 的 libvulkan-1.dll.a）。不要请求 glslc 组件：
    # ImGui 的 Vulkan 后端把 shader 内嵌在源码里，不需要 glslc，
    # 而 MSYS2 的 vulkan-headers/loader 包里也没有它。
    find_package(Vulkan REQUIRED)

    # 必须挂在 bklv_opts 上，不能只链在可执行目标上：
    # vulkan_backend.cpp / imgui_impl_vulkan.cpp 是在 bklv_core、bklv_imgui 这两个
    # OBJECT 库里编译的，可执行目标的链接依赖影响不到它们的 include 路径，
    # 结果就是 GL 构建好好的、vk 构建报 'vulkan/vulkan.h' file not found。
    # 挂在这里顺带把 loader 也带出去，调用方不需要再单独链一次。
    target_link_libraries(bklv_opts INTERFACE Vulkan::Vulkan)
endif()

# ---------------------------------------------------------------------------
# Yoga：单独用 C++20 编译
# ---------------------------------------------------------------------------
#
# ---------------------------------------------------------------------------
# 可选图片解码器：WebP（libwebp）与 SVG（lunasvg + plutovg）
#
# 两者都是**可选**的：子模块没拉下来（或显式关掉）时配置阶段只给一条提示、不报错，
# 编译出来的库里对应的分支会明确说"这个构建没有编 X 解码"。这样别人 clone 完
# 没跑 `git submodule update --init --recursive` 也能编过。
# ---------------------------------------------------------------------------

option(BKLV_WITH_WEBP "启用 WebP 解码（需要 bklv/3rd/webp 子模块）" ON)
option(BKLV_WITH_SVG "启用 SVG 栅格化（需要 bklv/3rd/lunasvg + bklv/3rd/plutovg 子模块）" ON)

if(BKLV_WITH_WEBP AND EXISTS "${BKLV_3RD}/webp/CMakeLists.txt")
    # 只要解码器 + demux（动图）：编码器、命令行工具、示例全关 —— Switch 上省体积，
    # 也避免它们把 -Wall 的警告带进来。
    foreach(_webp_option
        WEBP_BUILD_ANIM_UTILS WEBP_BUILD_CWEBP WEBP_BUILD_DWEBP WEBP_BUILD_GIF2WEBP
        WEBP_BUILD_IMG2WEBP WEBP_BUILD_VWEBP WEBP_BUILD_WEBPINFO WEBP_BUILD_WEBPMUX
        WEBP_BUILD_LIBWEBPMUX WEBP_BUILD_EXTRAS WEBP_BUILD_WEBP_JS WEBP_BUILD_FUZZTEST
        WEBP_USE_THREAD)
        set(${_webp_option} OFF CACHE BOOL "" FORCE)
    endforeach()
    add_subdirectory("${BKLV_3RD}/webp" "${CMAKE_CURRENT_BINARY_DIR}/webp" EXCLUDE_FROM_ALL)
    target_link_libraries(bklv_opts INTERFACE webp webpdemux)
    target_compile_definitions(bklv_opts INTERFACE BKLV_USE_WEBP)
    message(STATUS "bklv：WebP 解码已启用（libwebp 解码器 + demux）")
elseif(BKLV_WITH_WEBP)
    message(STATUS "bklv：没找到 bklv/3rd/webp，WebP 解码关闭（git submodule update --init --recursive）")
endif()

if(BKLV_WITH_SVG
    AND EXISTS "${BKLV_3RD}/lunasvg/source/lunasvg.cpp"
    AND EXISTS "${BKLV_3RD}/plutovg/source/plutovg-canvas.c")
    # **不用 lunasvg 自己的 CMakeLists**：它会用 FetchContent 去网上拉 plutovg，
    # 配置阶段要联网、还多一个隐藏依赖。这里按源文件自己编，plutovg 也是我们的子模块，
    # 于是整个配置/编译过程完全离线。
    file(GLOB_RECURSE BKLV_LUNASVG_SOURCES CONFIGURE_DEPENDS "${BKLV_3RD}/lunasvg/source/*.cpp")
    file(GLOB_RECURSE BKLV_PLUTOVG_SOURCES CONFIGURE_DEPENDS "${BKLV_3RD}/plutovg/source/*.c")

    add_library(bklv_svg STATIC ${BKLV_LUNASVG_SOURCES} ${BKLV_PLUTOVG_SOURCES})
    target_include_directories(bklv_svg
        PUBLIC "${BKLV_3RD}/lunasvg/include"
        PRIVATE "${BKLV_3RD}/lunasvg/source" "${BKLV_3RD}/plutovg/include"
            "${BKLV_3RD}/plutovg/source")
    # 和它们自己 CMakeLists 的静态构建定义保持一致
    target_compile_definitions(bklv_svg PRIVATE
        LUNASVG_BUILD LUNASVG_BUILD_STATIC PLUTOVG_BUILD PLUTOVG_BUILD_STATIC)
    set_target_properties(bklv_svg PROPERTIES
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON C_STANDARD 11 C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(bklv_opts INTERFACE bklv_svg)
    target_compile_definitions(bklv_opts INTERFACE BKLV_USE_SVG)
    message(STATUS "bklv：SVG 支持已启用（lunasvg + plutovg）")
elseif(BKLV_WITH_SVG)
    message(STATUS "bklv：没找到 bklv/3rd/lunasvg（或 plutovg），SVG 支持关闭")
endif()

# 不用 add_subdirectory(3rd/yoga/yoga)：它 include 的 cmake/project-defaults.cmake
# 会在目录作用域里 add_compile_options(-Wall -Werror -fno-rtti) 并
# set(CMAKE_CXX_STANDARD 20)，把 -Werror 传染给整个工程、破坏零警告约定。

file(GLOB_RECURSE YOGA_SOURCES CONFIGURE_DEPENDS "${BKLV_3RD}/yoga/yoga/*.cpp")

add_library(bklv_yoga STATIC ${YOGA_SOURCES})
target_link_libraries(bklv_yoga PUBLIC bklv_opts)
target_include_directories(bklv_yoga PUBLIC "${BKLV_3RD}/yoga")
target_compile_features(bklv_yoga PUBLIC cxx_std_20)

# ---------------------------------------------------------------------------
# ImGui：核心 + 平台/渲染后端
# ---------------------------------------------------------------------------
#
# 用 OBJECT 库而不是 STATIC：编进静态库后，链接器会按需抽取目标文件，
# 可能丢掉 ImGui 里靠静态初始化注册的东西。OBJECT 库的产物会被原样带进
# 每个使用者的链接命令行里，行为和"sources 内联进目标"一致。

set(BKLV_IMGUI_SOURCES
    "${BKLV_3RD}/imgui/imgui.cpp"
    "${BKLV_3RD}/imgui/imgui_draw.cpp"
    "${BKLV_3RD}/imgui/imgui_tables.cpp"
    "${BKLV_3RD}/imgui/imgui_widgets.cpp"
    "${BKLV_3RD}/imgui/backends/imgui_impl_sdl3.cpp"
    "${BKLV_DIR}/backends/imgui_impl_opengl3_wrapper.cpp"
)

if(BKLV_GRAPHICS STREQUAL "vk")
    # 只有 vk 构建需要它，GL 构建不必多编一份。
    list(APPEND BKLV_IMGUI_SOURCES "${BKLV_3RD}/imgui/backends/imgui_impl_vulkan.cpp")
endif()

add_library(bklv_imgui OBJECT ${BKLV_IMGUI_SOURCES})
target_link_libraries(bklv_imgui PUBLIC bklv_opts SDL3::SDL3-static)

# ---------------------------------------------------------------------------
# 库本体
# ---------------------------------------------------------------------------
#
# 注意这里**不是** STATIC 库：上面几个 OBJECT 库直接挂在 INTERFACE 上，
# 使用者的链接命令行里会带上全部目标文件。做成 .a 会有上面说的 ImGui 静态初始化
# 被丢掉的风险，也会让"头文件 + 静态库"两处分发成为负担。
#
# 因此 bklv::bklv 目前只支持 add_subdirectory 方式引入。
# 真正的 STATIC 库 + install()/find_package 见 TODO.md 的 P0，那里要一并解决
# ImGui 静态初始化的问题。

set(BKLV_CORE_SOURCES
    core/globals.cpp
    core/frame_budget.cpp
    core/frame_stats.cpp
    core/focus_ring.cpp
    core/gradient.cpp
    core/transition_player.cpp
    core/glass/glass_blur_plan.cpp
    core/glass/glass_geometry.cpp
    core/glass/glass_queue.cpp
    core/glass/glass_tuning.cpp
    core/glass/glass_service.cpp
    backends/gl_glass.cpp
    backends/vk_glass.cpp
    backends/gl_glass_procs.cpp
    backends/gl_glass_shaders.cpp
    core/logger.cpp
    core/input.cpp
    core/shadow.cpp
    core/task.cpp
    core/time.cpp
    core/font.cpp
    core/icon_font.cpp
    core/event.cpp
    core/gesture.cpp
    core/graphics.cpp
    core/decode_queue.cpp
    core/image_decoder.cpp
    core/image_decoder_gif.cpp
    core/image_decoder_webp.cpp
    core/image_decoder_svg.cpp
    core/image_writer.cpp
    core/resource.cpp
    core/i18n.cpp
    core/attributes.cpp
    compile/view_parse.cpp
    compile/theme_parse.cpp
    core/ime.cpp
    core/ui_scale.cpp
    core/appearance.cpp
    compile/json_util.cpp
    compile/json_vars.cpp
    compile/json_policy.cpp
    compile/layer_registry.cpp
    compile/appearance_box.cpp
    compile/appearance_gradient.cpp
    compile/appearance_imports.cpp
    compile/appearance_layer_fields.cpp
    compile/appearance_layer_meta.cpp
    compile/appearance_children.cpp
    core/audio.cpp
    core/platform.cpp
    core/gamepad.cpp
    core/view.cpp
    core/box.cpp
    core/app.cpp
    views/label.cpp
    views/scrolling_frame.cpp
    views/h_scrolling_frame.cpp
    views/json_cell.cpp
    views/recycler.cpp
    views/data_source.cpp
    views/item_binder.cpp
    views/debug_layer.cpp
    views/image.cpp
    views/padding.cpp
    views/progress_spinner.cpp
    views/debug_log_view.cpp
    views/fps_overlay.cpp
    views/preset_backdrop.cpp
    views/scroll_box.cpp
    # widgets/：外观驱动的控件（绘制结构由主题 JSON 描述，见 core/appearance.hpp）
    widgets/account.cpp
    widgets/appearance_view.cpp
    widgets/appearance_children.cpp
    widgets/layer_draw_rect.cpp
    widgets/layer_draw_shadow.cpp
    widgets/layer_draw_glass.cpp
    widgets/layer_draw_text.cpp
    widgets/layer_draw_image.cpp
    widgets/layer_draw_custom.cpp
    widgets/recycler_view.cpp
    widgets/notification.cpp
    widgets/rectangle.cpp
    widgets/button.cpp
    widgets/section.cpp
    widgets/toast.cpp
    widgets/form_buttons.cpp
    widgets/selector_button.cpp
    widgets/header.cpp
    widgets/bottom_bar.cpp
    widgets/cell.cpp
    widgets/slider.cpp
    widgets/tab_frame.cpp
    widgets/status_area.cpp
    widgets/widget_types.cpp
    widgets/dialog.cpp
    widgets/dropdown.cpp
    widgets/focus_ring_view.cpp
    widgets/placement.cpp
    widgets/backdrop_manager.cpp
    widgets/content_stack.cpp
    widgets/shell.cpp
    widgets/pager.cpp
    widgets/edit_text_dialog.cpp
    widgets/hints_bar.cpp
    platforms/sdl3_platform.cpp
    platforms/sdl3_gamepad.cpp
    backends/gl_loader.cpp
    backends/opengl_backend.cpp
    backends/vulkan_backend.cpp
)

# ---------------------------------------------------------------------------
# 源文件清单漏项检查
# ---------------------------------------------------------------------------
#
# AGENTS.md 里写着"加了新 .cpp 要加进列表，漏一个会以 undefined reference 的形式在
# 链接期才暴露"。这条在 configure 阶段就能查：core / views / widgets 是两个平台
# 共用的，必须是列全的（backends / platforms 有平台分支，各脚本按需收集，不查）。
set(_bklv_listed "")
foreach(_src IN LISTS BKLV_CORE_SOURCES BKLV_IMGUI_SOURCES)
    get_filename_component(_abs "${CMAKE_CURRENT_SOURCE_DIR}/${_src}" ABSOLUTE)
    list(APPEND _bklv_listed "${_abs}")
endforeach()

file(GLOB_RECURSE _bklv_found
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/views/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/widgets/*.cpp")

foreach(_file ${_bklv_found})
    if(NOT _file IN_LIST _bklv_listed)
        file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_file}")
        message(FATAL_ERROR
            "bklv/CMakeLists.txt 的源文件清单里没有 ${_rel}；"
            "加进 BKLV_CORE_SOURCES 或 BKLV_IMGUI_SOURCES（漏了只会在链接期报 undefined reference）")
    endif()
endforeach()


# 源文件列表用相对路径写，这里补成绝对路径，方便核对。
set(BKLV_CORE_ABS "")
foreach(src IN LISTS BKLV_CORE_SOURCES)
    list(APPEND BKLV_CORE_ABS "${BKLV_DIR}/${src}")
endforeach()

add_library(bklv_core OBJECT ${BKLV_CORE_ABS})
find_package(Threads REQUIRED)
target_link_libraries(bklv_core PUBLIC bklv_opts SDL3::SDL3-static Threads::Threads)

add_library(bklv INTERFACE)
add_library(bklv::bklv ALIAS bklv)

# bklv_core / bklv_imgui 是 OBJECT 库，它们的目标文件只对「直接链接它们的目标」生效，
# **不会**透过 INTERFACE 库传给最终的可执行文件——实测现象是调用方只拿到了
# libbklv_yoga.a 与 libSDL3.a，bklv::* 与 ImDrawList 的符号全是 undefined。
# 所以这里用 $<TARGET_OBJECTS:> 把目标文件显式写进链接接口。
#
# 同时保留对 OBJECT 库本身的链接：它们的使用要求（include 目录、编译选项、SDL3）
# 要靠这条传递。两处不冲突——OBJECT 库通过 INTERFACE 不会重复加目标文件。
target_link_libraries(bklv INTERFACE
    bklv_opts
    bklv_core
    $<TARGET_OBJECTS:bklv_core>
    bklv_imgui
    $<TARGET_OBJECTS:bklv_imgui>
    bklv_yoga
)

# ---------------------------------------------------------------------------
# 状态输出
# ---------------------------------------------------------------------------

string(TOUPPER "${BKLV_GRAPHICS}" BKLV_GRAPHICS_UPPER)
message(STATUS "bklv 库")
message(STATUS "  平台     : ${BKLV_HOST}")
message(STATUS "  后端     : ${BKLV_GRAPHICS_UPPER}")
if(BKLV_GRAPHICS STREQUAL "vk")
    message(STATUS "  Vulkan   : ${Vulkan_INCLUDE_DIRS} / ${Vulkan_LIBRARIES}")
endif()
