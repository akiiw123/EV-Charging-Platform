add_executable(core-tests test_message_protocol.cpp)
target_link_libraries(core-tests PRIVATE charging-core Qt6::Test)
add_test(NAME core-tests COMMAND core-tests)

add_executable(repository-tests test_repositories.cpp)
target_link_libraries(repository-tests PRIVATE charging-core Qt6::Test Qt6::Sql)
add_test(NAME repository-tests COMMAND repository-tests)

add_executable(tcp-integration-tests test_tcp_integration.cpp)
target_link_libraries(tcp-integration-tests PRIVATE charging-core Qt6::Test Qt6::Network Qt6::Sql)
add_test(NAME tcp-integration-tests COMMAND tcp-integration-tests)

add_executable(pricing-tests test_pricing.cpp)
target_link_libraries(pricing-tests PRIVATE charging-core Qt6::Test Qt6::Sql)
add_test(NAME pricing-tests COMMAND pricing-tests)

add_executable(database-maintenance-tests test_database_maintenance.cpp)
target_link_libraries(database-maintenance-tests PRIVATE charging-core Qt6::Test Qt6::Sql)
add_test(NAME database-maintenance-tests COMMAND database-maintenance-tests)

add_executable(database-robustness-tests test_database_robustness.cpp)
target_link_libraries(database-robustness-tests PRIVATE charging-core Qt6::Test Qt6::Sql)
add_test(NAME database-robustness-tests COMMAND database-robustness-tests)
