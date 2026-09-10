set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_library(charging-ui STATIC)
qt_add_qml_module(charging-ui
    URI Charging.UI
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Charging/UI
    QML_FILES
        qml/Theme.qml
        qml/PanelCard.qml
        qml/AppButton.qml
        qml/AppTextField.qml
        qml/StatusBadge.qml
        qml/LineIcon.qml
)
target_link_libraries(charging-ui PRIVATE Qt6::Quick Qt6::QuickControls2)
