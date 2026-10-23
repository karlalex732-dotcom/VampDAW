"""VampDAW local stem worker. GPL-3.0-only; Demucs is MIT licensed.

The parent passes a UTF-8 JSON job, never a shell command. Audio stays local.
"""
from pathlib import Path
import argparse
import json
import math
import os
import random
import sys
import time
import traceback


def atomic_json(path, value):
    path = Path(path)
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, ensure_ascii=False), encoding='utf-8')
    for attempt in range(8):
        try:
            os.replace(temporary, path)
            return
        except PermissionError:
            time.sleep(.02 * (attempt + 1))
    os.replace(temporary, path)


def select_device(torch, requested):
    if requested not in ('cpu', 'auto', 'cuda'):
        raise ValueError('Invalid processing device')
    if requested == 'cpu':
        return 'cpu', False, ''
    try:
        available = bool(torch.cuda.is_available())
    except Exception:
        available = False
    if available:
        return 'cuda', False, ''
    cpu_build = getattr(torch.version, 'cuda', None) is None
    reason = ('The installed stem engine is a CPU build. NVIDIA hardware alone does not enable CUDA.'
              if cpu_build else 'CUDA is unavailable to this runtime. Check the NVIDIA driver and runtime compatibility.')
    # Auto choosing CPU is normal; an explicit GPU request needs an explanation.
    return 'cpu', requested == 'cuda', reason


def run(job):
    status = Path(job['status'])
    def report(progress, stage):
        atomic_json(status, {'progress': max(0, min(1, progress)), 'stage': stage})
    report(.01, 'Loading separation engine')
    os.environ['TORCH_HOME'] = job['cache']
    import numpy as np
    import soundfile as sf
    import torch
    from demucs.pretrained import get_model
    from demucs.audio import convert_audio
    from demucs import apply as inference
    model_name = job.get('model', 'htdemucs')
    if model_name not in ('htdemucs', 'htdemucs_ft', 'htdemucs_6s', 'bs_roformer_target'):
        raise ValueError('Unsupported separation model')
    torch.set_num_threads(max(1, min(16, int(job.get('threads', 4)))))
    seed = int(job.get('seed', 42))
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    requested = job.get('device', 'auto')
    device, device_fallback, device_note = select_device(torch, requested)
    if device_fallback:
        report(.02, 'CUDA unavailable; using CPU')
    report(.03, 'Loading model (first use may download model weights)')
    if model_name == 'bs_roformer_target':
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        from wind_model import load_wind_model
        model = load_wind_model(job.get('target', 'wind'), job['cache'], report)
    else:
        model = get_model(model_name)
    model.eval()
    report(.06, 'Reading audio')
    audio, original_rate = sf.read(job['input'], dtype='float32', always_2d=True)
    if not len(audio) or not np.isfinite(audio).all():
        raise ValueError('Input contains no audio or invalid samples')
    original_frames = len(audio)
    mix = convert_audio(torch.from_numpy(audio.T.copy()), original_rate, model.samplerate, model.audio_channels)
    reference = mix.mean(0)
    mean, std = reference.mean(), reference.std()
    sources = list(model.sources)
    shifts = max(0, min(8, int(job.get('shifts', 1))))
    overlap = max(.1, min(.75, float(job.get('overlap', .25))))
    segment = max(2., min(7.8, float(job.get('segment', 7.8))))
    if hasattr(model, 'max_allowed_segment'):
        segment = min(segment, model.max_allowed_segment)
    total_chunks = max(1, math.ceil((mix.shape[-1] + model.samplerate / 2) / ((1 - overlap) * segment * model.samplerate)))
    total_chunks *= max(1, shifts) * len(getattr(model, 'models', [model]))
    completed = 0
    # Demucs 4.0.1 exposes its split progress through tqdm, including ensemble
    # and shift passes. Report completed inference chunks rather than a timer.
    class Progress:
        def __init__(self, iterable, **kwargs):
            self.iterable = iterable
        def __iter__(self):
            nonlocal completed
            for item in self.iterable:
                yield item
                completed += 1
                report(.08 + .78 * min(1, completed / total_chunks), 'Separating audio')
    original_progress = inference.tqdm.tqdm
    inference.tqdm.tqdm = Progress
    report(.08, 'Separating audio')
    try:
        if std.item() < 1e-8:
            estimates = torch.zeros(len(sources), model.audio_channels, mix.shape[-1])
            estimates[-1] = mix
        else:
            with torch.inference_mode():
                model_input = mix if model_name == 'bs_roformer_target' else (mix - mean) / std
                estimates = inference.apply_model(model, model_input[None], shifts=shifts,
                    split=True, overlap=overlap, progress=True, device=device, num_workers=0, segment=segment)[0]
            estimates = estimates.cpu().clone()
            if model_name != 'bs_roformer_target':
                estimates = estimates * std + mean
        if job.get('consistent', True):
            residual = mix - estimates.sum(0)
            # Assign the reconstruction remainder to Other. Dedicated stems
            # keep their model estimates, and all stems sum to the input.
            estimates[sources.index('other') if 'other' in sources else -1] += residual
    finally:
        inference.tqdm.tqdm = original_progress
    mode = job.get('target', 'all')
    if mode != 'all':
        if mode not in sources:
            raise ValueError('This source is available only in the 6-stem model')
        index = sources.index(mode)
        selected = estimates[index]
        estimates = torch.stack([selected, mix - selected])
        sources = [mode, 'no_' + mode]
    report(.88, 'Preparing stems')
    output_rate = int(job.get('rate', 0)) or original_rate
    if output_rate not in (32000, 44100, 48000, 88200, 96000, 176400, 192000):
        output_rate = original_rate
    channels = 1 if job.get('mono', False) else audio.shape[1]
    channels = min(2, max(1, channels))
    results = []
    converted = []
    waves = []
    for estimate in estimates:
        wave = convert_audio(estimate, model.samplerate, output_rate, channels)
        frames = round(original_frames * output_rate / original_rate)
        if wave.shape[-1] < frames:
            wave = torch.nn.functional.pad(wave, (0, frames - wave.shape[-1]))
        wave = wave[:, :frames]
        waves.append(wave)
    # Correct the final sample-rate conversion too. A round trip through the
    # model's rate is not lossless; keep that difference in the remainder stem.
    if job.get('consistent', True) or mode != 'all':
        original = convert_audio(torch.from_numpy(audio.T.copy()), original_rate, output_rate, channels)
        if original.shape[-1] < frames:
            original = torch.nn.functional.pad(original, (0, frames - original.shape[-1]))
        residual = original[:, :frames] - torch.stack(waves).sum(0)
        waves[sources.index('other') if 'other' in sources else -1] += residual
    for wave in waves:
        if job.get('remove_dc', False):
            wave -= wave.mean(-1, keepdim=True)
        wave *= 10 ** (max(-24, min(12, float(job.get('gain_db', 0)))) / 20)
        converted.append(wave.numpy().T.copy())
    peak = max(float(np.max(np.abs(w))) for w in converted)
    normalization = job.get('normalization', 'linked')
    common_gain = min(1., .99 / max(peak, 1e-12)) if normalization == 'linked' else 1.
    directory = Path(job['output'])
    directory.mkdir(parents=True, exist_ok=True)
    encoding = job.get('encoding', 'PCM_24')
    if encoding not in ('PCM_16', 'PCM_24', 'FLOAT', 'FLAC'):
        raise ValueError('Invalid stem output encoding')
    for index, (name, wave) in enumerate(zip(sources, converted)):
        gain = .99 / max(float(np.max(np.abs(wave))), 1e-12) if normalization == 'individual' else common_gain
        wave *= gain
        if not np.isfinite(wave).all():
            raise ValueError('Model produced invalid audio')
        suffix = '.flac' if encoding == 'FLAC' else '.wav'
        target = directory / (name + suffix)
        temporary = target.with_suffix('.part' + suffix)
        sf.write(temporary, wave, output_rate, subtype='PCM_24' if encoding == 'FLAC' else encoding)
        os.replace(temporary, target)
        import_path = target
        if encoding == 'FLAC':
            import_path = directory / 'Playlist' / (name + '.wav')
            import_path.parent.mkdir(exist_ok=True)
            sf.write(import_path, wave, output_rate, subtype='FLOAT')
        results.append({'name': name, 'path': str(target.resolve()), 'rate': output_rate, 'frames': len(wave),
                        'import_path': str(import_path.resolve()), 'peak': float(np.max(np.abs(wave))), 'gain': gain})
        report(.9 + .09 * (index + 1) / len(sources), 'Writing stems')
    result = {'passed': True, 'model': model_name, 'device': device, 'sources': results,
              'requested_device': requested, 'device_fallback': device_fallback, 'device_note': device_note,
              'input_frames': original_frames, 'input_rate': original_rate, 'settings': job,
              'pre_normalization_peak': peak, 'linked_gain': common_gain}
    atomic_json(job['result'], result)
    report(1., 'Complete')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--job', required=True, type=Path)
    args = parser.parse_args()
    job = json.loads(args.job.read_text(encoding='utf-8'))
    try:
        run(job)
    except Exception as error:
        traceback.print_exc()
        atomic_json(job['result'], {'passed': False, 'error': str(error)})
        atomic_json(job['status'], {'progress': 0, 'stage': 'Failed', 'error': str(error)})
        raise SystemExit(1)


if __name__ == '__main__':
    main()
