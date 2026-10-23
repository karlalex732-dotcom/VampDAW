# Blood Stems / Sample Lab

Open **Sample Lab → Blood Stems**. **Process → Blood Stems** opens the same
tab inside the laboratory. Editor returns to waveform editing without cancelling
an active job. Expand enlarges the laboratory. Separation and Fine tuning split
the settings into two pages that fit the panel without scrolling.

Choose the selected sample, a selected time range, or Browse audio. The original
audio is preserved. Each run writes to a new folder. Preview the resulting stems
and add them to Playlist as a group of tracks; automatic import and muting the
original are optional. A result is not automatically inserted into a different
project opened while separation is running. Undo removes an imported group.
The working input snapshot uses float32, preserving headroom above 0 dBFS without
clipping it to integer PCM before inference.

Models:

| Model | Outputs | Availability |
|---|---|---|
| Hybrid | vocals, drums, bass, other | Included with Stem Engine |
| Fine tuned | four stems, ensemble of four models | Download on first use, approximately 321 MB |
| Six stems | vocals, drums, bass, other, guitar, piano | Included; piano separation is experimental |
| Wind specialist | wind, brass, woodwind, saxophone, trumpet, flute, clarinet or trombone, plus remainder | A separate approximately 78 MB model is downloaded for each chosen target |

Wind targets overlap conceptually: wind and brass are alternatives for extraction,
not disjoint parts to sum together. For each chosen target the remainder is
provided separately. Separation can leave bleed or remove parts of the wanted
instrument, depending on the recording. This alpha has operational tests and a
real wind recording test, not an independent quality ranking.

Fast/Balanced/Detailed presets control shifts, overlap and chunk length. More
passes and overlap take longer; shorter chunks reduce memory use. CPU threads,
source, output rate, WAV 16/24/float or FLAC 24, mono, DC removal, gain and level
handling can be adjusted independently. Keep stems sum consistent puts the
reconstruction difference into Other/remainder, including sample-rate conversion.
Linked level protection applies one gain to all stems; individual normalization
changes their relative balance. FLAC results also have float WAV companions for
reliable Playlist import.

Inference runs locally in a separate process below normal priority. Cancel stops
only that worker. Closing the laboratory leaves the job running. The bundled
engine uses CPU; CUDA needs a separately prepared compatible runtime. Audio is
not uploaded. Installation includes Python and the CPU libraries; the user does
not install Python. Extra models need internet on their first use.

Sources: [Demucs](https://github.com/facebookresearch/demucs),
[MSST v1.0.21](https://github.com/ZFTurbo/Music-Source-Separation-Training/releases/tag/v1.0.21),
[single-target weights](https://huggingface.co/noblebarkrr/BS-Roformer-MVSep-Mega-53-stems).
The specialist adapter pins the upstream revision and verifies weights/configs
with SHA-256. MSST code is MIT licensed, with its original notice retained in
tools/stems/vendor. Specialist weights are downloaded from upstream and are not
redistributed inside this installer. Their source/license information remains
separate from the MIT inference-code license.

## Diagnostics (0.4.7)

The panel opens before a job exists. Absent results and malformed saved controls
use safe defaults. UI exceptions are contained in the Blood Stems callback;
worker errors are reported without importing partial results. Runtime crashes
include an exit code and a path to the job logs.

Installed startup/error journal: `%LOCALAPPDATA%\VampDAW\logs\blood-stems.log`.
Portable journal: `projects\logs\blood-stems.log` beside the application.
The per-job result folder holds `host.log`, `engine.log`, `job.json` and any
completed result. Logs can include local file paths; share them deliberately.
If a crash persists on another PC, provide the journal, CPU model and whether
it happened when opening the tab or pressing Separate.

The bundled PyTorch DLLs also depend on the system Microsoft Visual C++ x64
runtime (`MSVCP140.dll`). If it is missing, install the official
[Microsoft Visual C++ x64 Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).
The installer does not copy DLLs from the developer's Windows installation.


## Panel and processing device (0.4.8)

Blood Stems remains inside Sample Lab. **Separation** contains the source, model,
target, quality, format, sample rate and level policy. **Fine tuning** contains
shifts, overlap, segment length, CPU threads, device, gain and signal options.
The source, output folder, status, preview and actions stay available on both tabs.
The panel fits its width and height without scrolling. Long values have tooltips;
the wheel over a closed list does not accidentally change a parameter.

Device defaults to **Auto**. The bundled PyTorch is a **CPU build**. An NVIDIA
graphics card does not turn that build into a CUDA build. If **GPU / CUDA when
available** is selected but CUDA cannot be used, the worker completes on CPU,
records `requested_device`, `device_fallback`, `device_note` and `device` in
result.json, and the panel reports the fallback. The startup journal records the
reason. A compatible optional CUDA-enabled runtime is required for actual GPU
processing; no such runtime is bundled in this release. Physical GPU execution
and the affected external PC were not tested.
