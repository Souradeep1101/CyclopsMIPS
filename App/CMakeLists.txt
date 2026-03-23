add_executable(CyclopsApp
    main.cpp
    src/ImGuiApp.cpp
    src/UI_PipelineWidget.cpp
    src/UI_MemoryWidget.cpp
    src/UI_RegisterWidget.cpp
    src/UI_EditorWidget.cpp
    src/UI_ArchitectureWidget.cpp
    src/UI_SignalsWidget.cpp
    src/TextEditor.cpp
)

# Phase 9: UI Dependencies (GLFW, Glad, ImGui)
find_package(glfw3 CONFIG QUIET)
if (glfw3_FOUND)
    message(STATUS "Found vcpkg glfw3")
else()
    message(STATUS "vcpkg glfw3 not found, falling back to FetchContent...")
    include(FetchContent)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

find_package(glad CONFIG QUIET)
if (glad_FOUND)
    message(STATUS "Found vcpkg glad")
else()
    message(STATUS "vcpkg glad not found, falling back to FetchContent...")
    include(FetchContent)
    FetchContent_Declare(glad
        GIT_REPOSITORY https://github.com/Dav1dde/glad.git
        GIT_TAG v0.1.36
    )
    set(GLAD_PROFILE "core" CACHE STRING "OpenGL profile" FORCE)
    set(GLAD_API "gl=" CACHE STRING "API type/version pairs" FORCE)
    set(GLAD_GENERATOR "c" CACHE STRING "Language to generate the binding for" FORCE)
    FetchContent_MakeAvailable(glad)
endif()

# Define ImGui static library compilation (from Git Submodule)
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
add_library(ImGui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(ImGui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(ImGui PUBLIC glfw)
if (glad_FOUND)
    target_link_libraries(ImGui PUBLIC glad::glad)
else()
    target_link_libraries(ImGui PUBLIC glad)
endif()

# Include directories for CyclopsApp
target_include_directories(CyclopsApp PRIVATE 
    include
    ${IMGUI_DIR} 
    ${IMGUI_DIR}/backends
)

# Link everything together
target_link_libraries(CyclopsApp PRIVATE CyclopsCore ImGui)
if (glad_FOUND)
    target_link_libraries(CyclopsApp PRIVATE glad::glad)
else()
    target_link_libraries(CyclopsApp PRIVATE glad)
endif()