cmake_minimum_required(VERSION 3.15)
project(RallyGame CXX)

include(CTest)
find_package(Python3 COMPONENTS Interpreter QUIET)

set(CMAKE_CXX_STANDARD 14)

# --- 0. ТОЛЬКО CLANG ---
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Проект собирается только Clang'ом! Пример: cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ ..")
endif()

# --- 1. УКАЗЫВАЕМ ПУТЬ К ПАПКЕ SDL2 ---
# Можно переопределить без правки файла: cmake -DSDL2_ROOT="C:/путь/к/SDL2" ..
# Разрядность: -DRALLY_ARCH=32 (по умолчанию) или -DRALLY_ARCH=64
set(RALLY_ARCH "32" CACHE STRING "Разрядность: 32 или 64")
string(APPEND _arch_suffix "-w64-mingw32")
if(RALLY_ARCH STREQUAL "64")
    set(RALLY_TRIPLE x86_64${_arch_suffix})
    set(RALLY_BITS_FLAG -m64)
else()
    set(RALLY_TRIPLE i686${_arch_suffix})
    set(RALLY_BITS_FLAG -m32)
endif()

set(SDL2_ROOT "D:/Program Files/RallyGame/SDL2-2.32.8/${RALLY_TRIPLE}" CACHE PATH "Корень SDL2")

# Проверка, чтобы не искать ошибку часами
if(NOT EXISTS "${SDL2_ROOT}/include/SDL2/SDL.h")
    message(FATAL_ERROR "Путь к SDL2 указан неверно! Проверь переменную SDL2_ROOT.")
endif()

# --- 2. ПОДКЛЮЧАЕМ ЗАГОЛОВКИ И ЛИБЫ ---
include_directories(${SDL2_ROOT}/include)
link_directories(${SDL2_ROOT}/lib)

# --- 3. ЦЕЛЬ: 32- ИЛИ 64-БИТНЫЙ WINDOWS (${RALLY_TRIPLE}) ---
# Работает и с обычным clang (нужен sysroot), и с llvm-mingw.
add_compile_options("--target=${RALLY_TRIPLE}" ${RALLY_BITS_FLAG})
add_link_options("--target=${RALLY_TRIPLE}" ${RALLY_BITS_FLAG} -fuse-ld=lld)

# --- 4. НАСТРОЙКИ ДЛЯ XP ---
add_definitions(-D_WIN32_WINNT=0x0501 -DWINVER=0x0501)

# --- 4.5 ЖЁСТКАЯ ОПТИМИЗАЦИЯ ПОД ЦЕЛЕВОЙ CPU (AMD K8 / Turion 64) ---
# Включается только в отдельном каталоге сборки: -DRALLY_OPT_K8=ON.
# NDEBUG не выставляется специально: assert() в тестах должен работать.
# -fno-strict-aliasing обязателен: Q_rsqrt в Renderer.cpp делает type punning.
option(RALLY_OPT_K8 "Жёсткая оптимизация: -O3 -march=k8 -ffast-math" OFF)
if(RALLY_OPT_K8)
    add_compile_options(-O3 -march=k8 -mtune=k8 -ffast-math -fno-strict-aliasing)
endif()

add_executable(${PROJECT_NAME}
        main.cpp
        src/domain/RaceSession.cpp
        src/domain/TrackModel.cpp
        src/application/ProfileStore.cpp
        src/application/StageParser.cpp
        src/application/Progression.cpp
        src/application/Game.cpp
        src/platform/ObjModel.cpp
        src/platform/SdlInput.cpp
        src/platform/Renderer.cpp
        src/platform/VectorFont.cpp
        src/platform/StageFiles.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE src)

# Изменение внешней модели должно запускать POST_BUILD-копирование даже если
# исходный C++ код не менялся.
set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY
        LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/lancia_delta_volterra.obj"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/lancia_delta_volterra.bmp")

# --- 5. ФЛАГИ ЛИНКОВКИ (СТАТИКА) ---
target_link_options(${PROJECT_NAME} PRIVATE
        -static-libgcc
        -static-libstdc++
        -Wl,--subsystem,windows # Убирает консоль. Если нужны логи - закомментируй.
)

# --- 6. ЛИНКОВКА (ПОРЯДОК ВАЖЕН!) ---
target_link_libraries(${PROJECT_NAME}
        mingw32
        SDL2main
        SDL2
        opengl32
)

# --- 7. АВТО-КОПИРОВАНИЕ DLL ---
# В архиве для MinGW файл SDL2.dll лежит в папке 'bin', а не 'lib'
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_ROOT}/bin/SDL2.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/lancia_delta_volterra.obj"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/car.obj"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/lancia_delta_volterra.bmp"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/lancia_delta_volterra.bmp"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/stages"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/stages"
        COMMENT "Copying SDL2.dll from bin folder..."
)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME car_asset_integrity
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_car_assets.py)
endif()
if(BUILD_TESTING)
    add_executable(race_session_test tests/race_session_test.cpp src/domain/RaceSession.cpp)
    target_include_directories(race_session_test PRIVATE src)
    target_link_options(race_session_test PRIVATE -static-libgcc -static-libstdc++)
    add_test(NAME race_session_rules COMMAND race_session_test)

    # Чистые модули без SDL/OpenGL: domain и application.
    add_executable(track_model_test tests/track_model_test.cpp src/domain/TrackModel.cpp)
    target_include_directories(track_model_test PRIVATE src)
    target_link_options(track_model_test PRIVATE -static-libgcc -static-libstdc++)
    add_test(NAME track_model_rules COMMAND track_model_test)

    add_executable(stage_parser_test tests/stage_parser_test.cpp src/application/StageParser.cpp)
    target_include_directories(stage_parser_test PRIVATE src)
    target_link_options(stage_parser_test PRIVATE -static-libgcc -static-libstdc++)
    add_test(NAME stage_parser_rules COMMAND stage_parser_test)

    add_executable(progression_test tests/progression_test.cpp src/application/Progression.cpp)
    target_include_directories(progression_test PRIVATE src)
    target_link_options(progression_test PRIVATE -static-libgcc -static-libstdc++)
    add_test(NAME progression_rules COMMAND progression_test)

    add_executable(game_flow_test tests/game_flow_test.cpp
            src/domain/RaceSession.cpp
            src/domain/TrackModel.cpp
            src/application/Progression.cpp
            src/application/Game.cpp)
    target_include_directories(game_flow_test PRIVATE src)
    target_link_options(game_flow_test PRIVATE -static-libgcc -static-libstdc++)
    add_test(NAME game_flow_rules COMMAND game_flow_test)
endif()
