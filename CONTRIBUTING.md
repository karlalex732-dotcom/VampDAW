# Contributing / участие

## Bugs

Use the **Bug report / Ошибка** issue form. Include the exact VampDAW version, Windows build, CPU, driver, sample rate/buffer, external plugin versions and repeatable steps. Describe the expected and actual behavior. A small project with legally shareable audio is useful; remove personal data, plugin activation keys and copyrighted material you cannot distribute.

For a Windows security detection, include its exact name, detected file path and the SHA-256 of the installer/file. Do not turn off protection as a workaround. Screenshots and logs should omit personal details.

## Changes

Describe the problem and proposed behavior before starting a large change. Keep each contribution focused. Follow the existing C++17/Win32 style and run the relevant tests from [README.txt](README.txt). UI changes should account for English/Russian, compact/wide layouts, high DPI and keyboard navigation. Audio processing should avoid allocation and blocking in the real-time callback.

Include the relevant verification and known limits. Do not commit generated executables, local settings, audio projects, proprietary plugin binaries, model weights or downloaded toolchains. Keep third-party license notices intact. New contributions are made under the project's GPL-3.0-only license unless explicitly identified with a compatible third-party license.

## Русский

Сообщайте об ошибках через форму **Bug report / Ошибка** с точной версией, системой, драйвером, частотой/буфером, плагинами и шагами. При обнаружении угрозы нужны её название, путь файла и SHA-256; отключать защиту не требуется. Удаляйте личные данные и лицензионные ключи из приложений к отчёту.

Для крупных изменений сначала опишите задачу. Проверяйте подходящие тесты, оба языка, масштаб Windows и компактные окна. Не добавляйте в Git проекты, звуки, настройки, бинарники, веса моделей и локальные инструменты сборки. Сохраняйте уведомления о лицензиях; изменения распространяются на условиях GPL-3.0-only.
