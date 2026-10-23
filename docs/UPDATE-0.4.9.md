# VampDAW 0.4.9-alpha

Маркеры в Playlist можно редактировать прямо в песне:

- Перетащить флажок с текущей привязкой; Alt временно отключает её.
- Двойной клик или F2 меняет название.
- ПКМ открывает название, точную позицию `такт:доля`, цвет и удаление.
- После выбора Delete удаляет только маркер, Escape снимает выбор.
- ←/→ сдвигают выбранный маркер на шаг сетки, Shift+←/→ — на такт
  при выключенной игре с компьютерной клавиатуры.
- Изменения сохраняются в проекте и отменяются через Ctrl+Z.

Night Arranger теперь по умолчанию заменяет расстановку на участвующих дорожках.
Append to song остаётся отдельным выбором. При каждом Apply **все маркеры песни**
заменяются новым планом секций, включая переименованные маркеры и старый конец.
Это также очищает маркеры из проектов прежних версий. Подсказка в аранжировщике
указывает на замену. В режиме замены удаляется прежний диапазон повтора;
длина песни соответствует новому плану и оставшимся клипам защищённых дорожек.
Frozen, External MIDI и дорожки с ролью Ignore сохраняются. Паттерны и исходные
аудиофайлы не удаляются. Всё применение — одна операция Undo.

Доступно 20 жанров: Electronic, Pop, Hip-hop, Rock, Ambient, Dubstep, Riddim,
House, Techno, Drum & Bass, Trap, Phonk, Trance, Synthwave, Reggaeton, R&B,
Lo-fi, Future bass, Metal и Cinematic. У каждого своя последовательность секций,
распределение длины, энергия и пороги вступления партий. Длительность — от 32
до 512 тактов; длину и энергию каждой секции можно менять отдельно.
Это локальная аранжировка ваших паттернов и аудио по правилам: она не создаёт
новые звуки и не меняет темп или ритм исходного паттерна автоматически.

English: Playlist section flags support drag, rename (double-click / F2), exact
bar:beat position, color and deletion via right-click; Delete removes a selected
marker and Escape deselects it. Marker edits support Undo and project persistence.
Applying an arrangement replaces the complete section map, including markers from
older releases. Replacement is the default clip mode; append remains optional.
Twenty genre profiles vary section names, lengths, energy and instrument entries.
Original pattern/audio sources and frozen, external or ignored tracks are retained.

Validation covers deterministic plans for all 20 genres at nine lengths, repeated
replacement, shorter songs, marker editing/persistence, native hidden-window mouse
and keyboard callbacks, Undo/Redo and shared-renderer previews in both languages.
Desktop interaction and the user's other PC are not covered by these checks.

Импорт FLP: современное поле BPM имеет приоритет над устаревшим независимо
от порядка событий; для старых проектов объединяются целая и дробная части.
Нулевое старое поле больше не перезаписывает корректный темп. Если современное
поле повреждено, но старое корректно, импорт восстанавливает его с предупреждением.
Ошибка некорректного темпа теперь содержит фактический BPM.
Диапазон VampDAW расширен до **10–999 BPM** в проекте, ручном вводе, FLP/ALS
и MIDI. Значения сохраняются без подмены на 30 или 300; проверены сохранение,
загрузка, звук и округление темпа в MIDI. Для открытия таких проектов на другом
ПК также нужна 0.4.9 или более новая версия. Конкретный файл пользователя пока
не предоставлен: регрессия проверена на созданных FLP/ALS с несколькими вариантами
полей темпа, не на его исходном проекте.
