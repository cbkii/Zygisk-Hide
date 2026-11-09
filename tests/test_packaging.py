"""Validate packaging contracts, optionally against built module ZIPs."""
import pathlib
import subprocess
import sys
import zipfile

root = pathlib.Path(__file__).resolve().parents[1]
expected = {'arm64-v8a.so', 'armeabi-v7a.so', 'x86.so', 'x86_64.so'}
for arg in sys.argv[1:]:
    with zipfile.ZipFile(arg) as archive:
        names = set(archive.namelist())
        assert {p.removeprefix('zygisk/') for p in names if p.startswith('zygisk/') and p.endswith('.so')} == expected
        assert not any(p.endswith('sepolicy.rule') for p in names)
        assert {'customize.sh', 'diagnose.sh', 'diagnose-maps.awk', 'diagnose-inline.awk', 'module.prop', 'webroot/index.html'} <= names
        assert b'id=zygisk-hide\n' in archive.read('module.prop')
        assert b'${moduleId}' not in archive.read('module.prop')
        for name in expected:
            assert archive.read('zygisk/' + name).startswith(b'\x7fELF')
        assert b'/data/adb/zygisk-hide/config.json' in archive.read('webroot/index.html')
assert not (root / 'module/template/sepolicy.rule').exists()
for name in ('post-fs-data.sh', 'service.sh'):
    text = (root / 'module/template' / name).read_text()
    assert text.index('legacy-props') < text.index('resetprop_if_diff')
    assert '/sys/fs/selinux/policy' not in text

inline_scope = root / 'module/template/diagnose-inline.awk'
targets = 'targets=example.app example.app:helper example.other'
legacy = subprocess.run(
    ['awk', '-v', 'version_code=1', '-v', targets, '-f', inline_scope],
    input='1:libart.so:pread\r\nexample.app:helper\r\n', text=True,
    capture_output=True, check=True,
).stdout.splitlines()
assert legacy == [
    'inline_config=enabled library=libart.so method=pread',
    'example.app listed=no configured_remap=no method=none',
    'example.app:helper listed=yes configured_remap=yes method=pread',
    'example.other listed=no configured_remap=no method=none',
]
modern_args = ['awk', '-v', 'version_code=5', '-v', targets, '-f', inline_scope]
config = (
    '1:libart.so:mmap-once\nexample.app\nexample.app:helper\n'
    '@mode example.app:helper mmap\n@mode example.other pread\n'
)
modern = subprocess.run(
    modern_args, input=config, text=True, capture_output=True, check=True,
).stdout.splitlines()
assert modern == [
    'inline_config=enabled library=libart.so method=mmap-once',
    'example.app listed=yes configured_remap=yes method=mmap-once',
    'example.app:helper listed=yes configured_remap=yes method=mmap',
    'example.other listed=no configured_remap=no method=none',
]
invalid = subprocess.run(
    modern_args, input=config + '@mode example.other mmap\n', text=True, capture_output=True,
)
assert invalid.returncode == 1 and invalid.stdout == 'inline_config=invalid\n'
