# VampDAW 0.4.7-alpha

Blood Stems stability fix:

- Opening Sample Lab → Blood Stems no longer calls JSON `value()` on the null
  result before the first separation. That code path threw an exception during
  the native paint callback. Missing, older and incorrectly typed preferences
  fall back to valid choices.
- Native Blood Stems callbacks catch C++ exceptions before they cross the Windows
  callback boundary. Failed panel creation is cleaned up, and errors are reported
  from the main message loop. The project remains available.
- Worker replies are validated before preview/import; failed loads clear partial
  previews. Fatal worker exits retain their hexadecimal exit code, including
  missing DLLs, incompatible CPU instructions and memory errors. A subsequent
  job can run in the same DAW instance.
- The progress control is explicitly initialized. Each opening/start/error is
  journaled in `%LOCALAPPDATA%\VampDAW\logs\blood-stems.log` for an installed copy,
  or `projects\logs\blood-stems.log` beside a portable copy. Each separation's
  output folder contains `host.log` and `engine.log`.

Install the update with the Blood Stems component selected (the default).
Save your project and close the old VampDAW instance before updating.
No global Python installation is required.

Validation includes native hidden-window callback tests for initial paint,
malformed preferences, resizing, English/Russian text, close/reopen and exception
containment; real CPU separation and cancellation; fatal-worker/malformed-reply
fixtures and recovery; existing core tests. These are automated tests, not visual
inspection or confirmation on the affected external PC. Its processor and crash
dump were not supplied; this fix addresses a reproduced local code defect.
