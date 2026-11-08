"""Validate packaging contracts, optionally against built module ZIPs."""
import pathlib
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
