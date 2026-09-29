#pragma once

#include <filesystem>

#include "app.h"

namespace state_serialization {

// Сохраняет состояние приложения (игровые сессии + игроки) в файл.
// Исключения пробрасываются наружу.
void SaveState(const app::Application& app, const std::filesystem::path& path);

// Загружает состояние из файла, полностью заменяя текущее состояние
// приложения. Если файл не существует или невалиден — состояние
// приложения не изменяется, выбрасывается исключение.
void LoadState(app::Application& app, const std::filesystem::path& path);

}  // namespace state_serialization