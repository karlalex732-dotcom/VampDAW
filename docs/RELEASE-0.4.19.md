# VampDAW 0.4.19 Alpha

**A vampire-themed DAW for Windows 11, by Karsei.** Build patterns, arrange audio, record instruments and mix with native effects or your own VST plugins. English and Russian interfaces are included.

## New in 0.4.19

- **Resizable Piano roll:** drag the bottom edge to change its height. Show 2, 3, 4 or 5 octaves using the **oct ▾** menu.
- **Wider Sequencer:** drag its right edge and choose **1, 2, 4, 8 or 16 bars** — up to 256 steps per row. Editing the last bar extends the pattern correctly.
- **Remembered workspace:** editor dimensions, note density and the Sequencer bar range persist between launches. Double-click a resize edge to restore the automatic size.
- **Smoother long-pattern editing:** the Sequencer indexes notes once per row rather than searching the entire pattern for every step.

These editors resize inside the main application workspace. This update keeps the existing audio engine and project format.

## Included in this alpha

- Up to **512 tracks**, nested folders, per-track heights, track/folder freeze and bounce in place.
- Playlist audio editing with trimming, fades, stretching and clip-header dragging; drawable automation.
- Piano roll with scale highlighting and chord, melody and bass generators; drum generation with genres, subgenres and reroll.
- A mixer with routing, sends, sidechain controls, recording/monitoring and up to **16 plugin slots per channel and Master**.
- Windows x64 **VST2/VST3**, ASIO, WASAPI and USB/MIDI workflows.
- **46 native audio effects plus Oracle Assist**, a local sound-analysis chain assistant with 30 scenarios and a 1–30-second listening range.
- **Blood Stems** in Sample Lab. The full installer includes an optional CPU runtime and the default 4-stem and 6-stem models.
- Customizable themes, animation levels, an interface frame-rate limit from 60 to 240, and an all-cores DSP option.
- Export to **WAV, MP3, FLAC, OGG Vorbis, AAC/M4A, Opus and AIFF**; the audio encoder is included.
- **Experimental ARA 2** for compatible VST3 audio editors, introduced in 0.4.18.

## Download and install

Open **Assets** below this release and download:

| File | Purpose |
| --- | --- |
| `VampDAW-0.4.19-alpha-win64-Setup.exe` | Offline Windows 11 x64 installer, 241,159,056 bytes. |
| `VampDAW-0.4.19-source.zip` | Complete corresponding source and dependency notices, 26,493,628 bytes. |
| `SHA256SUMS.txt` | SHA-256 checksums for the installer and corresponding-source archive. |
| `VampDAW-0.4.19-alpha-Tester-Guide.md` | Installation, quick start and testing notes in English and Russian. |

**Requirements:** Windows 11 x64, build 22000 or newer; Intel or AMD x64 CPU. ARM64 and 32-bit plugins are not supported. External plugins and audio-interface drivers must be installed separately. No developer tools or administrator account are required to install VampDAW.

Close VampDAW before updating. The default destination is `%LOCALAPPDATA%\Programs\VampDAW`. Keep a backup of existing projects and their source audio before testing. Uninstalling preserves user projects and settings.

## Alpha status and known limitations

- This is a **pre-release**, supplied without warranty. The installer and application are **not code-signed**; Windows may show a publisher/reputation warning.
- ARA protocol, audio access, archives, playback, export and UI behavior passed tests using an original test plugin. **Melodyne and Vovious have not been tested with this release.** Their compatibility is not yet confirmed. Commercial plugins are not bundled.
- Project import from other DAWs is partial; plugin state and unsupported features may not transfer exactly.
- Automated checks cover 44 editor layouts, mouse resize handlers, 3/4/5-octave pitch hit testing, all 256 steps, persistence and export to seven formats. They do not certify all third-party plugins, connected controllers, audio interfaces or desktop configurations. No comparative CPU-performance claim is made.

Report issues through this repository's **Issues** tab with the version, Windows build, audio driver, sample rate, buffer size, plugin versions and reproduction steps. Share only audio/projects you have permission to distribute.

## License

VampDAW is **GPL-3.0-only**. Corresponding source is supplied as an explicit release asset and inside the installer. Third-party components retain their own licenses and notices. Copyright and development: **Karsei**.

---

<details>
<summary>Русский — описание релиза</summary>

# VampDAW 0.4.19 Alpha

**DAW в вампирской тематике для Windows 11. Разработчик — Karsei.** Паттерны, аранжировка, запись инструментов и сведение со встроенными эффектами и собственными VST. Интерфейс доступен на русском и английском языках.

## Новое в 0.4.19

- **Высота Piano roll:** растягивайте нижнюю границу. Меню **окт ▾** показывает 2, 3, 4 или 5 октав.
- **Ширина секвенсора:** растягивайте правую границу. Выбор **1, 2, 4, 8 или 16 тактов** даёт до 256 шагов на строку. Поздние шаги корректно увеличивают длину паттерна.
- **Сохранение рабочего пространства:** размеры редакторов, плотность нот и диапазон секвенсора запоминаются. Двойной щелчок по границе возвращает автоматический размер.
- **Оптимизация длинных паттернов:** секвенсор индексирует ноты один раз на строку вместо повторного поиска для каждого шага.

Редакторы меняют размер внутри основного рабочего окна. Звуковой движок и формат проектов в этом обновлении не менялись.

## Возможности альфы

- До **512 дорожек**, вложенные папки, индивидуальная высота, фриз дорожек и папок, Bounce in place.
- Обрезка аудио, фейды, растяжение и перенос клипов за плашку названия; рисуемая автоматизация.
- Подсветка ладов и генераторы аккордов, мелодий и баса; генератор ударных с жанрами, поджанрами и рероллом.
- Микшер с маршрутизацией, посылами, сайдчейном, записью и мониторингом; до **16 слотов плагинов на канал и Master**.
- **VST2/VST3 x64**, ASIO, WASAPI и USB/MIDI.
- **46 встроенных эффектов и Oracle Assist:** локальный подбор цепочки по анализу звука, 30 сценариев и прослушивание от 1 до 30 секунд.
- **Blood Stems** в Sample Lab. В полном установщике есть отключаемый CPU-движок и стандартные модели на 4 и 6 стемов.
- Настраиваемые темы и анимации, ограничение частоты интерфейса от 60 до 240 FPS, режим использования всех ядер DSP.
- Экспорт **WAV, MP3, FLAC, OGG Vorbis, AAC/M4A, Opus и AIFF**. Кодировщик входит в установщик.
- **Экспериментальная ARA 2** для совместимых VST3-аудиоредакторов, добавленная в 0.4.18.

## Скачать и установить

В разделе **Assets** этого релиза:

| Файл | Назначение |
| --- | --- |
| `VampDAW-0.4.19-alpha-win64-Setup.exe` | Офлайн-установщик Windows 11 x64, 241 159 056 байт. |
| `VampDAW-0.4.19-source.zip` | Полные соответствующие исходники и сведения о зависимостях, 26 493 628 байт. |
| `SHA256SUMS.txt` | Контрольные суммы SHA-256 установщика и исходников. |
| `VampDAW-0.4.19-alpha-Tester-Guide.md` | Краткая инструкция для тестировщиков на двух языках. |

**Требования:** Windows 11 x64 от сборки 22000, процессор Intel или AMD x64. ARM64 и 32-битные плагины не поддерживаются. Сторонние плагины и драйвер интерфейса устанавливаются отдельно. Для установки VampDAW не нужны средства разработки и права администратора.

Перед обновлением закройте VampDAW и сохраните резервную копию проектов вместе с исходным аудио. Стандартная папка установки — `%LOCALAPPDATA%\Programs\VampDAW`. Удаление программы сохраняет пользовательские проекты и настройки.

## Статус альфы и ограничения

- Это **Pre-release** без гарантий. Установщик и приложение **без цифровой подписи**; Windows может показать предупреждение об издателе или репутации.
- ARA проверена собственным тестовым плагином: протокол, чтение аудио, архивы, воспроизведение, экспорт и интерфейс. **Melodyne и Vovious в этой версии не проверены**, совместимость пока не подтверждена. Коммерческие плагины в комплект не входят.
- Импорт из других DAW частичный: неподдерживаемые функции и состояния плагинов могут переноситься неточно.
- Автоматические проверки включают 44 компоновки редакторов, обработчики изменения размеров мышью, координаты нот в 3/4/5 октавах, все 256 шагов, сохранение настроек и семь форматов экспорта. Это не проверка всех сторонних плагинов, контроллеров, интерфейсов и конфигураций рабочего стола. Сравнительная экономия CPU не заявляется.

Об ошибках сообщайте во вкладке **Issues**: версия, сборка Windows, драйвер, частота, буфер, версии плагинов и действия для воспроизведения. Прикладывайте только проекты и аудио, которые можете распространять.

## Лицензия

**GPL-3.0-only.** Соответствующие исходники доступны отдельным файлом релиза и внутри установщика. Зависимости сохраняют собственные лицензии и уведомления. Автор и разработчик: **Karsei**.

</details>
