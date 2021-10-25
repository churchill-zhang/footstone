cmake_minimum_required(VERSION 2.8)
project(footstone)
set(CMAKE_CXX_STANDARD 17)
set(BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(THIRD_PARTY ${CMAKE_CURRENT_SOURCE_DIR}/third_party) 

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${THIRD_PARTY}/refl-cpp/include
    ${THIRD_PARTY}/json/include
)
set(FOOTSTONE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(src)
add_subdirectory(test)