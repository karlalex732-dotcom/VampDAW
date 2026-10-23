# VampDAW 0.4.8-alpha

Blood Stems is simpler and fits inside Sample Lab without scrolling:

- **Separation / Разделение**: source, model, target stems, quality, format,
  sample rate, levels, selected range, auto import and source muting.
- **Fine tuning / Тонкая настройка**: shift passes, overlap, segment length,
  CPU threads, processing device, gain, mono, DC removal and sum consistency.
- The output folder, progress, preview and import controls stay in the same
  places on both pages. Width and height both affect layout. The panel uses the
  current theme, custom controls, hover/focus feedback and tooltips for full values.
  Wheel gestures over closed lists no longer accidentally change parameters.

If CUDA is unavailable, separation continues on CPU. The result and journal record
the requested device, actual device and fallback reason. The bundled engine is a
CPU build: owning an NVIDIA card alone does not enable CUDA acceleration.
Actual GPU inference needs a compatible CUDA-enabled runtime, which is not bundled.

Closing auxiliary windows and built-in/VST editor wrappers now follows one focus
policy: hide or destroy that window, release its mouse capture, then return control
to its visible, enabled owner if the closing window was active. This preserves the
owner's normal/maximized state and does not restore a deliberately minimized owner.
USB/MIDI/Audio and Aggregate Device close buttons use the same path as the title-bar
cross. Escape inside built-in editor child controls also closes the editor.
F12 additionally closes the plugin picker and returns to the main workspace.
Main-window close still exits the application with the existing save check.

Checks: hidden native callback and layout tests cover both languages, two tabs,
four sizes at four scales, malformed preferences/results, wheel behavior and
close/reopen. Lifecycle callbacks cover all 47 built-in editors and six auxiliary
windows, without modifying the project. Shared-renderer PNG previews were inspected
for compact and regular layouts. Core/worker tests and real offline CPU inference
with an explicit CUDA request check recovery and CPU fallback.

Desktop interaction inspection was unavailable because the Computer Use connection
failed. The focus behavior with real third-party editors and the affected external
PC still needs manual alpha testing. No physical NVIDIA inference was tested.

Save the project and close the old VampDAW instance before installing. Keep the
Blood Stems component selected. Publisher: **Karsei**. License: **GPLv3**;
corresponding source is included in the distribution.

## Arrangement and Playlist

**Night Arranger**, immediately after Automation, is an offline, rule-based
arrangement assistant. It analyzes track names and MIDI register/duration, and
uses existing patterns/audio. It offers Electronic, Pop, Hip-hop, Rock and Ambient
templates, 32–256 bar targets, section names/lengths/energy, manual track roles,
density, variation, deterministic seeds, drum variants, and protected vocal takes.
It does not synthesize new audio or use a neural model. Preview does not modify
the project. Append is the default; Apply is one Undo. Existing automation stays
at its original positions, as warned in preview. Frozen/external MIDI tracks skip.

Right-clicking a clip now opens its action menu. **Bounce in place** renders
the selected clips on that track, including inserts and sidechain processing,
to a float WAV. It replaces only those placements with an adjacent clean audio
lane at the same timeline position; original pattern sources remain available.
The new lane preserves fader/pan, their automation, folder and outgoing routing.
Insert automation is baked and removed from the clean lane to avoid double FX.
Optional tail: 2 seconds. Cancel keeps source clips; Undo restores them after commit.

Ctrl+drag starts a selection rectangle even on a clip; Ctrl+click toggles a clip.
Selection auto-scrolls across tracks and time. Delete removes placements, retaining
pattern/audio sources. The ruler chooses label spacing from pixel width: multiple
bars, bars/beats, or bars/beats/ticks. Labels have at least 72 logical pixels of
spacing. Marker titles use a separate row. BAR/BEAT/TICK have fixed cells and
monospaced digits; tick resolution is 96 per beat.

## Folder freeze and mixer

Folder menus expose Freeze and Unfreeze including nested subfolders. A freeze
queue renders sources before their buses, skips existing freezes/external MIDI,
shows overall progress, allows cancellation, and shares one Undo snapshot. Already
completed tracks remain frozen after cancellation; Undo restores the whole batch.
Unfreeze restores all nested sources and their insert states in one operation.
The Master and track strips are about 70 logical pixels wide, with the number
of visible strips adapting to available width. Master stays on the right.

## Library, loading and appearance

Project Pool has sortable Name/Channel/Length/Clips columns, search, placement
filters, multiselection, keyboard actions, sequence/layer placement, batch drag
to Playlist, rename, find placements and reveal/copy audio paths.

A corner loading panel covers project/sample/plugin loading, plugin scanning,
sample processing, separation, exports, bounce and freeze. Known progress is
reported numerically; unknown progress uses an indeterminate bar. It does not
take focus and is hidden while a different application is foreground. Vendor
plugin calls can block the UI temporarily, so their indeterminate animation may
pause until control returns.

Native auxiliary controls follow the active palette; third-party plugin/driver
editors retain their own UI. An embedded archival photograph of Bela Lugosi as
Dracula replaces the drawn face, with a head-and-shoulders crop and fade in/out.
See THIRD_PARTY_NOTICES.txt for the specific photograph's provenance.

Both application and test binaries embed a Common Controls v6 manifest, fixing
the GetWindowSubclass entry-point error reported during verification.

## Hover glow
Settings > Animation > **Hover glow** enables a finite 160 ms fade in and
260 ms fade out on buttons, piano keys and parameters. CINEMA adds a stronger
theme-colored halo; FULL and LOW use a quieter version. The independent switch
is saved in settings. Global OFF also disables it. Settled hover states stop
requesting animation frames. Native Vamp buttons, list selectors, effect controls
and Oracle actions follow the same preference. Third-party plugin UIs are unchanged.
Hidden-window checks cover finite transitions, rapid reversal, native controls,
piano rows, restart persistence and both disable paths. English/Russian animation
settings and piano previews were inspected using the shared renderer.

## VST2 and plugin folders
64-bit VST2 DLLs can now be loaded as instruments or effects, alongside VST3.
The host supports note/CC events with sample offsets, tempo/position, automation,
state chunks or parameter fallback, latency reporting, mono/stereo/sidechain input,
native editors and sample-rate changes. The browser, picker, Starred and drag/drop
accept both formats. Existing 16-slot limits on each track and Master apply to both.
No proprietary VST2 SDK is distributed; the interface header is VeSTige (GPL).
32-bit bridges, VST2 shell containers, MIDI output and arbitrary multichannel
output routing are outside this alpha. Plugins run in the application's process;
the catalog scanner uses isolated probe processes. A faulty running plugin can
still terminate the application.

The first catalog scan starts after the main window opens. Known system folders
and recognizable paths in installed DAW settings are merged into editable User
Plugins folders: VST registry paths, shared plugin search paths, REAPER INI and
selected Ableton/Steinberg/PreSonus/Bitwig preference files. The reader does not
understand every proprietary configuration format and does not scan entire drives.
Deleted auto-discovered entries stay removed; Settings > Browser > **Find DAW
folders** explicitly rediscovers them. Only installed, licensed plugins can load.
Filesystem enumeration and plugin catalog work run in the background.

Four original test DLLs exercise MIDI instruments, stereo/mono effects, sidechain,
state, automation, timing and sample-rate changes. The installed Auburn Sounds
Lens 64-bit VST2 was also probed successfully (132 parameters, processing and state
restore). This does not certify every VST2 plugin.

## MIDI controller profiles
Settings > Audio/MIDI > Controllers opens a branded profile and mapping editor.
Use a separate musical MIDI input and DAW/control port. Profiles include Mackie
Control/MCU, Arturia KeyLab Essential mk3 in DAW/MCU mode, and the full-size Novation
Launchkey MK3 DAW protocol. Akai and Native Instruments controllers use generic
MIDI plus Learn; proprietary NKS/screen integrations are not implemented. Mini/MK4
Launchkey models are not claimed compatible with the MK3 mapping.

Available controls include transport, eight-channel banking, Master, mute/solo/REC,
faders, pan and eight parameters in the selected plugin slot. Learn supports
absolute and four relative CC modes, optional soft takeover and per-port/profile
settings. MCU/Launchkey feedback sends only changes; MCU fader touch temporarily
suppresses motor feedback. Standard MIDI CC, sustain, pitch bend and pressure
are forwarded to mapped VST3 controls; VST2 receives standard MIDI events.
Controller callbacks, feedback bytes, persistence and VST3 CC processing have
automated coverage. Physical controllers have not been available for verification.

## Initial FLP / ALS project import
SESSION > **Import project / FLP / ALS…**, Open or drag/drop accepts FLP and
Ableton ALS arrangement projects. Import runs in a separate Python process,
keeps the source unchanged, and creates an editable Vamp project with a report.
Save the result as .vamp. **Last import report** reopens the warnings. Escape
cancels a running import; a failed import keeps the current project.
Keep the default Blood Stems component selected: the importer shares its bundled
Python runtime. It does not download or run a third-party DAW.

The initial converter transfers constant tempo, MIDI notes/velocity, patterns and
arrangement placements, audio references and basic track information. ALS loops
become editable repeated/partial patterns. FLP rack parts are split into tracks;
only the first arrangement is read. Referenced audio is resolved relative to the
source and decoded into imported assets with the bundled encoder. Missing files
are listed and their audio placements are muted.

This is migration, not full project compatibility. Native instruments, plugin
instances/presets, automation, detailed routing/returns, time signatures, tempo
changes and exact warp/stretch behavior need manual reconstruction. ALS Session
clips and FLP automation channels are not imported. Instruments must be assigned
again; until then MIDI tracks use the current Vamp instrument. Warped/transposed
audio gets an explicit review warning. The source cannot be saved back to FLP/ALS.
Acceptance fixtures cover ALS gzip/XML loops and referenced audio, FLP 20/21
playlist record layouts, notes, corrupt input, source preservation and compilation
through the real Vamp renderer. No user-supplied FLP/ALS projects were available;
keep backups and inspect the import report before editing.
Windows status snapshots retry transient sharing locks. A persistently locked
progress indicator is nonfatal; regression checks exercise both cases.
