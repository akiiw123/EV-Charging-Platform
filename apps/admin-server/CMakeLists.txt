add_executable(charging-admin
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
)

target_link_libraries(charging-admin PRIVATE charging-core Qt6::Widgets Qt6::Charts Qt6::Network)
