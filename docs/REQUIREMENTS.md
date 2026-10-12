# Luma — требования к разработке

## 1. Цель проекта

Разработать полноценное desktop-приложение класса Adobe Lightroom / Lightroom Classic для macOS, ориентированное на профессиональную работу с фотографиями:

- импорт и каталогизация фототеки;
- поддержка RAW/JPEG/HEIF/TIFF/DNG;
- профессиональная неразрушающая RAW-обработка;
- локальные корректировки и маски;
- пакетная обработка;
- пресеты;
- сравнение и отбор фотографий;
- экспорт;
- локальная работа без обязательной облачной инфраструктуры.

Рабочее название проекта: **Luma**.

Название, логотип, иконки, ресурсы и интерфейс Adobe Lightroom не копировать. Lightroom использовать только как функциональный ориентир.

---

## 2. Основные продуктовые принципы

### 2.1. Local-first

Программа должна полноценно работать без:

- регистрации;
- облачного аккаунта;
- подключения к интернету;
- загрузки пользовательских фотографий на сервер.

Облачные и AI-функции должны быть дополнительными модулями.

### 2.2. Non-destructive editing

Оригинальный файл никогда не изменяется в процессе обработки.

Все изменения хранятся отдельно в виде параметров обработки.

Должна быть возможность:

- мгновенно вернуть оригинал;
- отключить отдельную коррекцию;
- изменить старую коррекцию;
- создать несколько версий обработки одной фотографии.

### 2.3. Производительность

UI никогда не должен ожидать завершения:

- декодирования RAW;
- генерации preview;
- экспорта;
- AI-анализа;
- индексации;
- записи metadata.

Все тяжёлые операции выполняются асинхронно через job system.

### 2.4. Безопасность фотографий

При любых ошибках приложения запрещена потеря:

- оригиналов;
- каталога;
- пользовательских правок;
- presets;
- metadata.

---

## 3. Целевая платформа

На текущем этапе приложение разрабатывается **только для macOS**.

Поддерживаемая архитектура:

- macOS;
- Apple Silicon;
- arm64.

Основная целевая конфигурация:

- Mac с Apple Silicon;
- 16 GB RAM и более;
- SSD;
- GPU Apple через Metal.

Минимально допустимая конфигурация:

- Apple Silicon;
- 8 GB unified memory.

В текущий scope не входят:

- Windows;
- Linux;
- Intel Mac.

При этом core-архитектура по возможности не должна искусственно блокировать дальнейшее портирование.

Не требуется:

- DirectX;
- Windows API;
- Win32-specific code;
- Windows installer;
- Windows CI;
- Windows packaging.

Все platform-specific реализации размещать отдельно:

```text
/platform/macos
```

Не добавлять пустые Windows abstraction layers ради гипотетической будущей поддержки.

---

## 4. Технологический стек

Основной стек:

- C++20/23;
- Qt 6;
- QML;
- CMake;
- SQLite;
- LibRaw;
- LittleCMS 2 и/или OpenColorIO;
- Lensfun либо собственный слой профилей объективов;
- OpenCV там, где это действительно необходимо;
- ONNX Runtime и/или Core ML для локальных ML-моделей;
- Metal для GPU-ускорения.

Предпочтительный GPU backend:

```text
Metal
```

Если Qt RHI обеспечивает необходимую производительность и контроль над pipeline, допускается Qt RHI с Metal backend.

Не требуется на текущем этапе реализовывать abstraction поверх:

- Vulkan;
- Direct3D;
- CUDA.

Image-processing core должен иметь CPU reference implementation.

---

## 5. Архитектура проекта

Разделить приложение минимум на следующие модули:

```text
/apps/desktop
/apps/cli

/core/catalog
/core/metadata
/core/raw
/core/image
/core/render
/core/color
/core/edits
/core/masks
/core/cache
/core/export
/core/jobs
/core/presets
/core/search
/core/ai

/platform/macos

/tests/unit
/tests/integration
/tests/render
/tests/performance
```

Архитектурная зависимость:

```text
UI
↓
Application Services
↓
Photo Engine / Catalog / Metadata / Export / AI
```

Не создавать монолитный MainWindow или аналогичный класс с основной бизнес-логикой.

Image engine должен иметь возможность использоваться:

- из GUI;
- из CLI;
- из unit/integration tests.

---

## 6. Публичный GitHub-репозиторий

Проект должен разрабатываться исключительно в **публичном GitHub-репозитории**.

Репозиторий является основным source of truth для:

- исходного кода;
- документации;
- issue tracking;
- архитектурных решений;
- roadmap;
- releases;
- changelog.

Рекомендуемая структура корня:

```text
.github/
    workflows/
    ISSUE_TEMPLATE/
    PULL_REQUEST_TEMPLATE.md

apps/
core/
platform/
tests/
docs/

.gitignore
.editorconfig
.clang-format
.clang-tidy

CMakeLists.txt

README.md
ARCHITECTURE.md
BUILDING.md
CONTRIBUTING.md
ROADMAP.md
CHANGELOG.md
SECURITY.md
CODE_OF_CONDUCT.md
LICENSE
THIRD_PARTY_LICENSES.md
```

---

## 7. Open Source и лицензирование

Проект с первого commit должен считаться open-source.

Предпочтительная лицензия:

```text
Apache License 2.0
```

Допустимая альтернатива:

```text
MIT License
```

Все внешние зависимости должны быть совместимы с лицензией проекта.

Перед подключением каждой зависимости проверить:

- название;
- версию;
- URL проекта;
- лицензию;
- назначение.

Поддерживать файл:

```text
THIRD_PARTY_LICENSES.md
```

Не добавлять GPL-компоненты в core без отдельного решения по лицензированию всего проекта.

---

## 8. Запрет на публикацию закрытых материалов

Так как repository публичный, запрещено коммитить:

- личные фотографии;
- реальные пользовательские RAW без разрешения на публикацию;
- Apple Developer certificates;
- signing keys;
- provisioning profiles;
- API keys;
- tokens;
- passwords;
- SSH keys;
- `.env` с credentials;
- private endpoints;
- закрытые SDK;
- copyrighted camera/lens profiles без права распространения.

Secrets хранить только через GitHub Secrets или локальное окружение.

---

## 9. Тестовые изображения

Для автоматических тестов использовать только изображения, которые:

- имеют разрешение на публичное распространение;
- созданы специально для проекта;
- находятся в public domain;
- имеют совместимую open-source лицензию.

Каталог:

```text
tests/assets/
```

Для приватных тестовых RAW поддержать внешний каталог:

```text
LUMA_PRIVATE_TEST_ASSETS
```

Пример:

```bash
export LUMA_PRIVATE_TEST_ASSETS="$HOME/LumaTestAssets"
```

Если private assets отсутствуют, соответствующие integration tests должны корректно пропускаться.

Отдельный обязательный acceptance-test:

**Nikon D7500 NEF.**

---

## 10. Каталог фотографий

Использовать SQLite.

Обязательно включить:

- WAL;
- migrations;
- foreign keys;
- indexes;
- transactions.

### Asset

Хранить минимум:

- internal UUID;
- original path;
- filename;
- extension;
- filesize;
- checksum;
- capture date;
- import date;
- width;
- height;
- orientation;
- camera;
- lens;
- focal length;
- aperture;
- shutter speed;
- ISO;
- GPS;
- rating;
- flag;
- color label.

### EditDocument

Хранит набор параметров обработки фотографии.

### Version

Позволяет одной фотографии иметь несколько вариантов обработки:

- Original;
- Color;
- B&W;
- Print;
- Social.

### Collection

Поддержать:

- обычные коллекции;
- smart collections;
- nested folders.

Также хранить:

- Keyword;
- Preset;
- ImportSession;
- ExportPreset;
- RenderCacheEntry.

---

## 11. Импорт

Реализовать окно Import.

Источники:

- директория;
- несколько директорий;
- SD-карта;
- камера как файловое устройство.

Режимы:

### Add

Оригиналы остаются на существующем месте.

### Copy

Оригиналы копируются в библиотеку.

### Move

Файлы переносятся.

Параметры:

- destination directory;
- создание подпапок;
- структура по дате;
- rename template;
- metadata preset;
- keywords;
- develop preset;
- second backup copy.

Выявлять дубликаты по сочетанию:

- checksum;
- filesize;
- EXIF capture date;
- filename.

Импорт нескольких тысяч файлов не должен блокировать UI.

---

## 12. Поддерживаемые изображения

Обязательно:

- Nikon NEF;
- Canon CR2/CR3;
- Sony ARW;
- Fujifilm RAF;
- Panasonic RW2;
- Olympus ORF;
- DNG;
- Apple ProRAW;
- JPEG;
- PNG;
- TIFF;
- HEIF/HEIC.

Архитектура должна позволять добавлять новые RAW-форматы без изменения UI.

---

## 13. RAW pipeline

Реализовать настоящий RAW processing pipeline.

Не использовать embedded JPEG как изображение для Develop.

Примерный pipeline:

```text
RAW file
↓
RAW decode
↓
sensor data normalization
↓
black / white level
↓
dead / hot pixel correction
↓
demosaicing
↓
white balance
↓
camera color transform
↓
working color space
↓
optical corrections
↓
global adjustments
↓
local adjustments
↓
crop / geometry
↓
output transform
↓
display profile
```

Внутренний pipeline должен использовать floating point.

Предпочтительно:

- минимум 16-bit float;
- 32-bit float для части вычислений.

Избегать промежуточного преобразования в 8-bit RGB.

LibRaw использовать прежде всего как RAW decoder, а не как готовый production rendering engine.

---

## 14. Color management

Color management является обязательной частью ядра.

Поддержать:

- embedded ICC profile;
- monitor ICC profile;
- sRGB;
- Display P3;
- Adobe RGB;
- ProPhoto RGB;
- HDR-compatible working pipeline в архитектуре.

На macOS использовать ColorSync там, где это оправдано.

При переносе окна между мониторами изображение должно корректно пересчитываться под ICC-профиль нового дисплея.

---

## 15. Главное представление Develop

Основной layout:

```text
┌────────────────────────────────────────────┐
│ toolbar                                    │
├───────────┬────────────────────┬───────────┤
│ library   │                    │ histogram │
│ presets   │       image        │ develop   │
│ history   │       viewer       │ controls  │
│           │                    │           │
├───────────┴────────────────────┴───────────┤
│               filmstrip                    │
└────────────────────────────────────────────┘
```

Панели должны:

- скрываться;
- изменять ширину;
- запоминать состояние.

Поддержать fullscreen editing.

---

## 16. Viewer

Поддержать:

- Fit;
- Fill;
- 1:1;
- 2:1;
- произвольный zoom;
- mouse-wheel zoom;
- pinch zoom;
- pan;
- double click 1:1/Fit;
- before/after;
- split before/after;
- clipping indicators.

При 1:1 использовать полноценный render, а не растянутый preview.

---

## 17. Histogram

Отображать:

- RGB histogram;
- luminance histogram;
- clipping.

Histogram должен обновляться практически realtime.

Дополнительно разрешить менять через histogram:

- Exposure;
- Highlights;
- Shadows;
- Whites;
- Blacks.

---

## 18. Basic / Light

### White Balance

- As Shot;
- Auto;
- Daylight;
- Cloudy;
- Shade;
- Tungsten;
- Fluorescent;
- Custom.

Параметры:

- Temperature;
- Tint.

Eyedropper WB.

### Tone

- Exposure;
- Contrast;
- Highlights;
- Shadows;
- Whites;
- Blacks.

### Presence

- Texture;
- Clarity;
- Dehaze.

### Color

- Vibrance;
- Saturation.

Double click на slider должен сбрасывать его.

Alt/Option при Whites/Blacks должен показывать clipping preview.

---

## 19. Tone Curve

Два режима.

### Parametric curve

- Highlights;
- Lights;
- Darks;
- Shadows.

### Point curve

Поддержать:

- RGB;
- Red;
- Green;
- Blue.

Добавление/удаление control points.

Targeted adjustment tool обязателен.

---

## 20. HSL / Color Mixer

Для диапазонов:

- Red;
- Orange;
- Yellow;
- Green;
- Aqua;
- Blue;
- Purple;
- Magenta.

Редактировать:

- Hue;
- Saturation;
- Luminance.

Добавить targeted adjustment tool.

---

## 21. Point Color

Eyedropper по изображению.

Для выбранного диапазона:

- Hue shift;
- Saturation;
- Luminance;
- Hue range;
- Saturation range;
- Luminance range.

Добавить `Visualize range`.

---

## 22. Color Grading

Color wheels:

- Shadows;
- Midtones;
- Highlights;
- Global.

Для каждого:

- Hue;
- Saturation;
- Luminance.

Дополнительно:

- Blending;
- Balance.

---

## 23. Detail

### Sharpening

- Amount;
- Radius;
- Detail;
- Masking.

### Noise Reduction

Luminance:

- Amount;
- Detail;
- Contrast.

Color:

- Amount;
- Detail;
- Smoothness.

Diagnostic views через Alt/Option.

---

## 24. AI Denoise

Создать интерфейс:

```cpp
class IDenoiseProvider;
```

AI Denoise:

- работает с RAW;
- не разрушает исходник;
- cancellable;
- показывает progress;
- использует GPU/ANE при наличии;
- имеет CPU fallback.

На macOS предпочтительно использовать Core ML там, где это даёт преимущество.

---

## 25. Lens corrections

Автоматически определить:

- camera;
- lens;
- focal length;
- aperture.

Коррекции:

- distortion;
- vignetting;
- chromatic aberration.

Позволить:

- enable/disable profile;
- выбрать lens profile вручную;
- изменить distortion amount;
- изменить vignetting amount.

---

## 26. Transform / Geometry

Реализовать:

- Rotate;
- Horizontal;
- Vertical;
- Aspect;
- Scale;
- X Offset;
- Y Offset.

Auto perspective correction.

Guided correction минимум по:

- двум вертикальным линиям;
- двум горизонтальным линиям.

---

## 27. Crop

Поддержать:

- свободный crop;
- locked aspect;
- original aspect;
- 1:1;
- 4:3;
- 3:2;
- 16:9;
- custom ratio.

Функции:

- rotate;
- straighten;
- flip horizontal;
- flip vertical.

Overlays:

- thirds;
- grid;
- diagonal;
- golden ratio.

---

## 28. Effects

### Post Crop Vignette

- Amount;
- Midpoint;
- Roundness;
- Feather;
- Highlights.

### Grain

- Amount;
- Size;
- Roughness.

---

## 29. Маски

Маски обязательны для первой стабильной версии.

Типы:

- Brush;
- Linear Gradient;
- Radial Gradient;
- Luminance Range;
- Color Range;
- Depth Range при наличии depth map.

Поддержать:

- Add;
- Subtract;
- Intersect;
- Invert;
- Feather;
- opacity overlay;
- mask color overlay.

Одна фотография может иметь произвольное количество масок.

---

## 30. AI masking

Создать abstraction:

```cpp
class ISegmentationProvider;
```

Поддержать:

- Select Subject;
- Select Sky;
- Select Background;
- Select Object.

Позднее:

- People;
- face skin;
- body skin;
- hair;
- eyes;
- lips;
- clothes.

После генерации AI-маска должна становиться обычной bitmap/vector mask.

---

## 31. Healing / Remove

Три режима:

### Clone

Копирование пикселей из source area.

### Heal

Сохранение текстуры с адаптацией цвета/яркости.

### Content-aware remove

Автоматический поиск replacement area.

Поддержать:

- brush size;
- feather;
- opacity;
- source repositioning.

Хранить операции параметрически.

---

## 32. Generative Remove

Предусмотреть интерфейс:

```cpp
class IGenerativeEditProvider;
```

Cloud API не должно быть обязательным для работы программы.

Generative remove реализовывать после базового content-aware remove.

---

## 33. History

Каждое изменение должно появляться в History.

Пример:

```text
Import
Exposure +0.40
Highlights -37
Temperature 5350 K
Crop
Mask 1 — Exposure +0.30
```

Поддержать:

- undo;
- redo;
- переход к произвольному state;
- clear history;
- snapshot/version from history state.

Cmd+Z должен работать последовательно во всём Develop.

---

## 34. Versions / Snapshots

Пользователь может сохранить текущую обработку как Named Version.

Переключение версий не должно дублировать оригинальный RAW.

Version хранит только параметры обработки.

---

## 35. Presets

Preset может содержать выбранный subset параметров.

Поддержать:

- create;
- edit;
- rename;
- delete;
- folders;
- import;
- export.

Preview пресета отображать при наведении курсора без применения.

Применение preset — одна операция Undo.

---

## 36. Copy/Paste settings

Выбор параметров:

- Light;
- Color;
- Curve;
- HSL;
- Detail;
- Optics;
- Geometry;
- Crop;
- Masks;
- Effects.

Поддержать применение к одной или нескольким фотографиям.

---

## 37. Batch editing

Выбрать N фотографий и:

- применить preset;
- paste settings;
- изменить metadata;
- rating;
- flag;
- keywords;
- rotate;
- export.

Реализовать `Sync Settings`.

---

## 38. Library grid

Grid должен выдерживать минимум:

**100 000 фотографий.**

Использовать:

- virtualized grid;
- thumbnail cache;
- lazy loading.

Режимы:

- Grid;
- Loupe;
- Compare;
- Survey.

---

## 39. Filmstrip

Поддержать:

- thumbnails;
- selection;
- multi-selection;
- ratings;
- flags;
- color labels;
- active image.

---

## 40. Отбор фотографий

Flags:

- Pick;
- Unflagged;
- Reject.

Rating:

- 0–5 stars.

Color labels:

- минимум пять.

Горячие клавиши:

- P = Pick;
- X = Reject;
- U = Unflag;
- 0–5 = rating.

---

## 41. Metadata

Читать:

- EXIF;
- IPTC;
- XMP.

Редактируемые поля:

- Title;
- Caption;
- Copyright;
- Creator;
- Location;
- Keywords;
- Rating;
- Label.

Настройка:

`Automatically write changes to XMP sidecar`.

Для RAW:

```text
DSC_0001.NEF
DSC_0001.xmp
```

---

## 42. Search

Обычный поиск:

- filename;
- folder;
- keyword;
- camera;
- lens;
- ISO;
- focal length;
- aperture;
- shutter;
- rating;
- flag;
- date.

Поддержать запросы:

```text
camera:"Nikon D7500"
rating:>=4
iso:>=1600
lens:"35mm"
date:2026-10
```

---

## 43. Smart Collections

Условия:

```text
Rating >= 4
AND
Camera = Nikon D7500
AND
ISO >= 1600
```

Поддержать:

- AND;
- OR;
- NOT;
- nested groups.

---

## 44. Semantic search

Опциональный модуль.

Локально вычислять image embeddings.

Поддержать запросы:

```text
ночное небо
ребёнок на велосипеде
машина зимой
закат над водой
```

Все embeddings хранить локально.

Функция должна полностью отключаться.

---

## 45. Face recognition

Опциональная функция.

Pipeline:

```text
face detection
↓
face embedding
↓
clustering
↓
user assigns person name
```

Обработка локальная.

На macOS допускается использование Vision/Core ML.

---

## 46. Compare

Выбрать две фотографии.

Отобразить:

- Candidate;
- Select.

Поддержать linked zoom/pan.

---

## 47. Focus inspection

При zoom 1:1 обеспечить быстрый просмотр следующего RAW.

Желательно заранее декодировать:

```text
current
previous
next
```

---

## 48. HDR Merge

Выделить несколько bracketed RAW.

Команда:

`Photo -> Merge -> HDR`

Функции:

- automatic alignment;
- ghost removal;
- exposure merge;
- floating point output.

Результат должен оставаться пригодным для Develop.

---

## 49. Panorama

Поддержать:

- Perspective;
- Cylindrical;
- Spherical.

Pipeline:

- feature detection;
- image matching;
- alignment;
- projection;
- blending;
- seam correction.

Дополнительно:

- auto crop;
- boundary warp/fill.

---

## 50. HDR Panorama

Архитектура должна позволять объединять exposure brackets и panorama stitching в один pipeline.

Реализовать после отдельно работающих HDR и Panorama.

---

## 51. Auto Adjust

Создать:

```cpp
class IAutoAdjustProvider;
```

Auto может корректировать:

- Exposure;
- Contrast;
- Highlights;
- Shadows;
- Whites;
- Blacks;
- Vibrance.

Результат должен превращаться в обычные параметры Develop.

---

## 52. Export

Форматы:

- JPEG;
- PNG;
- TIFF;
- DNG.

Желательно:

- WebP;
- AVIF.

Настройки:

### File

- destination;
- rename;
- overwrite policy.

### JPEG

- quality;
- chroma settings при необходимости.

### TIFF

- 8/16 bit;
- compression.

### Color Space

- sRGB;
- Display P3;
- Adobe RGB;
- ProPhoto RGB.

### Resize

- original;
- width;
- height;
- long edge;
- short edge;
- megapixels;
- percentage.

### Output sharpening

- Screen;
- Matte;
- Glossy.

Amount:

- Low;
- Standard;
- High.

### Metadata

- all;
- copyright only;
- remove location;
- none.

### Watermark

- text;
- image.

---

## 53. Export Queue

Показывать:

- progress;
- current filename;
- completed/total;
- errors.

Поддержать:

- pause;
- resume;
- cancel.

---

## 54. Background jobs

Создать общий JobManager.

Типы jobs:

- Import;
- PreviewGeneration;
- RAWDecode;
- Export;
- AI;
- Indexing;
- FaceDetection;
- SemanticIndex;
- HDR;
- Panorama.

Jobs должны иметь:

- priority;
- progress;
- cancellation token;
- state;
- error.

Interactive rendering всегда имеет приоритет над background indexing.

---

## 55. Preview/cache architecture

Уровни preview:

- thumbnail: 256 px;
- grid preview: 1024 px;
- standard preview: 2560 px;
- full-resolution cache.

Cache key должен зависеть минимум от:

```text
asset
edit version
render pipeline version
requested resolution
color profile
```

Добавить:

- automatic cache eviction;
- настройку максимального размера cache.

---

## 56. GPU rendering

Приоритетные GPU-операции:

- Exposure;
- Tone;
- Curves;
- HSL;
- Color Grading;
- masks;
- gradients;
- sharpening;
- noise reduction;
- lens correction;
- geometry.

На macOS использовать Metal.

CPU implementation остаётся reference implementation.

GPU и CPU должны давать визуально эквивалентный результат в пределах заданной tolerance.

---

## 57. Реактивный preview

Во время движения slider использовать:

```text
interactive preview quality
```

После отпускания:

```text
full preview quality
```

Цель:

**визуальный feedback < 50 ms** на стандартном preview.

---

## 58. Autosave

Любое изменение Develop автоматически сохраняется.

Не делать кнопку Save.

Запись:

- debounce;
- transaction;
- crash-safe.

После аварийного завершения приложение должно восстановить последние подтверждённые изменения.

---

## 59. Catalog backup

Автоматически создавать backup SQLite.

Настройки:

- каждый запуск;
- раз в день;
- раз в неделю;
- вручную.

Хранить последние N backups.

Перед удалением старых backup проверять SQLite integrity.

---

## 60. Missing files

Если файл перемещён вне приложения:

показывать состояние `Missing`.

Поддержать:

- Locate;
- Locate Folder;
- Find Nearby Missing Files.

При изменении корневой директории не терять edits.

---

## 61. External file changes

Использовать filesystem watcher.

Отслеживать:

- rename;
- move;
- delete;
- modification;
- new XMP.

Не выполнять опасные автоматические действия без подтверждения.

---

## 62. Keyboard shortcuts

Все основные операции должны иметь shortcuts.

Создать Preferences -> Shortcuts.

Shortcuts должны быть переназначаемыми.

На текущем этапе ориентироваться на macOS:

- Cmd;
- Option;
- Shift;
- Control.

---

## 63. Preferences

Разделы:

- General;
- Performance;
- Import;
- Export;
- Metadata;
- External Editors;
- AI;
- Privacy.

---

## 64. Themes

Минимум:

- Dark;
- System.

Редактор должен использовать нейтральную тёмную тему по умолчанию.

---

## 65. Multimonitor

Поддержать несколько мониторов.

Позднее реализовать Secondary Display:

- Loupe;
- Grid;
- Compare.

Color management должен выполняться отдельно для каждого монитора.

---

## 66. External editor

Команда:

`Edit In...`

Форматы:

- TIFF;
- PSD-compatible TIFF;
- JPEG.

После сохранения внешним редактором импортировать файл обратно как связанный asset.

---

## 67. Plugin architecture

Предусмотреть plugin API минимум для:

- AI providers;
- exporters;
- metadata processors;
- external integrations.

Core не должен напрямую зависеть от конкретного AI SaaS.

---

## 68. CLI

Создать CLI.

Примеры:

```bash
photon import ~/Pictures/2026
```

```bash
photon render DSC_0123.NEF --preset portrait
```

```bash
photon export album "Vacation" --preset web
```

```bash
photon catalog verify
```

CLI должен использовать то же ядро, что и desktop app.

---

## 69. Diagnostics

Preferences -> Diagnostics:

Показывать:

- application version;
- catalog version;
- GPU;
- GPU backend;
- CPU;
- RAM;
- cache usage;
- current catalog;
- image engine version.

Кнопка:

`Export Diagnostic Report`.

Не включать фотографии пользователя в отчёт.

---

## 70. Logging

Structured logging.

Levels:

- trace;
- debug;
- info;
- warning;
- error.

Логи должны ротироваться.

В production не логировать:

- изображения;
- секреты;
- credentials.

---

## 71. Crash handling

При аварии:

- сохранить crash dump;
- сохранить stack trace, где возможно;
- сохранить последние log lines;
- не повредить catalog.

После следующего запуска предложить:

`Recover previous session`.

---

## 72. Performance requirements

На современной Apple Silicon машине:

### Startup

Warm start:

< 2 seconds.

Cold:

< 5 seconds.

### Grid

Плавный scroll большого каталога с целевой частотой 60 FPS.

### Develop

Изменение обычного slider:

feedback желательно < 50 ms.

### Cached photo switching

< 200 ms.

### RAW initial preview

Желательно:

< 1.5 s для RAW около 20–30 MP.

---

## 73. Memory management

Использовать:

- LRU;
- bounded caches;
- memory pressure handling;
- tile rendering при необходимости.

Программа не должна декодировать десятки full-resolution RAW одновременно.

При 8 GB unified memory приложение должно оставаться стабильным.

---

## 74. Large catalog requirements

Создать synthetic test catalogs:

- 10k;
- 100k;
- 500k assets.

Проверить:

- startup;
- search;
- sorting;
- filtering;
- scrolling;
- smart collections.

Не допускать O(N) full catalog scan для стандартных фильтров.

---

## 75. Image quality testing

Создать golden master tests.

Набор RAW:

- Nikon;
- Canon;
- Sony;
- Fujifilm;
- DNG.

Проверять:

- WB;
- demosaic;
- color;
- exposure;
- curves;
- lens corrections;
- highlights;
- shadows.

Pixel-perfect совпадение CPU/GPU необязательно, но отличие должно быть в установленной tolerance.

---

## 76. Metadata tests

Обязательно протестировать:

- Unicode filenames;
- Cyrillic;
- spaces;
- very long paths;
- malformed metadata;
- broken EXIF;
- rotated JPEG;
- missing EXIF;
- duplicate filenames.

---

## 77. Data integrity tests

Сценарии:

### App kill during import

После перезапуска каталог консистентен.

### App kill during Develop save

Последняя подтверждённая версия правок восстанавливается.

### App kill during export

Оригинал не повреждён.

### Disk full

Корректная ошибка.

### Original removed

Catalog сохраняет edits.

---

## 78. UI testing

Автоматизированно проверять:

- import;
- selection;
- rating;
- develop;
- crop;
- masks;
- undo;
- export.

---

## 79. Никаких destructive операций по умолчанию

Удаление из каталога и удаление с диска — разные команды:

```text
Remove from Catalog
```

и

```text
Delete from Disk
```

При Delete from Disk всегда показывать explicit confirmation.

---

## 80. AI privacy

Все AI-функции обозначать как:

### Local

Изображение остаётся на устройстве.

### Cloud

Изображение или его часть отправляется внешнему сервису.

Перед первым использованием cloud-AI пользователь должен дать explicit consent.

---

## 81. Telemetry

По умолчанию необязательную telemetry не включать.

Если telemetry появится:

- opt-in;
- прозрачный список данных;
- возможность полностью отключить.

---

## 82. Apple-specific integration

Использовать native macOS возможности там, где они дают преимущество:

- Metal;
- ColorSync;
- Core ML;
- Vision;
- ImageIO;
- Core Graphics;
- Accelerate/vImage;
- Grand Central Dispatch.

Platform-specific реализации должны оставаться за интерфейсами core.

Пример:

```text
core:
IColorManagementService

macOS:
ColorSyncColorManagementService
```

---

## 83. Apple Neural Engine

Для AI-функций по возможности использовать Core ML с ускорением через:

- GPU;
- Apple Neural Engine.

Сохранить abstraction:

```cpp
class ISegmentationProvider;
class IDenoiseProvider;
class IEmbeddingProvider;
```

Core не должен зависеть непосредственно от Core ML.

---

## 84. Packaging

Основной артефакт:

```text
Luma.app
```

Позднее:

```text
Luma.dmg
```

Code signing не должен быть обязательным для debug build.

До появления официальной подписи разработчика должна существовать возможность локально собрать и запустить приложение.

---

## 85. Git workflow

Основная ветка:

```text
main
```

`main` всегда должна:

- собираться;
- проходить тесты;
- оставаться пригодной для запуска.

Формат commit messages:

```text
feat: add RAW import pipeline
fix: preserve edit state after restart
perf: reduce preview render latency
refactor: split catalog and metadata services
test: add Nikon NEF integration test
docs: document render pipeline
```

Не делать giant commits с десятками несвязанных изменений.

---

## 86. Pull Requests

Для значительных изменений использовать отдельную branch.

PR должен содержать:

```text
What
Why
Implementation
Testing
Known limitations
```

Большие функции разбивать на несколько PR.

---

## 87. GitHub Issues

Любая крупная функция должна иметь Issue.

Labels:

```text
feature
bug
performance
rendering
raw
catalog
metadata
ui
gpu
ai
testing
documentation
architecture
good first issue
```

Roadmap должен быть связан с Issues.

---

## 88. GitHub Actions

Минимальный pipeline:

```text
checkout
↓
install dependencies
↓
configure CMake
↓
build
↓
unit tests
↓
integration tests
↓
clang-format check
↓
clang-tidy
```

CI запускается:

- на Pull Request;
- при push в `main`.

Пока нужен только macOS build.

---

## 89. GitHub Releases

Использовать Semantic Versioning:

```text
0.1.0
0.2.0
...
1.0.0
```

Каждый release должен иметь:

- changelog;
- macOS build;
- checksum;
- known issues.

---

## 90. Сборка macOS

Стандартный сценарий:

```bash
git clone <repository>
cd <repository>

cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Все дополнительные шаги документировать в:

```text
BUILDING.md
```

---

## 91. Dependency management

Все зависимости должны быть воспроизводимыми.

Допустимо использовать:

- CMake FetchContent;
- CPM.cmake;
- Conan;
- vcpkg.

Выбрать один основной подход.

Homebrew допускается для developer/toolchain dependencies, но runtime приложения не должен зависеть от случайно установленных Homebrew-библиотек.

---

## 92. Reproducible builds

Все версии dependencies должны быть зафиксированы.

Не использовать:

```text
latest
master
main
HEAD
```

как production dependency references.

Использовать:

- release version;
- Git tag;
- точный commit hash.

---

## 93. Public development documentation

README.md должен содержать:

```text
Luma
Open-source non-destructive RAW photo editor for macOS
```

Также:

- описание;
- screenshot;
- текущее состояние;
- поддерживаемые RAW;
- build instructions;
- roadmap;
- license.

Не утверждать, что функция существует, если она ещё не реализована.

Статусы:

```text
✅ Implemented
🚧 In progress
🗓 Planned
```

---

## 94. ARCHITECTURE.md

Обязательно описать:

- module boundaries;
- application layers;
- dependency direction;
- threading;
- image pipeline;
- render pipeline;
- catalog;
- cache;
- job system;
- Metal backend;
- AI providers.

Для схем использовать Mermaid либо другой GitHub-friendly текстовый формат.

---

## 95. Architecture Decision Records

Создать:

```text
docs/adr/
```

Примеры:

```text
0001-use-qt.md
0002-use-sqlite.md
0003-use-libraw.md
0004-metal-render-backend.md
0005-catalog-edit-model.md
```

ADR должен содержать:

```text
Status
Context
Decision
Consequences
```

---

## 96. Roadmap

### 0.1 — Foundation

- repository;
- build;
- Qt application shell;
- SQLite;
- LibRaw;
- NEF opening;
- basic metadata.

### 0.2 — Develop Core

- Exposure;
- WB;
- Tone;
- basic color;
- persistence.

### 0.3 — Library

- import;
- thumbnails;
- grid;
- ratings;
- flags.

### 0.4 — Professional Develop

- curves;
- HSL;
- detail;
- crop;
- optics;
- presets.

### 0.5 — Local Adjustments

- masks;
- brush;
- gradients;
- range masks.

### 0.6 — Performance

- Metal;
- cache;
- progressive rendering.

### 0.7 — Advanced

- AI Denoise;
- AI masks;
- remove;
- semantic search.

### 0.8 — Merge

- HDR;
- Panorama.

### 0.9 — Stabilization

- large catalogs;
- performance;
- metadata/XMP;
- recovery.

### 1.0

Полноценная стабильная версия для ежедневного использования.

---

## 97. Требования к работе Codex

Codex должен действовать как senior software engineer.

Перед реализацией каждого крупного блока:

1. изучить существующую архитектуру;
2. определить зависимости;
3. описать решение;
4. разбить задачу на небольшие изменения;
5. написать код;
6. написать тесты;
7. запустить build;
8. запустить тесты;
9. исправить ошибки;
10. проверить отсутствие regressions.

Не оставлять псевдокод вместо реализации, если задача требует рабочего функционала.

Не пытаться за один проход реализовать всё ТЗ.

Работа ведётся итерациями:

```text
Issue
↓
Architecture check
↓
Implementation
↓
Tests
↓
Local verification
↓
Commit
↓
PR-ready state
```

---

## 98. Правила изменения архитектуры

Если для реализации новой функции требуется серьёзно изменить архитектуру:

не делать локальный workaround.

Сначала обновить:

```text
ARCHITECTURE.md
```

и при необходимости соответствующий ADR.

---

## 99. Definition of Done

Функция считается завершённой только если:

- реализована;
- интегрирована в настоящий workflow;
- имеет UI, если это пользовательская функция;
- сохраняет состояние;
- работает после restart;
- имеет error handling;
- имеет тесты;
- не повреждает originals;
- не блокирует UI тяжёлыми операциями;
- работает на поддерживаемой версии macOS/Apple Silicon.

Наличие класса или кнопки без end-to-end workflow не считается реализацией.

---

## 100. Первая функционально законченная версия

Первая версия считается пригодной к реальному использованию, когда пользователь может:

1. создать каталог;
2. импортировать RAW;
3. быстро просмотреть фотосессию;
4. поставить Pick/Reject и rating;
5. открыть RAW в Develop;
6. исправить WB;
7. изменить exposure;
8. восстановить highlights/shadows;
9. изменить HSL;
10. применить curve;
11. сделать crop;
12. применить lens correction;
13. повысить резкость;
14. убрать noise;
15. создать локальную маску;
16. применить preset;
17. синхронизировать настройки на группу;
18. экспортировать JPEG;
19. закрыть приложение;
20. открыть снова и увидеть все правки.

Если любой из этих сценариев не работает стабильно, версия не считается законченной.

---

## 101. Первая задача Codex

Первый development milestone:

```text
M0 — RAW Vertical Slice
```

Codex должен:

1. создать структуру публичного GitHub-проекта;
2. добавить open-source license;
3. добавить `.gitignore`;
4. добавить CI;
5. создать CMake;
6. создать Qt 6 macOS application;
7. подключить LibRaw;
8. подключить SQLite;
9. создать catalog schema;
10. реализовать открытие `.NEF`;
11. декодировать настоящий RAW;
12. отобразить его в viewer;
13. вывести EXIF:
   - camera;
   - lens;
   - ISO;
   - shutter;
   - aperture;
   - focal length;
14. реализовать Exposure slider;
15. хранить Exposure non-destructively;
16. автоматически сохранять edit;
17. перезапустить приложение;
18. восстановить фотографию;
19. восстановить Exposure;
20. добавить unit/integration tests;
21. убедиться, что CI зелёный.

Milestone считается завершённым только после end-to-end сценария:

```text
git clone
↓
build
↓
run
↓
open Nikon D7500 NEF
↓
change Exposure
↓
close app
↓
run again
↓
Exposure restored
```

---

## 102. Текущий platform scope

До отдельного изменения ROADMAP считать:

```text
macOS Apple Silicon = supported
Windows = out of scope
Linux = out of scope
Intel Mac = out of scope
```

Не тратить development time на неподдерживаемые платформы.

Архитектуру проектировать аккуратно, но соблюдать YAGNI.

---

## 103. Главный критерий проекта

Цель — не визуально воспроизвести Lightroom.

Цель:

> Создать быстрый, надёжный, color-managed, non-destructive RAW photo workflow, которым фотограф действительно сможет заменить Lightroom при ежедневной работе.

При конфликте между количеством функций, архитектурой и качеством изображения выбирать:

**correctness → сохранность данных → image quality → performance → UX → количество функций.**
