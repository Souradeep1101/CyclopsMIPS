# ===================================================================================
#                               CyclopsMIPS App Target
# ===================================================================================

# 1. Native File Dialog Extended (Build from Submodule Source)
# EXCLUDE_FROM_ALL ensures we only compile the components we explicitly link against.
add_subdirectory("${CMAKE_SOURCE_DIR}/third_party/nativefiledialog-extended" "${CMAKE_BINARY_DIR}/nfd_build")

# 2. GLFW (Multi-Platform Windowing Submodule)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_SOURCE_DIR}/third_party/glfw" "${CMAKE_BINARY_DIR}/glfw_build")

# 3. ImGui Static Library Target

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")
add_library(ImGui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(ImGui PUBLIC "${IMGUI_DIR}" "${IMGUI_DIR}/backends" "${CMAKE_SOURCE_DIR}/third_party/glad/include")
target_link_libraries(ImGui PUBLIC glfw)

# 3.5 GLAD (OpenGL Loader)
add_library(glad STATIC "${CMAKE_SOURCE_DIR}/third_party/glad/src/glad.c")
target_include_directories(glad PUBLIC "${CMAKE_SOURCE_DIR}/third_party/glad/include")

# 4. Create CyclopsApp Executable
add_executable(CyclopsApp
    # Core Launcher
    main.cpp
    src/ImGuiApp.cpp
    src/TextEditor.cpp
    src/TextureLoader.cpp
    
    # UI Components & Widgets
    src/UI_PipelineWidget.cpp
    src/UI_MemoryWidget.cpp
    src/UI_RegisterWidget.cpp
    src/UI_EditorWidget.cpp
    src/UI_ArchitectureWidget.cpp
    src/UI_SignalsWidget.cpp
    src/UI_TerminalWidget.cpp
    src/UI_Widgets.cpp
    
    # Cross-Platform Logic Layers
    src/OpenGLDebug.cpp
    src/PlatformUtils.cpp
)

# 5. Target Properties (Enforce Industry-Standard C++23)
target_compile_features(CyclopsApp PRIVATE cxx_std_23)

# 6. Target Include Directories
target_include_directories(CyclopsApp PRIVATE 
    include
    "${CMAKE_SOURCE_DIR}/Core/include"
    "${CMAKE_SOURCE_DIR}/third_party/glad/include" 
    "${IMGUI_DIR}" 
    "${IMGUI_DIR}/backends"
)

# 7. Platform-Specific Compile Definitions
if(WIN32)
    target_compile_definitions(CyclopsApp PRIVATE CYCLOPS_PLATFORM_WINDOWS)
elseif(APPLE)
    target_compile_definitions(CyclopsApp PRIVATE CYCLOPS_PLATFORM_MAC)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(CyclopsApp PRIVATE CYCLOPS_PLATFORM_LINUX)
endif()

# 8. Compiler-Specific Warnings & Errors (Generator Expressions)
target_compile_options(CyclopsApp PRIVATE
    # MSVC Flags: High quality, Warnings as Errors, Modern Preprocessor
    # Suppressing C5054 (Bitwise between different enums) and C4100 (Unused parameters)
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /Zc:preprocessor /wd5054 /wd4100 /wd4189 /wd4244>
    
    # GCC/Clang Flags: Strict Pedantic Compliance, Warnings as Errors
    # Suppressing third-party UI warnings for compatibility across Clang/GCC/AppleClang
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
        -Wall -Wextra -Werror -pedantic 
        -Wno-unused-variable 
        -Wno-unused-but-set-variable 
        -Wno-sign-compare 
        -Wno-missing-field-initializers 
        -Wno-reorder
        -Wno-unused-parameter
        -Wno-deprecated-enum-enum-conversion
        -Wno-sequence-point
    >
)

# 9. Core Linkage
# Note: 'nfd' and 'glfw' are targets exported by their respective submodules
target_link_libraries(CyclopsApp PRIVATE 
    CyclopsCore 
    ImGui 
    nfd
    glfw
    glad
)

# 10. OS-Specific Interface & Framework Linkage
if(APPLE)
    # Required for NFD on macOS (Native UI Hooks)
    target_link_libraries(CyclopsApp PRIVATE "-framework AppKit" "-framework UniformTypeIdentifiers")
elseif(WIN32)
    # Required for NFD on Windows (COM Objects)
    target_link_libraries(CyclopsApp PRIVATE ole32 uuid)
endif()

# 11. Assets Management (Ensure assets follow the binary)
add_custom_command(TARGET CyclopsApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:CyclopsApp>/assets"
    COMMENT "Syncing CyclopsMIPS assets to output directory..."
)