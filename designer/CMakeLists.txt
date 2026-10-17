# 设计器（见 designer/DESIGN.md）。根 CMakeLists 里 add_subdirectory(designer) 进来。
#
#   designer_core  —— 无窗口可测的文档模型（tests/*_test.cpp 通过 TARGET 检查链接它）
#   designer       —— 程序本体（P2 起）
#
# 输出与 demo 同目录（BKLV_OUT_DIR 由根 CMakeLists 设好），资源路径按可执行文件目录解析。

set(DESIGNER_CORE_SOURCES
    core/json_document.cpp
    core/appearance_doc.cpp
    core/save_transaction.cpp
    core/undo_stack.cpp
    core/layer_fields.cpp
    core/tokens_doc.cpp
    core/view_doc.cpp
)

set(DESIGNER_CORE_ABS "")
foreach(src IN LISTS DESIGNER_CORE_SOURCES)
    list(APPEND DESIGNER_CORE_ABS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
endforeach()

add_library(designer_core STATIC ${DESIGNER_CORE_ABS})
target_link_libraries(designer_core PUBLIC bklv::bklv)
target_include_directories(designer_core PUBLIC "${CMAKE_SOURCE_DIR}")

# ---- 程序本体（P2 起）----
set(DESIGNER_UI_SOURCES
    main.cpp
    ui/designer_app.cpp
)
set(DESIGNER_UI_ABS "")
foreach(src IN LISTS DESIGNER_UI_SOURCES)
    list(APPEND DESIGNER_UI_ABS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
endforeach()

add_executable(designer ${DESIGNER_UI_ABS})
target_link_libraries(designer PRIVATE designer_core)
set_target_properties(designer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BKLV_OUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${BKLV_OUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BKLV_OUT_DIR}"
)
bklv_copy_resources(designer)
