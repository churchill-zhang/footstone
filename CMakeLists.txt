cmake_minimum_required(VERSION 3.4.1)

project(base)

execute_process(COMMAND dirname ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE ROOT_DIR)
string(REPLACE "\n" "" ROOT_DIR "${ROOT_DIR}") 
set(BASE_PATH ${ROOT_DIR}/base)
set(THIRD_PARTY ${ROOT_DIR}/third_party)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${THIRD_PARTY}/refl-cpp/include
)
set(TDF_BASE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(src)
#add_subdirectory(test)