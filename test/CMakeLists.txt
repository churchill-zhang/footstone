

include_directories(${FOOTSTONE_SOURCE_DIR})

#link_libraries(${CMAKE_SOURCE_DIR}/lib/platform/win)

add_executable(test worker_pool_test.cc)
target_link_libraries(test footstone)

set_property(TARGET test PROPERTY CXX_STANDARD 17)