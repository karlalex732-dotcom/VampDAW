# VampDAW

![VampDAW icon](assets/vamp.svg)

**A vampire-themed digital audio workstation for Windows 11.** Developed by **Karsei**, licensed under **GPL-3.0-only**.

[Русский](README.ru.md) · [Build instructions](README.txt) · [Alpha tester guide](docs/ALPHA-TESTING.md) · [Release notes](docs/RELEASE-0.4.19.md)

## Download

Current version: **0.4.19 Alpha**, tag **v0.4.19-alpha**.

Open this repository's **Releases** section and download `VampDAW-0.4.19-alpha-win64-Setup.exe` from **Assets**. The installer supports **Windows 11 x64 on Intel and AMD**, works offline, and installs for the current user without administrator rights. It includes the audio encoder, corresponding source and an optional CPU-based Blood Stems runtime with two default models.

This source repository contains no compiled installer. The explicit `VampDAW-0.4.19-source.zip` release asset contains the corresponding source distributed with the binary. Compare downloads with `SHA256SUMS.txt` in the release.

## Make music

- **Arrange:** Playlist, patterns, audio clips, markers, drawable automation, 512 tracks and nested folders.
- **Write:** Piano roll with scale highlighting, chord/melody/bass generators and a step sequencer with genre/subgenre drum generation.
- **Record:** external audio into the Playlist, per-channel input selection, REC and monitoring; ASIO, WASAPI and USB/MIDI.
- **Mix:** routing, sends, sidechain controls, up to 16 plugin slots per channel and Master, VST2/VST3 x64 and 46 native audio effects.
- **Assist:** Oracle Assist analyzes sound locally and suggests editable chains, with 30 scenarios and a 1–30-second listening range.
- **Edit audio:** trim, fades, stretching, bounce in place, track/folder freeze, Sample Lab and Blood Stems separation.
- **Customize:** English/Russian UI, themes, animation settings, a 60–240 interface FPS cap and configurable multicore DSP.
- **Deliver:** WAV, MP3, FLAC, OGG Vorbis, AAC/M4A, Opus and AIFF export.

### New in 0.4.19

Resize the **Piano roll vertically** and the **Sequencer horizontally**. Show 3–5 octaves and up to **16 bars / 256 steps per row**. Preferences persist; double-click a resize edge to restore the automatic size. Editors resize inside the main workspace.

## Alpha status

This is a pre-release without warranty. The application and installer are unsigned. Back up projects and their source audio before testing. ARM64, 32-bit plugins, AU and AAX are not supported. Project import from other DAWs is partial.

**ARA 2 is experimental.** Integration tests use an original test plugin; commercial Melodyne and Vovious versions have not been tested. See [ARA setup and limitations](docs/ARA.md). Do not assume compatibility with every plugin, audio interface or MIDI controller. Comparative CPU savings have not been established.

## Build and contribute

The application is native C++17 for Windows x64. See [README.txt](README.txt) for the pinned Zig 0.15.2, codec sources, test build and optional installer/stem-runtime setup. Start with `build.ps1 -Tests` after preparing the dependencies, then build the application with `build.ps1`.

See [CONTRIBUTING.md](CONTRIBUTING.md) for reports and proposed changes. Historical `docs/UPDATE-*` files describe their own versions; the current ARA status is the one above. Original source files are covered by `SOURCE-SHA256.json`; the added GitHub presentation files are identified in `GITHUB-PACKAGE.json`.

## License

[GNU GPL version 3 only](LICENSE.txt). Third-party components keep their own licenses; see [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt) and `licenses/`. No external commercial plugins are bundled.
