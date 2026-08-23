#pragma once

#include "../domain/RaceTypes.h"
#include <string>

namespace rally {

// Чистый разбор формата этапа key=value (stages/*.stage).
// Не обращается к файловой системе — файл читает адаптер, парсер тестируется сам.
class StageParser {
public:
    // Применяет одну пару ключ/значение к конфигу. Незнакомые ключи игнорирует.
    static void parseLine(const std::string& key, const std::string& value, StageConfig& stage);
    // Разбирает весь текст этапа; true, если получено непустое имя.
    static bool parseText(const std::string& text, StageConfig& stage);
};

} // namespace rally
