add_executable(CyclopsApp
    main.cpp
)

# Link the Core library
target_link_libraries(CyclopsApp PRIVATE CyclopsCore)