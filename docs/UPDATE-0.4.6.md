# VampDAW 0.4.6-alpha

- Sample Lab has Editor / Blood Stems tabs. Local four/six-stem separation and
  specialist wind extraction, settings, progress, cancel, preview and Playlist import.
- Views → Project Pool / Patterns & Audio: project patterns and unique audio files,
  search, preview, double-click or Enter to edit, Space to preview, drag to Playlist
  or Add at cursor. Pattern drops on another channel create an editable copy.
- Settings → Customize theme, or Settings → Themes → Customize theme: nine colors,
  live preview, HEX on right-click, save/cancel, import/export .vamptheme files.
  The seven factory themes and independent animation settings remain available.
- Right-click the Playlist ruler to clear the selected time range. Clip context
  actions elsewhere retain their existing behavior.
- Device windows use buffered painting and avoid repeatedly repainting native
  controls from status timers, addressing dropdown flicker.
- User sound/plugin folder lists use atomic, backed-up settings with concurrent
  window merging. Offline folders are retained until explicitly removed.
- Project v7 stores VST states as base64 and reads legacy byte arrays while
  streaming. The old 256 MB JSON file restriction is removed. Versions 1–7 are
  readable; save a copy before migrating an important project.

Verification: a synthetic 310,378,737-byte legacy project opened and re-saved as
an 11,186,631-byte compact project with byte-exact plugin state. Settings restart,
backup recovery and concurrent edits, real VST state round trips, Project Pool
placement, custom theme files, real CPU separation and worker cancellation passed.
The user's affected project is on another PC and was not tested directly.
Native visual and mouse-interaction checks were unavailable because the desktop
automation connection failed; do not treat compilation as visual verification.
