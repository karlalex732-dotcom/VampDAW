"""Specialist wind model adapter; inference code comes from MSST (MIT).
Weights are downloaded from the pinned upstream repack, with SHA-256 checks.
"""
from pathlib import Path
import hashlib
import json
import os
import sys
import urllib.request
import torch
import yaml


def load_wind_model(target, cache, report):
    root = Path(__file__).resolve().parent
    catalog = json.loads((root / 'wind-models.json').read_text(encoding='utf-8'))
    if target not in catalog:
        raise ValueError('Unsupported wind instrument')
    entry = catalog[target]
    path = Path(cache) / 'wind' / (target + '.ckpt')
    def digest(filename):
        result = hashlib.sha256()
        with open(filename, 'rb') as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b''):
                result.update(block)
        return result.hexdigest()
    if not path.exists() or digest(path) != entry['sha256']:
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = path.with_suffix('.download')
        try:
            with urllib.request.urlopen(entry['weights_url'], timeout=60) as response, temporary.open('wb') as stream:
                count = 0
                while True:
                    chunk = response.read(1024 * 1024)
                    if not chunk:
                        break
                    stream.write(chunk)
                    count += len(chunk)
                    report(.03 + .025 * count / entry['bytes'], 'Downloading ' + target + ' model')
            if digest(temporary) != entry['sha256']:
                raise ValueError('Wind model download checksum mismatch. Try again.')
            os.replace(temporary, path)
        except Exception as error:
            raise RuntimeError('Cannot download wind model. Check the internet connection and try again: ' + str(error)) from error
    config_path = root / 'configs' / (target + '.yaml')
    if digest(config_path) != entry['config_sha256']:
        raise ValueError('Wind model configuration checksum mismatch')
    class ConfigLoader(yaml.SafeLoader):
        pass
    ConfigLoader.add_constructor('tag:yaml.org,2002:python/tuple', lambda loader, node: tuple(loader.construct_sequence(node)))
    config = yaml.load(config_path.read_text(), Loader=ConfigLoader)
    sys.path.insert(0, str(root / 'vendor'))
    from models.bs_roformer.bs_roformer import BSRoformer
    parameters = config['model']
    parameters['flash_attn'] = False
    core = BSRoformer(**parameters)
    checkpoint = torch.load(path, map_location='cpu', weights_only=True)
    if 'state_dict' in checkpoint:
        checkpoint = checkpoint['state_dict']
    core.load_state_dict(checkpoint, strict=True)
    core.eval()
    class WindModel(torch.nn.Module):
        def __init__(self):
            super().__init__()
            self.core = core
            self.sources = [target, 'other']
            self.samplerate = config['audio']['sample_rate']
            self.audio_channels = 2
            self.segment = 7.8
        def forward(self, mix):
            separated = self.core(mix)
            if separated.ndim == 4:
                separated = separated[:, 0]
            return torch.stack([separated, mix - separated], dim=1)
    return WindModel()
