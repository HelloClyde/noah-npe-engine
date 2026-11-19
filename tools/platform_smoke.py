"""Validate distribution contents and a generated, non-game NPE header."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile
import zipfile

p = argparse.ArgumentParser()
p.add_argument('--binary', type=Path)
p.add_argument('--bundle', type=Path)
p.add_argument('--apk', type=Path)
a = p.parse_args()
if a.bundle:
    assert (a.bundle / "Contents/Resources/app.icns").is_file(), "app icon"
    for font in ('gbk12-native.bin', 'gbk16-native.bin'):
        assert (a.bundle / 'Contents/Resources/assets' / font).is_file(), font
if a.binary:
    # The normal parser validates 100 contiguous section descriptors. No game data.
    data = bytearray(0x35a + 2)
    data[:12] = b'NP560 RPG2.1'
    struct.pack_into('>II', data, 0x3a, 0x35a, 2)
    struct.pack_into('>H', data, 0x38, len(data))
    with tempfile.TemporaryDirectory() as folder:
        game = Path(folder) / 'synthetic.npe'; game.write_bytes(data)
        result = subprocess.run([str(a.binary.resolve()), '--inspect', str(game)], capture_output=True, text=True)
        if result.returncode:
            raise SystemExit(result.stderr + result.stdout)
        assert 'sections: 1' in result.stdout, result.stdout
if a.apk:
    with zipfile.ZipFile(a.apk) as archive:
        names = set(archive.namelist())
        assert any(n.startswith('res/mipmap-') and n.endswith('/ic_launcher.png') for n in names), 'launcher icon'
        for abi in ('arm64-v8a', 'x86_64'):
            for library in ('libmain.so', 'libSDL2.so'):
                assert f'lib/{abi}/{library}' in names, (abi, library)
        for font in ('gbk12-native.bin', 'gbk16-native.bin'):
            assert f'assets/assets/{font}' in names, font
print('Platform package smoke checks passed')
