cmake_minimum_required(VERSION 3.10)

project(SilverFoxAntiVirus
    VERSION 1.0.0
    DESCRIPTION "银狐木马专杀工具"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

file(GLOB SOURCES src/*.cpp)

add_executable(SilverFoxAntiVirus WIN32 ${SOURCES})

target_link_libraries(SilverFoxAntiVirus 
    shell32.lib
    user32.lib
    advapi32.lib)

set_target_properties(SilverFoxAntiVirus PROPERTIES
    OUTPUT_NAME "银狐木马专杀工具"
    WIN32_EXECUTABLE TRUE)