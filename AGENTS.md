# RallySimulator — инструкции для агентов

## Цель проекта

Ретро-аркадный ралли-тайм-аттак в духе Sega Rally. Целевое железо: AMD Turion 64, 1 ГБ ОЗУ, ATI Radeon Xpress 200M. Главный критерий — стабильные 60 FPS при OpenGL 1.4 без шейдеров.

Полный дизайн зафиксирован в [GDD.md](GDD.md). Не расширяй объём v1 за его границы без явного запроса пользователя.

## Сборка и проверка

Используется Clang из MSYS2 и Ninja. В Windows PowerShell из корня проекта:

```powershell
& 'C:\msys64\clang64\bin\cmake.exe' --build build64
& 'C:\msys64\clang64\bin\ctest.exe' --test-dir build64 --output-on-failure
```

64-битная игра: `build64\RallyGame.exe`.

Release-сборка с жёсткой оптимизацией под целевой CPU (K8/Turion 64) собирается
в отдельном каталоге (Ninja лежит в `C:\msys64\clang64\bin`, компилятор —
`C:\llvm-mingw\bin\x86_64-w64-mingw32-clang++.exe`, SDL2_ROOT как у build64):

```powershell
cmake -S . -B build64-release -G Ninja -DRALLY_ARCH=64 -DRALLY_OPT_K8=ON `
  -DCMAKE_CXX_COMPILER=C:/llvm-mingw/bin/x86_64-w64-mingw32-clang++.exe `
  -DSDL2_ROOT=C:/RallyDev/SDL2-2.32.8/x86_64-w64-mingw32
& 'C:\msys64\clang64\bin\cmake.exe' --build build64-release
```

Флаги: `-O3 -march=k8 -mtune=k8 -ffast-math -fno-strict-aliasing`.
`-fno-strict-aliasing` обязателен (type punning в `Q_rsqrt`), `NDEBUG` не
ставится — `assert()` в тестах должен оставаться рабочим.

32-битная XP-версия собирается так же, но с i686-тулчейном:

```powershell
cmake -S . -B build32-release -G Ninja -DRALLY_ARCH=32 -DRALLY_OPT_K8=ON `
  -DCMAKE_CXX_COMPILER=C:/llvm-mingw/bin/i686-w64-mingw32-clang++.exe `
  -DSDL2_ROOT=C:/RallyDev/SDL2-2.32.8/i686-w64-mingw32
& 'C:\msys64\clang64\bin\cmake.exe' --build build32-release
```

Не удаляй `build/`, `build64/`, `build32-release/` и `build64-release/` без явного разрешения. Пост-сборка сама копирует SDL2, этапы, OBJ и BMP рядом с exe.

## Архитектура

Не добавляй игровую логику в `main.cpp`. Он является временным legacy-адаптером SDL/OpenGL и должен постепенно уменьшаться.

- `src/domain/` — чистые правила и типы. Никаких SDL, OpenGL, файловой системы или Windows API.
- `src/application/` — сценарии приложения и интерфейсы хранилищ. Здесь допустимы интерфейсы, но не конкретные SDL/OpenGL-вызовы.
- Адаптеры платформы (SDL-ввод, OpenGL-рендер, файловые репозитории) должны зависеть от domain/application, а не наоборот.
- `stages/*.stage` — данные этапов. Не хардкодь параметры этапов в игровом цикле.
- `assets/` — модель и ливрея Volterra Racing. Не перезаписывай исходную текстуру нейросетевой правкой: UV-атлас должен сохранять пиксельную раскладку.

SOLID-правила:

1. Одна причина для изменения на класс/модуль.
2. Зависимости передавать через небольшие интерфейсы (`IProfileStore` — образец).
3. Domain-код должен тестироваться без окна и GPU.
4. Новое правило гонки требует unit-теста.
5. Не смешивать формат хранения, UI и правила медалей в одном классе.

## Графика и ассеты

- Только OpenGL 1.4 fixed pipeline; не добавлять GLSL или тяжёлые постэффекты.
- Держать BMP-текстуры в диапазоне 256–512 px, ограничивать частицы и дальность видимости fog'ом.
- OBJ машины экспортируется Blender Z-up; в рендере игра конвертирует оси в свою Y-up систему.
- Текущий загрузчик OBJ поддерживает `v`, `vt` и треугольные `f v/vt/vn`.
- Любое изменение OBJ/BMP запускает `car_asset_integrity`.

## Стиль работы

- Перед изменением проверяй `git status`; не затирай пользовательские изменения.
- Используй `apply_patch` для исходников и текстовых файлов.
- После изменения C++ выполняй сборку и `ctest`; в финальном сообщении указывай результаты.
- Если меняется управление, HUD, время этапов или сохранения — синхронизируй GDD.md.
