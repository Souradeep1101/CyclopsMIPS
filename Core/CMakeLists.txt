# Define the library (Static is best for simplicity & performance here)
add_library(CyclopsCore STATIC
    src/CPU.cpp
    src/ALU.cpp
    src/RegisterFile.cpp
    src/DataMemory.cpp
    src/MemoryBus.cpp
    src/Parser.cpp
    src/Assembler.cpp
)

# Define include directories
# PUBLIC = Users of this lib (App) can see these headers
# PRIVATE = Only this lib needs these folders
target_include_directories(CyclopsCore
    PUBLIC include
    PRIVATE src
)

# Compiler Warnings (Crucial for Systems Engineering)
if(MSVC)
    target_compile_options(CyclopsCore PRIVATE /W4 /WX) # /WX treats warnings as errors
else()
    target_compile_options(CyclopsCore PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()