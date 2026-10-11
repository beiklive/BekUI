# bklv —— 构建入口（CMake）
#
# 取代原来的 build.sh / build.bat / scripts/build_sdl3.sh。
#
# 后端是编译期决定的，所以「每个 demo 同时能编 GL 和 Vulkan」靠两个构建目录实现：
#
#   cmake -S . -B build/gl -DBKLV_GRAPHICS=gl
#   cmake -S . -B build/vk -DBKLV_GRAPHICS=vk
#
# 产物分别落在 dist/ 与 dist/vk/。因为分目录，可执行文件名不带后端后缀
# （Windows 上带 .exe），和原来的约定一致。
#
# 用法见 README.md 的「构建」一节。

cmake_minimum_required(VERSION 3.24)

# 先不带语言调用一次 project()，只为拿到 CMAKE_SYSTEM_NAME。
# 在 MSYS2 的「MSYS」环境里，CMake 判定的是 CMAKE_SYSTEM_NAME=MSYS 而 WIN32 为空；
# 真正的编译器探测（会失败）发生在带 LANGUAGES 的 project() 里，所以必须先探明平台，
# 再决定要不要自己找工具链。
project(bklv LANGUAGES NONE)

# ---------------------------------------------------------------------------
# MSYS2：编译器兜底
# ---------------------------------------------------------------------------
# MSYS2 里的 cmake 是跑在 MSYS 运行时上的 /usr/bin/cmake.exe。如果 PATH 里只有
# usr/bin 而没有 mingw64/bin，它的自动探测找不到 g++，报
# "No CMAKE_C_COMPILER could be found"——于是「在 MSYS2 shell 里直接 cmake」会失败。
# MINGW64 环境自带 MINGW_PREFIX，这里直接用它把编译器指出来，省掉手动 export PATH。
if(CMAKE_SYSTEM_NAME STREQUAL "MSYS" OR CMAKE_SYSTEM_NAME STREQUAL "MINGW")
    if(DEFINED ENV{MINGW_PREFIX} AND NOT "$ENV{MINGW_PREFIX}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{MINGW_PREFIX}" BKLV_MINGW)
        if(EXISTS "${BKLV_MINGW}/bin/gcc.exe" AND EXISTS "${BKLV_MINGW}/bin/g++.exe")
            # 必须在带 LANGUAGES 的 project() 之前设好，并且写进 cache，
            # 否则会被那一步的系统探测丢掉。
            set(CMAKE_C_COMPILER   "${BKLV_MINGW}/bin/gcc.exe"
                CACHE FILEPATH "C 编译器" FORCE)
            set(CMAKE_CXX_COMPILER "${BKLV_MINGW}/bin/g++.exe"
                CACHE FILEPATH "C++ 编译器" FORCE)
            message(STATUS "MSYS2 工具链：${BKLV_MINGW}")
        endif()
    elseif(NOT DEFINED CMAKE_C_COMPILER)
        message(STATUS
            "在 MSYS2 里没有自动找到 MinGW 工具链。请用「MSYS2 MINGW64」快捷方式"
            "打开终端，或传 -DCMAKE_CXX_COMPILER=<路径>/g++.exe")
    endif()
endif()

# 真正开始探测语言。
project(bklv LANGUAGES C CXX)

# 默认 Release：MinGW 下不指定构建类型会得到 -O0，和原来的 -O2 差太多。
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "构建类型" FORCE)
endif()

option(BKLV_WINDOWS_GUI "Windows 上不弹控制台窗口（会看不到 BKLV_LOG_* 输出）" OFF)
set(BKLV_GRAPHICS "gl" CACHE STRING "图形后端：gl 或 vk")
set_property(CACHE BKLV_GRAPHICS PROPERTY STRINGS gl vk)

# 只编指定的目标，留空=全部。按**完整名字**匹配，可以逗号分隔多个。
#
# 用精确匹配而不是子串匹配：主程序叫 demo，而 demo_01_widgets…demo_07_bklv_showcase
# 都以 demo 开头，子串匹配会让 -DBKLV_TARGETS=demo 把 8 个目标全选上。
# 例：cmake -S . -B build/gl -DBKLV_TARGETS=demo
#     cmake -S . -B build/gl -DBKLV_TARGETS=demo_06_bklv_layout,demo_07_bklv_showcase
set(BKLV_TARGETS "" CACHE STRING "只构建列出的目标（完整名字，逗号分隔；留空=全部）")

# ---------------------------------------------------------------------------
# 平台判定
# ---------------------------------------------------------------------------

# 注意不能只看 WIN32：在 MSYS2 的 MSYS 终端里 CMAKE_SYSTEM_NAME 是 MSYS、WIN32 为空，
# 那样 BKLV_HOST 会掉进 linux 分支，Windows 的链接参数与 stb_image 分支就被静默跳过了。
# 生成的仍然是原生 Windows 程序，所以按 Windows 处理。
if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "MSYS" OR CMAKE_SYSTEM_NAME STREQUAL "MINGW")
    set(BKLV_HOST "windows")
elseif(APPLE)
    set(BKLV_HOST "macos")
else()
    set(BKLV_HOST "linux")
endif()

# 输出目录：GL 走 dist/，Vulkan 走 dist/vk/。
if(BKLV_GRAPHICS STREQUAL "vk")
    set(BKLV_OUT_DIR "${CMAKE_SOURCE_DIR}/dist/vk")
elseif(BKLV_GRAPHICS STREQUAL "gl")
    set(BKLV_OUT_DIR "${CMAKE_SOURCE_DIR}/dist")
else()
    message(FATAL_ERROR "BKLV_GRAPHICS 只能是 gl 或 vk（当前：${BKLV_GRAPHICS}）")
endif()

# ---------------------------------------------------------------------------
# 子模块完整性
# ---------------------------------------------------------------------------

foreach(required IN ITEMS
        "3rd/imgui/imgui.h"
        "3rd/yoga/yoga/Yoga.h"
        "3rd/tweeny/include/tweeny/tweeny.h"
        "3rd/SDL/include/SDL3/SDL.h")
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${required}")
        message(FATAL_ERROR
            "${required} 不存在，子模块不完整。请先执行：\n"
            "  git submodule update --init --recursive")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# SDL3：直接用 SDK 自己的 CMake，静态库，随仓库编译
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
    # 列表与原来的 scripts/build_sdl3.sh 保持一致。
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

add_subdirectory(3rd/SDL EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# 公共编译选项
# ---------------------------------------------------------------------------

add_library(bklv_opts INTERFACE)

target_include_directories(bklv_opts INTERFACE
    "${CMAKE_SOURCE_DIR}/3rd/imgui"
    "${CMAKE_SOURCE_DIR}/3rd/yoga"
    "${CMAKE_SOURCE_DIR}/3rd/tweeny/include"
    "${CMAKE_SOURCE_DIR}/3rd/stb"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/demo"
)

target_compile_options(bklv_opts INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-DGL_SILENCE_DEPRECATION>
)

# C++17 必须在这里声明，不能只写在各可执行目标上。
# bklv_core / bklv_app / imgui_impl 是 OBJECT 库，不受可执行目标的
# target_compile_features 影响；不声明的话它们用编译器默认标准——
# Apple clang 21 的默认是 C++14（__cplusplus = 201402L），
# std::clamp 这类 C++17 的东西会直接编不过。
# yogacore 另外要求 cxx_std_20，CMake 取较高者，不冲突。
target_compile_features(bklv_opts INTERFACE cxx_std_17)

# ImGui 的 OpenGL3 后端由 src/bklv/backends/imgui_impl_opengl3_wrapper.cpp 提供，
# 它自己先包含项目里的 GL 头，再包含 ImGui 的实现文件，这里只告诉 ImGui 别用它自带的 loader。
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
#
# 为什么 vk 构建也要：demo_01~05 是直接用 ImGui + GL 的学习示例（不经过 bklv 的
# RenderBackend），demo_common.h 里就有 glClear 这类调用；ImGui 的 OpenGL3 后端
# （imgui_impl_opengl3_wrapper.cpp）也在 IMGUI_SOURCES 里，不区分后端。
# 不链的话 vk 构建会以一堆 _glXxx undefined symbol 失败。
find_package(OpenGL REQUIRED)
target_link_libraries(bklv_opts INTERFACE OpenGL::GL)

# 图形后端选择（编译期）。
if(BKLV_GRAPHICS STREQUAL "vk")
    target_compile_definitions(bklv_opts INTERFACE BKLV_VULKAN)
else()
    target_compile_definitions(bklv_opts INTERFACE BKLV_OPENGL)
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
    # vulkan_backend.cpp / imgui_impl_vulkan.cpp 是在 bklv_core、imgui_impl 这两个
    # OBJECT 库里编译的，可执行目标的链接依赖影响不到它们的 include 路径，
    # 结果就是 GL 构建好好的、vk 构建报 'vulkan/vulkan.h' file not found。
    # 挂在这里顺带把 loader 也带给各可执行目标，不需要再单独链一次。
    target_link_libraries(bklv_opts INTERFACE Vulkan::Vulkan)
endif()

# ---------------------------------------------------------------------------
# Yoga：单独用 C++20 编译
# ---------------------------------------------------------------------------
#
# 不用 add_subdirectory(3rd/yoga/yoga)：它 include 的 cmake/project-defaults.cmake
# 会在目录作用域里 add_compile_options(-Wall -Werror -fno-rtti) 并
# set(CMAKE_CXX_STANDARD 20)，把 -Werror 传染给整个项目、破坏零警告约定。

file(GLOB_RECURSE YOGA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/3rd/yoga/yoga/*.cpp")

add_library(yogacore STATIC ${YOGA_SOURCES})
target_link_libraries(yogacore PUBLIC bklv_opts)
target_include_directories(yogacore PUBLIC "${CMAKE_SOURCE_DIR}/3rd/yoga")
target_compile_features(yogacore PUBLIC cxx_std_20)

# ---------------------------------------------------------------------------
# ImGui 核心 + 平台/渲染后端实现
# ---------------------------------------------------------------------------
#
# 用 OBJECT 库，而不是 STATIC：原来 build.sh 是把这些源文件内联进每个目标的
# 命令行里编译的，行为是「每个可执行文件都带上全部 ImGui 实现」。
# 静态库会按需抽取目标文件，可能丢掉 ImGui 里靠静态初始化注册的东西。

set(IMGUI_SOURCES
    "${CMAKE_SOURCE_DIR}/3rd/imgui/imgui.cpp"
    "${CMAKE_SOURCE_DIR}/3rd/imgui/imgui_draw.cpp"
    "${CMAKE_SOURCE_DIR}/3rd/imgui/imgui_tables.cpp"
    "${CMAKE_SOURCE_DIR}/3rd/imgui/imgui_widgets.cpp"
    "${CMAKE_SOURCE_DIR}/3rd/imgui/backends/imgui_impl_sdl3.cpp"
    "${CMAKE_SOURCE_DIR}/src/bklv/backends/imgui_impl_opengl3_wrapper.cpp"
)

if(BKLV_GRAPHICS STREQUAL "vk")
    # 只有 vk 构建需要它，GL 构建不必多编一份。
    list(APPEND IMGUI_SOURCES "${CMAKE_SOURCE_DIR}/3rd/imgui/backends/imgui_impl_vulkan.cpp")
endif()

add_library(imgui_impl OBJECT ${IMGUI_SOURCES})
target_link_libraries(imgui_impl PUBLIC bklv_opts SDL3::SDL3-static)

# ---------------------------------------------------------------------------
# bklv 移植层
# ---------------------------------------------------------------------------
#
# 编译一次，各目标复用（对应原来 dist/bklv-objects/<backend>/*.o）。

set(BKLV_CORE_SOURCES
    src/bklv/core/logger.cpp
    src/bklv/core/input.cpp
    src/bklv/core/theme.cpp
    src/bklv/core/time.cpp
    src/bklv/core/font.cpp
    src/bklv/core/graphics.cpp
    src/bklv/core/image_decoder.cpp
    src/bklv/core/platform.cpp
    src/bklv/core/gamepad.cpp
    src/bklv/core/view.cpp
    src/bklv/core/box.cpp
    src/bklv/core/app.cpp
    src/bklv/views/label.cpp
    src/bklv/views/hints.cpp
    src/bklv/views/scrolling_frame.cpp
    src/bklv/views/h_scrolling_frame.cpp
    src/bklv/views/recycler.cpp
    src/bklv/views/image.cpp
    src/bklv/views/progress_spinner.cpp
    src/bklv/views/debug_window.cpp
    src/bklv/platforms/sdl3_platform.cpp
    src/bklv/platforms/sdl3_gamepad.cpp
    src/bklv/backends/gl_loader.cpp
    src/bklv/backends/opengl_backend.cpp
    src/bklv/backends/vulkan_backend.cpp
)

add_library(bklv_core OBJECT ${BKLV_CORE_SOURCES})
target_link_libraries(bklv_core PUBLIC bklv_opts SDL3::SDL3-static)

# 只有主程序用得到 app 层（页面/tab/列表）。demo 用不到，不编进去。
set(BKLV_APP_SOURCES
    src/app/widgets.cpp
    src/app/page_helpers.cpp
    src/app/tab_bar.cpp
    src/app/page_host.cpp
    src/app/pages/page_widgets.cpp
    src/app/pages/page_text.cpp
    src/app/pages/page_layout.cpp
    src/app/pages/list_items.cpp
    src/app/pages/page_list.cpp
)

add_library(bklv_app OBJECT ${BKLV_APP_SOURCES})
target_link_libraries(bklv_app PUBLIC bklv_opts SDL3::SDL3-static)

# ---------------------------------------------------------------------------
# 目标程序
# ---------------------------------------------------------------------------
#
# 每个 demo 都编成 GL 与 Vulkan 两个版本（靠两个构建目录），不写依赖具体图形 API
# 的代码；需要图形能力时走 RenderBackend 接口。
#
# 显式列源文件，不用 GLOB 扫 demo/：加删文件比 glob 的便利更重要，
# 而且 CONFIGURE_DEPENDS 在部分生成器上并不可靠。

set(BKLV_TARGET_LIST
    "demo_01_widgets|demo/demo_01_widgets.cpp|core"
    "demo_02_layout|demo/demo_02_layout.cpp|core"
    "demo_03_custom_widget|demo/demo_03_custom_widget.cpp|core"
    "demo_04_yoga|demo/demo_04_yoga.cpp|core"
    "demo_05_tweeny|demo/demo_05_tweeny.cpp|core"
    "demo_06_bklv_layout|demo/demo_06_bklv_layout.cpp|core"
    "demo_07_bklv_showcase|demo/demo_07_bklv_showcase.cpp|core"
    "demo|src/main.cpp|app"
)

set(BKLV_TARGET_NAMES "")
set(BKLV_MATCHED 0)

foreach(entry IN LISTS BKLV_TARGET_LIST)
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 target_name)
    list(GET parts 1 target_src)
    list(GET parts 2 target_kind)

    if(NOT BKLV_TARGETS STREQUAL "")
        string(REPLACE "," ";" BKLV_TARGETS_LIST "${BKLV_TARGETS}")
        if(NOT target_name IN_LIST BKLV_TARGETS_LIST)
            continue()
        endif()
    endif()
    math(EXPR BKLV_MATCHED "${BKLV_MATCHED} + 1")

    add_executable(${target_name} "${CMAKE_SOURCE_DIR}/${target_src}")

    # 不再单独列 SDL3::SDL3-static：imgui_impl 与 bklv_core 都 PUBLIC 链接了它，
    # 这里再写一次会让链接器报 "ignoring duplicate libraries"（项目要求零警告）。
    target_link_libraries(${target_name} PRIVATE
        bklv_opts
        imgui_impl
        bklv_core
        yogacore
    )

    if(target_kind STREQUAL "app")
        target_link_libraries(${target_name} PRIVATE bklv_app)
    endif()

    # demo_04_yoga 直接用 Yoga 的 C++20 头文件，其余保持 C++17。
    if(target_name STREQUAL "demo_04_yoga")
        target_compile_features(${target_name} PRIVATE cxx_std_20)
    else()
        target_compile_features(${target_name} PRIVATE cxx_std_17)
    endif()

    # Windows 默认保留控制台，BKLV_LOG_* 才看得见（与 macOS / Linux 行为一致）。
    if(BKLV_HOST STREQUAL "windows" AND BKLV_WINDOWS_GUI)
        target_link_options(${target_name} PRIVATE -mwindows)
    endif()

    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${BKLV_OUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${BKLV_OUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BKLV_OUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${BKLV_OUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${BKLV_OUT_DIR}"
    )

    list(APPEND BKLV_TARGET_NAMES ${target_name})
endforeach()

if(NOT BKLV_TARGETS STREQUAL "" AND BKLV_MATCHED EQUAL 0)
    message(FATAL_ERROR
        "BKLV_TARGETS=${BKLV_TARGETS} 没有匹配到任何目标。可用目标：\n"
        "  demo_01_widgets\n  demo_02_layout\n  demo_03_custom_widget\n"
        "  demo_04_yoga\n  demo_05_tweeny\n  demo_06_bklv_layout\n"
        "  demo_07_bklv_showcase\n  demo")
endif()

# ---------------------------------------------------------------------------
# 状态输出
# ---------------------------------------------------------------------------

string(TOUPPER "${BKLV_GRAPHICS}" BKLV_GRAPHICS_UPPER)
list(LENGTH BKLV_TARGET_NAMES BKLV_TARGET_COUNT)

message(STATUS "bklv 配置")
message(STATUS "  平台     : ${BKLV_HOST}")
message(STATUS "  后端     : ${BKLV_GRAPHICS_UPPER}")
message(STATUS "  输出目录 : ${BKLV_OUT_DIR}")
message(STATUS "  目标     : ${BKLV_TARGET_COUNT} 个 —— ${BKLV_TARGET_NAMES}")
if(BKLV_GRAPHICS STREQUAL "vk")
    message(STATUS "  Vulkan   : ${Vulkan_INCLUDE_DIRS} / ${Vulkan_LIBRARIES}")
endif()
