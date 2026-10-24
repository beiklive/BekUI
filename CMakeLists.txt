# bklv —— 仓库构建入口
#
# 这里只做三件事：判定平台、定输出目录、编出 examples/ 里的示例程序。
# **库本身的构建在 bklv/CMakeLists.txt**，那个目录是自包含的，可以整目录拷走。
#
# 后端是编译期决定的（见 AGENTS.md），所以「同一份代码编 GL 和 Vulkan 两版」
# 靠两个构建目录实现：
#
#   cmake -S . -B build/gl -DBKLV_GRAPHICS=gl && cmake --build build/gl    # -> dist/
#   cmake -S . -B build/vk -DBKLV_GRAPHICS=vk && cmake --build build/vk    # -> dist/vk/
#
# 用法见 README.md。

# 先只要求 3.16（老版本的 CMake 不会在这里直接硬报错），然后给一条**看得懂**的版本提示：
# 真正需要 3.24 的是 SDL3 子模块 —— 它用 $<LINK_LIBRARY:FRAMEWORK,...>（CMake 3.24 起才有）
# 链接 macOS 的系统框架；tweeny 子模块要 3.23。写死 3.24 的话，用户只会看到
# "CMake 3.24 or higher is required"，看不出是谁要求的、该怎么办。
cmake_minimum_required(VERSION 3.16)

if(CMAKE_VERSION VERSION_LESS 3.24)
    message(FATAL_ERROR
        "本项目需要 CMake ≥ 3.24，当前是 ${CMAKE_VERSION}。\n"
        "  · SDL3 子模块用 $<LINK_LIBRARY:FRAMEWORK,...> 链接 macOS 系统框架（3.24 起支持）\n"
        "  · tweeny 子模块要求 ≥ 3.23\n"
        "升级办法：\n"
        "  · macOS: brew upgrade cmake\n"
        "  · Ubuntu/Debian: 用 Kitware 的 apt 源装新版（发行版自带通常偏旧）\n"
        "  · Windows/MSYS2: pacman -S mingw-w64-x86_64-cmake\n"
        "  · 或者用 CMake 官方安装包 / pip install cmake")
endif()

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

# 默认 Release：MinGW 下不指定构建类型会得到 -O0，和预期的 -O2 差太多。
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "构建类型" FORCE)
endif()

option(BKLV_WINDOWS_GUI "Windows 上不弹控制台窗口（会看不到 BKLV_LOG_* 输出）" OFF)
option(BKLV_BUILD_EXAMPLES "编译 examples/ 下的示例" ON)
set(BKLV_GRAPHICS "gl" CACHE STRING "图形后端：gl 或 vk")
set_property(CACHE BKLV_GRAPHICS PROPERTY STRINGS gl vk)

# ---------------------------------------------------------------------------
# 平台判定
# ---------------------------------------------------------------------------

# 注意不能只看 WIN32：在 MSYS2 的 MSYS 终端里 CMAKE_SYSTEM_NAME 是 MSYS、WIN32 为空，
# 那样 BKLV_HOST 会掉进 linux 分支，Windows 的链接参数与 stb_image 分支就被静默跳过了。
# 生成的仍然是原生 Windows 程序，所以按 Windows 处理。
# bklv/CMakeLists.txt 在没有这个变量时会自己判定，这里设好传下去。
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
# 库
# ---------------------------------------------------------------------------

add_subdirectory(bklv)

# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------
# 资源复制
# ---------------------------------------------------------------------------
#
# 资源按"可执行文件目录"解析（见 bklv/foundation/core/resource.hpp），所以每个可执行文件旁边
# 都要有一份 resources/：从哪个目录启动都能找到。
# 实现注意：**每个输出目录只复制一次**。
# 以前是给每个目标各挂一条 POST_BUILD copy_directory，全部往 dist/resources 写，
# -j 并行时互相踩 → 间歇性失败（`make: *** [all] Error 2`），而且失败后**二进制还是旧的**，
# 于是"改了代码没生效"这种假象会骗过验证。现在改成按输出目录建一个独立目标，
# 其余目标依赖它，复制只发生一次。
function(bklv_copy_resources target_name)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/resources")
        return()
    endif()

    # 所有目标共用 BKLV_OUT_DIR（见文件开头），按它建只复制一次的目标
    set(out_dir "${BKLV_OUT_DIR}")
    if(NOT out_dir)
        set(out_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endif()
    if(NOT out_dir)
        message(FATAL_ERROR "bklv_copy_resources: 输出目录未知（BKLV_OUT_DIR 为空）")
    endif()

    string(MAKE_C_IDENTIFIER "${out_dir}" dir_id)
    set(resource_target "bklv_copy_resources_${dir_id}")

    if(NOT TARGET ${resource_target})
        # 目标目录必须和源码目录分开：这条规则会**先删后拷**。
        if(out_dir STREQUAL "${CMAKE_SOURCE_DIR}"
            OR "${out_dir}/resources" STREQUAL "${CMAKE_SOURCE_DIR}/resources")
            message(FATAL_ERROR "bklv_copy_resources: 输出目录不能是仓库源码目录（${out_dir}）")
        endif()

        # 先删后拷，不是只 copy_directory：copy_directory **只加不删**，主题目录改布局之后
        # 旧布局的文件会一直留在 dist/resources 里（实测留了 45 个：根级 colors.json /
        # metrics.json 与整个 widgets/ 目录）。这些残留会骗过"磁盘 vs theme.json 的 files[]
        # 清单"这类校验（theme_test 第 9 节被它们报了 138 个假失败），也让 dist 里的资源
        # 和仓库不一致（2.4M vs 1.8M）。删的只是构建产物目录，仓库 resources/ 不动。
        add_custom_target(${resource_target}
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${out_dir}/resources"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/resources"
                "${out_dir}/resources"
            COMMENT "同步 resources/ 到 ${out_dir}（先删后拷，清掉已删除的文件）"
            VERBATIM
        )
    endif()

    add_dependencies(${target_name} ${resource_target})
endfunction()

# ---------------------------------------------------------------------------
# 状态输出
# ---------------------------------------------------------------------------

message(STATUS "bklv 仓库")
message(STATUS "  平台     : ${BKLV_HOST}")
message(STATUS "  输出目录 : ${BKLV_OUT_DIR}")
message(STATUS "  可执行   : 由 examples/ 提供（demo 已移除，待重做）")

# ---------------------------------------------------------------------------
# 嵌入示例（仅 GL；VK 构建下宿主后端不同，示例不编）
# ---------------------------------------------------------------------------

# 多配置生成器（Visual Studio / Xcode）默认会再插一层 Release/ 子目录，
# 而资源是按 <exe 目录>/resources 找的，落在 dist/Release 就找不到。
# 所以逐个配置都钉到 BKLV_OUT_DIR。
function(bklv_set_output_dir target_name)
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${BKLV_OUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${BKLV_OUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BKLV_OUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${BKLV_OUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${BKLV_OUT_DIR}")
endfunction()

if(BKLV_BUILD_EXAMPLES AND BKLV_GRAPHICS STREQUAL "gl")
    add_executable(embed_imgui_gl examples/embed_imgui_gl/main.cpp)
    target_link_libraries(embed_imgui_gl PRIVATE bklv::bklv)
    bklv_set_output_dir(embed_imgui_gl)
    bklv_copy_resources(embed_imgui_gl)

    add_executable(multi_context examples/multi_context/main.cpp)
    target_link_libraries(multi_context PRIVATE bklv::bklv)
    bklv_set_output_dir(multi_context)
    bklv_copy_resources(multi_context)
endif()
