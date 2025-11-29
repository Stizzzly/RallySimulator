cmake_minimum_required(VERSION 3.15)
project(RallyGame CXX)

set(CMAKE_CXX_STANDARD 14)

# --- 1. УКАЗЫВАЕМ ПУТЬ К ТВОЕЙ ПАПКЕ SDL2 ---
# Важно: используй прямые слеши /
set(SDL2_ROOT "D:/Program Files/RallyGame/SDL2-2.32.8/i686-w64-mingw32")

# Проверка, чтобы не искать ошибку часами
if(NOT EXISTS "${SDL2_ROOT}/include/SDL2/SDL.h")
    message(FATAL_ERROR "Путь к SDL2 указан неверно! Проверь переменную SDL2_ROOT.")
endif()

# --- 2. ПОДКЛЮЧАЕМ ЗАГОЛОВКИ И ЛИБЫ ---
include_directories(${SDL2_ROOT}/include)
link_directories(${SDL2_ROOT}/lib)

# --- 3. НАСТРОЙКИ ДЛЯ XP ---
add_definitions(-D_WIN32_WINNT=0x0501 -DWINVER=0x0501)

add_executable(${PROJECT_NAME} main.cpp)

# --- 4. ФЛАГИ ДЛЯ MINGW (СТАТИКА) ---
target_link_options(${PROJECT_NAME} PRIVATE
        -static-libgcc
        -static-libstdc++
        -Wl,--subsystem,windows # Убирает консоль. Если нужны логи - закомментируй.
)

# --- 5. ЛИНКОВКА (ПОРЯДОК ВАЖЕН!) ---
target_link_libraries(${PROJECT_NAME}
        mingw32
        SDL2main
        SDL2
)

# --- 6. АВТО-КОПИРОВАНИЕ DLL ---
# В архиве для MinGW файл SDL2.dll лежит в папке 'bin', а не 'lib'
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_ROOT}/bin/SDL2.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying SDL2.dll from bin folder..."
)
