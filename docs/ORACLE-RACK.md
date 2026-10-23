# Oracle Rack 0.1.0 Alpha / Karsei

Oracle Rack is a local chain assistant and effect rack for Windows x64 VST3 hosts. It contains 30 sound-specific scenarios, a 1–30 second listening control, native Vamp effects, and an internal host for compatible VST2/VST3 effects. It does not require VampDAW, a cloud account, or a neural model.

## Install and use

1. Run `Oracle-Rack-0.1.0-alpha-win64-Setup.exe`, or copy the complete **Oracle Rack.vst3** bundle from the ZIP to a VST3 folder. Keep `OracleScan.exe` beside the inner VST3 binary.
2. The default installation is `%LOCALAPPDATA%\Programs\Common\VST3\Oracle Rack.vst3`. If your host does not search this standard per-user folder automatically, add its parent VST3 folder in that host and rescan.
3. Insert **Oracle Rack** as an audio effect on an instrument, vocal, bus, or Master. It starts with an empty, transparent rack.
4. Use **Scan plugins…** to scan standard and discovered plugin folders, add your own folder, or remove an unwanted search folder. Scanning is isolated in a helper process; a failed or slow candidate does not load into the audio processor. Cancel stops the scan. A plugin's own installation and license must already work on this PC.
5. Pick a scenario, or leave Auto profile on. Set 1–30 seconds, press **Analyze sound**, and play the source. Choose a source-specific scenario manually when the technical auto profile does not identify your intent.
6. Review the suggested chain. **Choose…** replaces an effect for that role; **Automatic choice** restores automatic selection. **Use the tuned Vamp effect** selects a built-in implementation. All-plugins mode includes both user and Vamp effects.
7. **Replace rack** loads this chain inside Oracle. It replaces Oracle's previous internal chain, so save a host preset/state before replacing a chain you need to keep. The host's Undo implementation may also restore it. **Open** opens an applied effect's own editor.

The scenario, processing amount, listening duration, and source choices describe a **preview**; changing them requires applying the preview. Host bypass is an audio-processing parameter and uses a short transition and compensated dry path. The header displays the applied chain's control mapping and, after analysis, measured stage levels. Right-click a stage to see all suggested and matched controls.

## How controls and levels are coordinated

The assistant matches a processing role to compatible effect metadata. It reads names, units and actual numeric readouts instead of assuming that 0.5 means a specific number of dB or milliseconds. It checks readout monotonicity and derives a value within the plugin's range. Units and unambiguous names must agree. Semantic aliases cover common compressor/filter/EQ controls; proprietary or ambiguous controls remain unchanged and are listed in the report.

After recording the listening interval, a calibration pass runs a representative portion of up to two seconds through fresh instances of the proposed chain. Downstream compressor/expander/de-esser thresholds follow the preceding level changes; the final trim/limiter input follows the measured level. Output peak and RMS are reported. Calibration is local and does not run in the live audio callback. A preview without captured audio uses scenario settings without measurement. The result is a technical starting point: review unmapped settings and listen, especially when using channel strips, multiband processors, or mastering suites.

## State, compatibility and files

Host projects/presets store the internal effect paths, states, source choices and Oracle settings. When an effect is missing, its state is retained for a later reinstall; a warning identifies the unavailable effect. The remaining rack continues with that stage bypassed.

Plugin search folders and catalog live in `%LOCALAPPDATA%\Karsei\Oracle Rack\library.json`. User plugins and their licenses are not included. Scanning does not upload files. The rack hosts up to 16 effects. Mono and stereo VST3 I/O are supported, at 8–384 kHz; internal processing is float32, with conversion for float64 host buffers. Individual hosted plugin capabilities can impose narrower limits. Changes in nested latency notify the host; some hosts restart processing while updating compensation. Nested processing is in the host process. Windows 32-bit effects, AU and AAX are not supported.

This alpha is checked with a separate VST3 test host and controlled external fixtures. This does not certify every commercial DAW or plugin. See the shipped verification report for the actual tests.

## Русская памятка

Установите Oracle Rack и добавьте его как эффект в своей DAW. Через **Scan plugins…** просканируйте плагины и при необходимости добавьте папки. Выберите сценарий, длительность 1–30 секунд и запустите **Analyze sound** во время воспроизведения. **Choose…** заменяет эффект конкретного этапа. **Replace rack** применяет предложенную цепочку внутри Oracle, заменяя предыдущую. Перед заменой сохраните нужное состояние в пресете/проекте хоста. **Open** открывает редактор эффекта. Язык переключается в меню сканирования.

Oracle проверяет названия, единицы, числовые показания и диапазоны регуляторов. После анализа он делает пробный прогон и согласовывает пороги и финальный gain с предыдущими этапами. Нераспознанные параметры остаются без изменений и показываются в отчёте по ПКМ на этапе. Их нужно проверить вручную; автоматический анализ не заменяет оценку результата на слух.

Сохраняйте всю папку `Oracle Rack.vst3`, включая `OracleScan.exe`. По умолчанию установщик использует `%LOCALAPPDATA%\Programs\Common\VST3`. Если хост не обнаружил плагин, добавьте эту папку в его поиск и повторите сканирование. Поддерживаются Windows x64 VST2/VST3-эффекты внутри VST3-версии Oracle. Настройки каталога — `%LOCALAPPDATA%\Karsei\Oracle Rack`; удаление установщика не стирает эту папку и проекты хоста.

## Source and license

GPL-3.0-only. Copyright Karsei. The installer and ZIP include the corresponding VampDAW 0.4.15 source archive, which contains `plugins/oracle`, `src`, plugin interface headers, license notices and `build-oracle.ps1`. Follow its source-build guide to obtain the pinned portable compiler. From the extracted source directory, run `build-oracle.ps1`; `-Tests` also builds and runs the rack checks.

This alpha exposes the main audio input/output; nested external sidechain and MIDI buses are not exposed. Rack state is limited to 256 MiB. These limits do not apply to the native Oracle Assist version, whose effects use normal VampDAW channel slots.
