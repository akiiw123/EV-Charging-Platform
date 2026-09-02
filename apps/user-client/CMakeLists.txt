add_executable(charging-user
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
    src/views/login_view.cpp
    src/views/login_view.h
    src/views/home_view.cpp
    src/views/home_view.h
    src/views/station_detail_view.cpp
    src/views/station_detail_view.h
    src/views/charging_view.cpp
    src/views/charging_view.h
    src/views/profile_view.cpp
    src/views/profile_view.h
    src/views/map_navigation_view.cpp
    src/views/map_navigation_view.h
)

target_link_libraries(charging-user PRIVATE charging-core Qt6::Widgets Qt6::Network Qt6::WebEngineWidgets)
