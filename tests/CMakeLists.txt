# ============================================================
# 原有测试（已在仓库 main 分支中存在）
# ============================================================

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

# ============================================================
# 新增测试（本阶段补充）
# ============================================================

# 登录测试：非法手机号、首次注册、正常登录、冻结用户禁止登录、在线冻结后断开
add_executable(login-tests test_login.cpp)
target_link_libraries(login-tests PRIVATE charging-core Qt6::Test Qt6::Network Qt6::Sql)
add_test(NAME login-tests COMMAND login-tests)

# 订单扩展测试：重复预约、非空闲桩预约、越权订单、取消后重复取消、
# 预约超时、重复停止、余额不足、重复结算
add_executable(orders-extended-tests test_orders_extended.cpp)
target_link_libraries(orders-extended-tests PRIVATE charging-core Qt6::Test Qt6::Network Qt6::Sql)
add_test(NAME orders-extended-tests COMMAND orders-extended-tests)

# 管理端扩展测试：未登录访问 admin 接口、错误密码、首次改密、
# 非法站点坐标、重复电桩编号、充电中电桩重启失败
add_executable(admin-extended-tests test_admin_extended.cpp)
target_link_libraries(admin-extended-tests PRIVATE charging-core Qt6::Test Qt6::Network Qt6::Sql)
add_test(NAME admin-extended-tests COMMAND admin-extended-tests)

# 并发与错误测试：两个用户同时预约同一电桩、数据库写入失败、
# 服务端暂时不可用、连续发送同类型请求
add_executable(concurrency-errors-tests test_concurrency_errors.cpp)
target_link_libraries(concurrency-errors-tests PRIVATE charging-core Qt6::Test Qt6::Network Qt6::Sql)
add_test(NAME concurrency-errors-tests COMMAND concurrency-errors-tests)

# 数据库迁移回归测试：旧库（charging_stations 缺 status 列）必须能被
# initialize() 自动升级，防止 "no such column: status" 启动失败复发
add_executable(database-migration-tests test_database_migration.cpp)
target_link_libraries(database-migration-tests PRIVATE charging-core Qt6::Test Qt6::Sql)
add_test(NAME database-migration-tests COMMAND database-migration-tests)
