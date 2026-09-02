set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_executable(charging-user
    src/main.cpp
    src/user_app_controller.cpp
    src/user_app_controller.h
)

qt_add_qml_module(charging-user
    URI ChargingUser
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/components/AppButton.qml
        qml/components/AppCard.qml
        qml/components/BottomNav.qml
        qml/components/StationCard.qml
        qml/components/StatusBadge.qml
        qml/pages/LoginPage.qml
        qml/pages/HomePage.qml
        qml/pages/StationDetailPage.qml
        qml/pages/ChargingPage.qml
        qml/pages/ProfilePage.qml
        qml/pages/MapPage.qml
)

target_link_libraries(charging-user PRIVATE
    charging-core
    Qt6::Core
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::WebEngineQuick
)
