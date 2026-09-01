add_library(charging-core STATIC
    src/database_manager.cpp
    src/message_protocol.cpp
)

target_include_directories(charging-core PUBLIC include)
target_link_libraries(charging-core PUBLIC Qt6::Core Qt6::Sql Qt6::Network)

target_compile_definitions(charging-core PRIVATE
    CHARGING_SCHEMA_PATH="${CMAKE_SOURCE_DIR}/database/schema.sql"
    CHARGING_SEED_PATH="${CMAKE_SOURCE_DIR}/database/seed.sql"
)
