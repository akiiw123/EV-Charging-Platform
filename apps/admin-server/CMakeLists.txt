qt_add_executable(charging-admin
    src/main.cpp
    src/admin_app_controller.cpp
    src/admin_app_controller.h
    src/json_list_model.h
)

qt_add_qml_module(charging-admin
    URI ChargingAdmin
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/components/AppShell.qml
        qml/components/Sidebar.qml
        qml/components/TopBar.qml
        qml/components/PageHeader.qml
        qml/components/MetricCard.qml
        qml/components/DataTable.qml
        qml/components/SearchField.qml
        qml/components/FilterComboBox.qml
        qml/components/EmptyState.qml
        qml/components/ConfirmDialog.qml
        qml/components/DetailDrawer.qml
        qml/components/ThemePreviewCard.qml
        qml/pages/LoginPage.qml
        qml/pages/DashboardPage.qml
        qml/pages/StationsPage.qml
        qml/pages/PilesPage.qml
        qml/pages/OrdersPage.qml
        qml/pages/UsersPage.qml
        qml/pages/PredictionPage.qml
        qml/pages/SettingsPage.qml
)

target_link_libraries(charging-admin PRIVATE
    charging-core charging-ui charging-uiplugin
    Qt6::Core Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
