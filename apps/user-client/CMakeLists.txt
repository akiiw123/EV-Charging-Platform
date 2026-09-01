add_executable(charging-user
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
)

target_link_libraries(charging-user PRIVATE charging-core Qt6::Widgets Qt6::Network)
