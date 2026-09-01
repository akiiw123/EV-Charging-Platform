add_executable(core-tests test_message_protocol.cpp)
target_link_libraries(core-tests PRIVATE charging-core Qt6::Test)
add_test(NAME core-tests COMMAND core-tests)
