# Согласованные уточнения UX contract от 2026-10-19

Этот документ сохраняет исходные формулировки и три уточнения, явно согласованные пользователем. Уточнения относятся только к `UX-APP-014`, `UX-CRP-010` и `UX-VIS-003`. Они согласуют проверяемое поведение с текущими UI и policy; они не добавляют поддержку отсутствующих действий и не меняют product policy.

У всех трёх IDs сохранены исходные `severity`, `scope=CURRENT` и `mode=REGRESSION`. Изменение текста contract не регистрирует adapter, не присваивает `PASS`/`NOT_APPLICABLE`/`NOT_IMPLEMENTED` и не изменяет qualification counts. Для квалификации нужны отдельные реальные runtime evidence. Остальные case contracts не изменены. Оригинальный пользовательский файл спецификации не изменяется.

## UX-APP-014

Исходная формулировка (`P2`, `CURRENT`, `REGRESSION`):

> ширины 899/900/999/1000/1199/1200/1450 → no layout oscillation.

Согласованная формулировка:

> Native OS user resize requests 899/900/999 remain clamped to actual width ≥1000; supported widths 1000/1199/1200/1450 preserve readable controls and stable layout without oscillation.

Основание: `apps/desktop/qml/Main.qml` задаёт `minimumWidth: 1000` и `minimumHeight: 640`. Значения 899/900/999 — запросы пользователя через настоящий механизм изменения размера окна OS; ожидаемая фактическая ширина остаётся не меньше 1000. Прямой `QWindow::resize()` с обходом native minimum не является этим input contract.

Проверка должна подтвердить фактическую native/Qt геометрию после каждого такого запроса, а затем стабильный layout и читаемость/доступность реализованных controls при поддерживаемых ширинах 1000/1199/1200/1450. Высота должна оставаться поддерживаемой (не меньше 640). Квалификация не требует и не объявляет поддержку фактического окна шириной 899/900/999. Это небольшой функциональный проход, без многократного stress resize.

## UX-CRP-010

Исходная формулировка (`P1`, `CURRENT`, `REGRESSION`):

> Done/Cancel leaves no transient overlay residue.

Согласованная формулировка:

> Actual Crop Done terminates the crop interaction with no transient overlay/input residue or extra history edit; committed crop settings and neutral transient state remain correct. No Crop Cancel contract is implied.

Основание: `apps/desktop/qml/CropPanel.qml` предоставляет кнопку `cropTool` с состояниями `Edit crop`/`Done`. `EditorCrop::setCropMode()` завершает edit и меняет crop mode; отдельного продуктового Crop Cancel/rollback нет. `MouseArea.onCanceled`, Undo, Reset, выход из режима и навигация не являются заменой отсутствующего Crop Cancel.

Проверяется настоящий Crop input и его естественное завершение, затем настоящий click Done. После Done crop mode выключен, overlay и его guides/dimming больше не рисуются и не перехватывают input, drag/pressed state не остаётся активным. Полный документ сохраняет ожидаемый crop; Done не создаёт дополнительную history edit или дополнительную revision при отсутствии новой правки. Сброшены относящиеся к этому взаимодействию transient diagnostic/preview/gesture state. Последующее обычное движение/нажатие вне Crop не меняет crop скрытым gesture; повторный вход/Done без правки также не добавляет history. Один обычный Undo/Redo должен обращаться к исходной crop edit, а не к пустому Done.

Визуальный oracle должен наблюдать фактическую composited scene, поскольку provider pixels не содержат Canvas overlay. Нужны независимый ожидаемый ROI/fixture и положительное наблюдение активного overlay до Done; совпадение двух снимков после Done само по себе не доказывает отсутствие residue. Это требование к будущему adapter, а не утверждение о выполненном runtime.

## UX-VIS-003

Исходная формулировка (`P2`, `CURRENT`, `REGRESSION`):

> exporter current filename 255 chars.

Согласованная формулировка:

> Exporter current filename uses the longest legal expanded stem (230 UTF-8 bytes) plus format extension: full logical text matches the actual output basename, the bounded label remains readable without overlap, and intentional middle elision is allowed. Overlimit expanded stems are rejected before file execution without changing files or documents.

Основание: `apps/shared/ExportSettings.cpp::filename()` однократно раскрывает template, затем ограничивает `result.toUtf8().size()` значением 230 **до** добавления `options.extension()`. Это bytes expanded stem, не число glyphs, Unicode code points или UTF-16 units. Отдельный лимит template `naming.size() <= 120` не является лимитом expanded stem. Для fixture границы используется существующий `{filename}`. Текущие расширения `.png`, `.jpg`, `.tif`, `.dng` занимают четыре ASCII bytes; в свежей папке без collision stem 230 bytes даёт basename 234 bytes. Ветка conflict-rename может добавить suffix после проверки stem; она не используется для этой точной границы и не объявляется ограниченной 234 bytes.

`ExportController::next()` публикует полный `item.destination.filename()` в `currentFile`: это выходной basename с расширением, не исходный filename и не полный path. Текущий `Main.qml` Label имеет `text: exporter.currentFile`, `Text.PlainText`, `Text.ElideMiddle` и ограниченную layout ширину. Intentional middle elision допустим: проверяется полный logical `text`, совпадение с реальным output basename, корректная читаемая отрисовка в bounds без overlap с соседними controls. Не требуется уместить все glyphs в одну строку без elision. У этого Label нет explicit tooltip или `Accessible.name`; уточнение не требует добавлять их. Если backend уже предоставляет accessible text, он должен соответствовать полному logical имени; отсутствие нового explicit API не подменяется выдуманным требованием.

Boundary fixture должна независимо измерять UTF-8 bytes раскрытого stem, а не сравнивать результат с той же production helper. Например, 115 букв `я` дают 230 bytes; 115 `я` плюс `a` дают 231 byte. Fresh destination исключает conflict suffix, `.png` фиксирует ожидаемый полный basename. Нужны настоящая работающая очередь с наблюдаемым `currentFile`, сохранённый output и независимая проверка его basename/содержимого, а также неизменность source/document/catalog identities. Одного значения скрытого Label после завершения недостаточно.

Overlimit planning rejection проверяется отдельной ветвью: запрос через реальные Export settings/action отклоняет expanded stem >230 bytes до старта/commit любого file item. Не создаются новые output/temp, неизменны sources, документы, настройки и существующие output/sentinels. Под «очередь не изменена» здесь понимается отсутствие выполненных файлов отклонённого batch: published queue total/completed/succeeded/skipped остаются нулевыми и currentFile пуст. Это не обещание сохранить counters/status/error прежнего batch: `startIDs()` законно очищает их перед асинхронным planning. Сообщение об ошибке и завершение planning ожидаются; progress/status предыдущего запуска не являются immutable oracle.

Ни эта граница, ни читаемость Label не объявляют поддержку 255-character exported names и не меняют действующий лимит. Причина выбора исходного product cap не устанавливается этим уточнением.
