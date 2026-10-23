VampDAW 0.4.19-alpha — corresponding source / GPL-3.0-only

The source archive is included in every offline installer, in Sources, and is also
provided separately. No subscription or further permission is required to access it.
VampDAW is licensed under GNU GPL version 3. See LICENSE.txt. No warranty is provided.
Third-party files retain their own notices. No external VST plugins are included.

Contents:
  src/              Complete application source, including the native ASIO host.
  assets/           Editable SVG icon, application icon and Windows version resource.
  tests/            Core, workstation, export and device-buffer verification.
  third_party/      Exact VST3/JSON/ASIO headers and GPL VeSTige VST2 interface header.
  packaging/        Installer source, artwork and release scripts.
  dependency-sources/ Exact unmodified codec source archives (SHA-256 in dependencies.json).

Build on Windows 11 x64:
1. Extract the complete source archive into a directory without spaces, e.g. E:\VampDAW-src.
2. Download Zig 0.15.2 for Windows x86_64 from https://ziglang.org/download/0.15.2/
   and extract to tools\zig-x86_64-windows-0.15.2. This is a build dependency only.
3. Run PowerShell: .\build-codecs.ps1
   This uses the bundled source archives and downloads/extracts the pinned portable
   w64devkit compiler if missing. No machine-wide installation is performed.
   Run this from a normal user account. Windows tar.exe is used for source extraction.
   The build produces work\codec-prefix\bin\ffmpeg.exe, statically linked against
   LAME 3.100, libogg 1.3.6, libvorbis 1.3.7 and Opus 1.5.2.
   Exact configure/build flags are in packaging\build-codecs.sh. No proprietary codec
   libraries, network support, video encoders, or separate codec DLLs are needed.
4. Copy that ffmpeg.exe into tools\ffmpeg\bin\ffmpeg.exe. The application and
   test builds record its SHA-256 in src/encoder_digest.hpp. Keep the encoder
   alongside the application in tools/ffmpeg/bin/. No executable is extracted at runtime.
   Then run:
     .\build.ps1 -Tests
     .\build.ps1 -NoCopy -OutputName VampDAW-0.4.19.exe
   Zig compiles C++17 for native x64 Windows, with its static C++ runtime.
5. To build the installer, download Inno Setup 6.7.3 from the official release:
   https://github.com/jrsoftware/issrc/releases/tag/is-6_7_3
   Extract/install its compiler to tools\innosetup. It is not required at runtime.
   Install Python 3.11+ (standard library only), then run .\build-installer.ps1.
   Python packages below are also used by the optional local Stem Engine. This stages only the explicit distributable files,
   embeds this corresponding-source archive, and writes Setup.exe and checksums to dist.

Source and build archive hashes, download origins and compiler versions are recorded
in packaging\dependencies.json and the installed manifest.json. The scripts do not
bundle personal settings, audio projects, plugin paths, plugin binaries or development
tools. Compiler download URLs are build-time dependencies; testers install offline.

ASIO SDK 2.3.4 interface headers use their GPLv3 option. VST3 pluginterfaces revision
4f547e8 and nlohmann/json 3.12.0 use MIT. FFmpeg 8.1.3 is a separate process, with
its LGPL-licensed audio libraries; all exact source archives are included here.

To verify an installed copy from any working directory:
  VampDAW.exe --self-check "E:\Reports\installation-check.json"
For an isolated verification folder, append:
  --data-dir "E:\Reports\Isolated user data"
Only the self-check command supports this override; ordinary GUI storage remains
the user's Windows Documents and LocalAppData folders.

Optional local Stem Engine (included by default in the full installer):
Use CPython 3.12 x64. Create tools/stem-build-env with python -m venv. Install
tools/stems/requirements-lock.txt using that environment's Python and PyPI plus
https://download.pytorch.org/whl/cpu as an extra index for the pinned CPU wheels.
Run packaging/prepare-stem-runtime.py with that same base Python. It copies only
the relocatable interpreter/runtime; system Python is not modified. Exact worker,
wind adapter, vendor MIT sources, configs and model checksums are in tools/stems.
Use Demucs 4.0.1 get_model('htdemucs') and get_model('htdemucs_6s') with TORCH_HOME
set to tools/stems/models to populate the two default model checkpoints.
The source archive excludes interpreter binaries and model weights. Their notices
and pinned package list are supplied; specialist weights are downloaded on demand.
See docs/STEM-SEPARATOR.md for upstream links and scope.

PROJECT IMPORT AND VST2
tools/project-import/import_project.py is an original GPLv3 standard-library
Python adapter. It runs with the packaged Python runtime and bundled encoder;
no PyFLP package is needed. tests/project_import_tests.py accepts an application
path to verify its conversion fixtures through the real renderer.
third_party/vestige/aeffectx.h retains the VeSTige GPL-2.0-or-later header;
the combined application is distributed under GPLv3. Four original VST2 fixture
DLLs are built by build.ps1 -Tests. No external proprietary plugins are included.

Oracle Rack VST3 (standalone plugin for Windows x64 hosts):
  .\build-oracle.ps1
  .\build-oracle.ps1 -Tests
The Oracle tests build their own external-effect fixtures automatically.
Source: plugins/oracle; shared DSP/hosting: src; usage: docs/ORACLE-RACK.md.
No local VampDAW executable or user plugins are required to compile it.
