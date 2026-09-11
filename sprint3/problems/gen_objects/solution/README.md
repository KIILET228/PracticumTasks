# Найденные и исправленные ошибки

CI падал последовательно на разных этапах — вот все ошибки по порядку и что
с ними сделано.

## 1. `apt-get update` падал: просроченный индекс bullseye-security

```
E: Release file for .../bullseye-security/InRelease is expired
```

Образ `gcc:11` собран на Debian bullseye; метаданные `bullseye-security` на
зеркале оказались просрочены (`Valid-Until` в прошлом), apt по умолчанию
отказывается обновлять индексы с таким файлом.

**Фикс:** флаг, отключающий проверку срока действия индекса:

```dockerfile
RUN apt-get update -o Acquire::Check-Valid-Until=false && ...
```

## 2. `apt-get install` падал: 404 на python3-pip / python3-pkg-resources

После фикса №1 `update` стал проходить, но `install` падал с `404 Not Found`
на конкретные файлы пакетов из `bullseye-security`. Это рассинхронизация
edge-кеша `deb.debian.org` (Fastly CDN) — Packages-индекс с одного узла CDN
ссылается на версии, файлы которых на этом же узле уже удалены (типично
сразу после публикации security-патча). Обычный `apt-get update` это не
лечит.

**Фикс:** не тянуть `python3-pip` через apt вообще, поставить `pip` напрямую
через `get-pip.py`, в обход нестабильного apt-пакета:

```dockerfile
RUN apt-get update -o Acquire::Check-Valid-Until=false && \
    apt-get install -y --no-install-recommends \
        ca-certificates curl python3 python3-distutils cmake make && \
    curl -sS https://bootstrap.pypa.io/pip/3.9/get-pip.py -o /tmp/get-pip.py && \
    python3 /tmp/get-pip.py "pip<24" && \
    rm -rf /var/lib/apt/lists/* /tmp/get-pip.py
```

## 3. `cmake ..` падал: `conanbuildinfo.cmake` не найден

```
CMake Error at CMakeLists.txt:11 (include): include could not find load file:
    /app/build/conanbuildinfo.cmake
CMake Error: Unknown CMake command "conan_basic_setup".
```

Дошли до этапа сборки самого сервера — и здесь настоящий баг в самом
проекте, а не в окружении. `conanfile.txt` использует генератор
`cmake_multi`:

```ini
[generators]
cmake_multi
```

Этот генератор создаёт файлы `conanbuildinfo_multi.cmake` +
`conanbuildinfo_<config>.cmake` (в логе: `conanbuildinfo_release.cmake`), а
**не** `conanbuildinfo.cmake`. `CMakeLists.txt` же был написан в расчёте на
одноконфигурационный генератор `cmake` и включал именно
`conanbuildinfo.cmake` — файла с таким именем просто не существовало.

**Фикс** (по документации Conan 1.x для `cmake_multi`):

```cmake
include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)
```

## 4. Скрытая ошибка в CMakeLists.txt, всплыла бы на тестовом таргете

```cmake
target_include_directories(game_server_tests PRIVATE src CONAN_PKG::boost)
```

`target_include_directories` принимает пути к каталогам, а не имена
target'ов. `CONAN_PKG::boost` — это alias-таргет для `target_link_libraries`
(он и так уже подключён строкой ниже), а не путь. Убрал лишний/неверный
аргумент:

```cmake
target_include_directories(game_server_tests PRIVATE src)
```

## 5. Ещё одна ошибка, которая проявилась бы уже после успешной сборки

```dockerfile
COPY --from=build /app/build/bin/game_server ./game_server
```

`CMakeLists.txt` нигде не задавал `CMAKE_RUNTIME_OUTPUT_DIRECTORY`, поэтому
по умолчанию CMake кладёт бинарник прямо в `/app/build/game_server`, без
подпапки `bin/`. `COPY` на run-стадии не нашёл бы файл.

**Фикс** — явно задать каталог вывода бинарников в `CMakeLists.txt`, чтобы
он совпадал с тем, что ожидает `Dockerfile`:

```cmake
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
```

## 6. Хрупкость в main.cpp (доп. проверка, не проявлялась в логах)

`main.cpp` использует `std::vector` и `std::max`, но не подключает
`<vector>`/`<algorithm>` напрямую — работало только за счёт того, что их
транзитивно тянут заголовки Boost.Asio. Добавил явные инклюды, чтобы не
зависеть от деталей реализации конкретной версии Boost:

```cpp
#include <algorithm>
#include <vector>
```

## Остальной код

Логику `LootGenerator::Generate`, `GameSession::MakeRandomLostObject`,
`GameSession::GenerateLoot`, сериализацию `lostObjects`/`lootTypes` в
`api_handler.cpp`/`extra_data.h`, а также остальные `.cpp`/`.h` файлы
(`request_handler`, `http_server`, `players`, `app`, `json_loader`,
`logger`) я вручную прошёл ещё раз построчно: сверил с каждым тест-кейсом
из `loot_generator_tests.cpp`/`model-tests.cpp` и с ТЗ — расхождений не
нашёл. `model.cpp`, `loot_generator.cpp`, `app.cpp`, `players.cpp` также
чисто проходят `g++ -std=c++20 -fsyntax-only`.

Полную сборку с реальным Boost/Conan/CMake здесь проверить не могу — в этой
среде нет сети и предустановленных Boost.Beast/Boost.Log/Catch2/CMake/Conan,
поэтому часть проверки (пункты 3–6) сделана по документации Conan/CMake и
построчным ручным разбором, а не прогоном компилятора.

## Чего не хватает в архиве

В присланных файлах не было каталога `static/` (фронтенд), который
`Dockerfile` копирует строкой `COPY static/ ./static/`. Без него
`docker build` дойдёт до этого шага и упадёт с "not found". Добавьте свою
папку `static/` с фронтендом рядом с `src/`, `data/`, `Dockerfile` перед
сборкой образа.

## Структура архива

```
.
├── CMakeLists.txt     (исправлен: п.3, п.4, п.5)
├── Dockerfile          (исправлен: п.1, п.2)
├── conanfile.txt
├── data/
│   └── config.json
├── src/
│   └── main.cpp        (исправлен: п.6; остальное — без изменений)
└── tests/              (без изменений)
```
