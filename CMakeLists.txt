cmake_minimum_required(VERSION 3.0.0)
project(piano VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(piano main.cpp)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules" ${CMAKE_MODULE_PATH})
#Find any version 2.X of SFML
#See the FindSFML.cmake file for additional details and instructions
find_package(SFML 2 REQUIRED graphics window system audio)

include_directories(
        ${SFML_INCLUDE_DIR}
)
target_link_libraries(
        piano
        ${SFML_LIBRARIES}
        ${SFML_DEPENDENCIES}
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
