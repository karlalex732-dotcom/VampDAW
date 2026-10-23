# VampDAW 0.4.19 Alpha — tester guide / инструкция тестировщика

## English

1. Use **Windows 11 x64, build 22000+**, on Intel or AMD. Install your audio-interface driver and any external x64 VST2/VST3 plugins separately.
2. Close VampDAW, back up projects and their source audio, then run `VampDAW-0.4.19-alpha-win64-Setup.exe`. Default location: `%LOCALAPPDATA%\Programs\VampDAW`. The installer and app are unsigned. Do not disable Windows protection to install them; report a detection with its exact name and file path.
3. Choose whether to include **Blood Stems**. The full installation works offline with the default CPU runtime and 4/6-stem models. Optional specialist models require separate downloads. NVIDIA CUDA is not required for the included CPU runtime.
4. Launch VampDAW. In Settings choose the interface language, audio device, sample rate and buffer; add your own sound/plugin folders and scan plugins.
5. Add a sample or VST instrument to a track. Create a pattern in the Sequencer or Piano roll and place it in the Playlist. Add effects from the browser or the channel slots. For external recording select the track input, enable its REC/MON controls as needed and use the Playlist recording transport.
6. For the new editor sizes, drag the **bottom edge of Piano roll** or **right edge of Sequencer**. Select **oct ▾** for 3/4/5 octaves and **Bars** for up to 16 bars. Double-click the resize edge to reset. Preferences persist after restarting.
7. Use **Export** for WAV, MP3, FLAC, OGG Vorbis, AAC/M4A, Opus or AIFF. Keep the installed encoder and support folders with the application; copying only `VampDAW.exe` is not a complete installation.

Projects and preferences are stored outside the program folder, under Windows Documents and `%LOCALAPPDATA%\VampDAW`. Uninstalling preserves them. ARA 2 is experimental: Melodyne/Vovious compatibility is unverified; see the supplied `docs/ARA.md`.

### Useful alpha checks

- Save, close and reopen a small project with its audio available; compare playback and plugin state.
- Resize editors, test 3/4/5 octaves, edit the last step of bar 16, then restart and check view settings.
- Record through your own interface, test monitoring and the chosen buffer. Hardware/controller compatibility needs device-specific testing.
- Export a short section and listen to the file outside VampDAW.
- In a bug report include the exact version, Windows build, CPU, audio driver, sample rate/buffer, plugin versions, steps, expected behavior and actual result. Never share passwords, plugin licenses or unlicensed audio.

SHA-256 verification in PowerShell: `Get-FileHash -Algorithm SHA256 .\VampDAW-0.4.19-alpha-win64-Setup.exe`. Compare it to the release's `SHA256SUMS.txt`.

## Русский

1. Нужна **Windows 11 x64 от сборки 22000**, Intel или AMD. Драйвер аудиоинтерфейса и сторонние VST2/VST3 x64 установите отдельно.
2. Закройте VampDAW, сохраните резервные копии проектов и исходного аудио, запустите `VampDAW-0.4.19-alpha-win64-Setup.exe`. Папка по умолчанию — `%LOCALAPPDATA%\Programs\VampDAW`. Программа и установщик без цифровой подписи. Не отключайте защиту Windows ради установки; если обнаружена угроза, сообщите точное название и путь.
3. Выберите установку **Blood Stems**, если нужен стем-сепаратор. В полном комплекте есть CPU-движок и модели на 4/6 стемов для офлайн-работы. Специализированные модели загружаются отдельно. Для комплектного CPU-движка NVIDIA CUDA не требуется.
4. В настройках выберите язык, звуковое устройство, частоту и буфер. Добавьте пользовательские папки звуков и плагинов, запустите сканирование.
5. Добавьте семпл или VST-инструмент на дорожку, создайте паттерн в секвенсоре/пиано ролле и поместите в плейлист. Эффекты добавляются из браузера или слотов канала. Для внешней записи выберите вход на дорожке, используйте REC/MON и запись транспорта плейлиста.
6. Тяните **нижнюю границу пиано ролла** или **правую границу секвенсора**. В **окт ▾** выберите 3/4/5 октав, в **Такты** — до 16 тактов. Двойной щелчок по границе сбрасывает размер. Настройки вида сохраняются после перезапуска.
7. В **Export** доступны WAV, MP3, FLAC, OGG Vorbis, AAC/M4A, Opus, AIFF. Сохраняйте рядом с приложением установленные служебные папки: одного `VampDAW.exe` недостаточно.

Проекты и настройки лежат вне папки программы — в Documents и `%LOCALAPPDATA%\VampDAW`. При удалении программы они сохраняются. ARA 2 экспериментальная; совместимость Melodyne/Vovious не подтверждена. Инструкция — в установленном `docs/ARA.md`.

### Что проверить

- Сохранение/повторное открытие небольшого проекта вместе с аудио и состояниями плагинов.
- Изменение размеров, 3/4/5 октав, последний шаг 16-го такта и сохранение вида после перезапуска.
- Запись и мониторинг через свой интерфейс с выбранным буфером.
- Экспорт короткого участка и прослушивание готового файла вне VampDAW.
- В отчёте: точная версия, сборка Windows, CPU, драйвер, частота/буфер, версии плагинов, действия, ожидаемый и реальный результат. Не прикладывайте пароли, лицензии и чужое аудио без разрешения.

Для SHA-256 в PowerShell: `Get-FileHash -Algorithm SHA256 .\VampDAW-0.4.19-alpha-win64-Setup.exe`. Сравните с `SHA256SUMS.txt` релиза.
