
# vars used to enable subdirectories to extend the build of the vsgImGui library in a loose coupled way
set(EXTRA_DEFINES)
set(EXTRA_INCLUDES)

SET(HEADER_PATH ${VSGIMGUI_SOURCE_DIR}/include/vsgImGui)

set(HEADERS
    ${HEADER_PATH}/imgui.h
    ${HEADER_PATH}/SendEventsToImGui.h
    ${HEADER_PATH}/RenderImGui.h
    ${HEADER_PATH}/Texture.h	
    imgui/imconfig.h
    imgui/imgui_internal.h
    imgui/imstb_rectpack.h
    imgui/imstb_textedit.h
    imgui/imstb_truetype.h
    imgui/misc/cpp/imgui_stdlib.h
    implot/implot.h
    implot/implot_internal.h
    imguifiledialog/ImGuiFileDialog.h
    imguifiledialog/ImGuiFileDialogConfig.h
    imguizmo/ImGuizmo.h
    imguizmo/ImSequencer.h
    imnodes/imnodes.h
    imnodes/imnodes_internal.h
	imtools/AbstractPane.h
	imtools/ImWidgets.h
	imtools/LayoutManager.h
	imtools/Messaging.h
	imtools/ThemeHelper.h	
    fonts/foaw.h
    fonts/Roboto_Medium.h
)

set(SOURCES
    vsgImGui/RenderImGui.cpp
    vsgImGui/SendEventsToImGui.cpp
    vsgImGui/Texture.cpp
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_vulkan.cpp
    imgui/misc/cpp/imgui_stdlib.cpp
    implot/implot.cpp
    implot/implot_demo.cpp
    implot/implot_items.cpp	
    imguifiledialog/ImGuiFileDialog.cpp
    imguizmo/ImGuizmo.cpp
    imnodes/imnodes.cpp
    imguizmo/ImSequencer.cpp
	imtools/ImWidgets.cpp
	imtools/LayoutManager.cpp
	imtools/Messaging.cpp
	imtools/ThemeHelper.cpp
    fonts/foaw.cpp
    fonts/Roboto_Medium.cpp
)

add_library(vsgImGui ${HEADERS} ${SOURCES})

# add definitions to enable building vsgImGui as part of submodule
add_library(vsgImGui::vsgImGui ALIAS vsgImGui)
set(vsgImGui_FOUND TRUE CACHE INTERNAL "vsgImGui found.")
set(CMAKE_DISABLE_FIND_PACKAGE_vsgImGui TRUE CACHE INTERNAL "Disable find_package(vsgImGui) as it's not necessary.")

set(EXTRA_DEFINES "IMGUI_USER_CONFIG=<vsgImGui/Export.h>")

if (ANDROID)
    set(EXTRA_DEFINES {EXTRA_DEFINES} -DPLATFORM_ANDROID)
elseif (WIN32)
    set(EXTRA_DEFINES {EXTRA_DEFINES} -DPLATFORM_WIN32)
elseif (IOS)
    set(EXTRA_DEFINES {EXTRA_DEFINES} -DPLATFORM_IOS)
elseif (APPLE)
    set(EXTRA_DEFINES {EXTRA_DEFINES} -DPLATFORM_APPLE)
else()
    set(EXTRA_DEFINES {EXTRA_DEFINES} -DPLATFORM_LINUX)
endif()

set_property(TARGET vsgImGui PROPERTY VERSION ${VSGIMGUI_VERSION_MAJOR}.${VSGIMGUI_VERSION_MINOR}.${VSGIMGUI_VERSION_PATCH})
set_property(TARGET vsgImGui PROPERTY SOVERSION ${VSGIMGUI_SOVERSION})
set_property(TARGET vsgImGui PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET vsgImGui PROPERTY CXX_STANDARD 17)

target_compile_definitions(vsgImGui PRIVATE 
    ${EXTRA_DEFINES}
    -DIMGUI_DEFINE_MATH_OPERATORS
    -DIMGUI_DISABLE_OBSOLETE_KEYIO
    -DUSE_DECORATIONS_FOR_RESIZE_CHILD_WINDOWS
)

target_include_directories(vsgImGui PUBLIC
    $<BUILD_INTERFACE:${VSGIMGUI_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${VSGIMGUI_SOURCE_DIR}/include/vsgImGui>
    $<INSTALL_INTERFACE:include>
    ${EXTRA_INCLUDES}
)

target_link_libraries(vsgImGui
    PUBLIC
        vsg::vsg
    PRIVATE
        ${EXTRA_LIBRARIES}
)

install(TARGETS vsgImGui ${INSTALL_TARGETS_DEFAULT_FLAGS})

if (BUILD_SHARED_LIBS)
    target_compile_definitions(vsgImGui INTERFACE VSGIMGUI_SHARED_LIBRARY)
endif()


install(DIRECTORY ${VSGIMGUI_SOURCE_DIR}/include/vsgImGui DESTINATION include)

vsg_add_cmake_support_files(
    CONFIG_TEMPLATE
        vsgImGuiConfig.cmake.in
)
