cmake_minimum_required(VERSION 3.15)
project(RallyGame CXX)

set(CMAKE_CXX_STANDARD 14)
set(SDL2_ROOT "D:/Program Files/RallyGame/SDL2-2.32.8/i686-w64-mingw32")

if(NOT EXISTS "${SDL2_ROOT}/include/SDL2/SDL.h")
    message(FATAL_ERROR "Path to SDL2 is invalid; set SDL2_ROOT.")
endif()

include_directories(${SDL2_ROOT}/include)
link_directories(${SDL2_ROOT}/lib)
add_definitions(-D_WIN32_WINNT=0x0501 -DWINVER=0x0501)
add_executable(${PROJECT_NAME} main.cpp)
target_link_options(${PROJECT_NAME} PRIVATE -static-libgcc -static-libstdc++ -Wl,--subsystem,windows)
target_link_libraries(${PROJECT_NAME} mingw32 SDL2main SDL2)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL2_ROOT}/bin/SDL2.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
