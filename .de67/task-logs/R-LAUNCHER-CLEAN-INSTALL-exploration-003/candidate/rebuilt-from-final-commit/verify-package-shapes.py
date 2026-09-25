#!/usr/bin/env python3
from pathlib import Path
import hashlib, json, tarfile, zipfile
ROOT = Path(__file__).resolve().parent
MANIFEST = json.loads((ROOT / 'manifest.json').read_text())
SUMS = {line.split(maxsplit=1)[1].strip(): line.split(maxsplit=1)[0]
        for line in (ROOT / 'SHA256SUMS.txt').read_text().splitlines()}
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def export_record(name):
    return next(row for row in MANIFEST['package_exports'] if Path(row['artifact']['path']).name == name)
for name in ('Catapult-Dabubu-macos-unsigned.zip', 'Catapult-Dabubu-linux-unsigned.tar.gz', 'Catapult-Dabubu-windows-unsigned.zip'):
    path = ROOT / name
    assert digest(path) == SUMS[name] == export_record(name)['artifact']['sha256']
    print(f'{name}: {path.stat().st_size} bytes sha256={digest(path)}')
with zipfile.ZipFile(ROOT / 'Catapult-Dabubu-macos-unsigned.zip') as archive:
    names = archive.namelist()
    assert archive.testzip() is None
    pck_name = 'Catapult-Dabubu.app/Contents/Resources/Catapult-Dabubu.pck'
    assert len(names) == 17
    assert 'Catapult-Dabubu.app/Contents/Info.plist' in names
    assert 'Catapult-Dabubu.app/Contents/MacOS/Catapult-Dabubu' in names
    pck = archive.read(pck_name)
    assert hashlib.sha256(pck).hexdigest() == MANIFEST['independent_archive_validation']['macos']['embedded_pck_sha256']
    assert not any(name.endswith('/utils/7za') for name in names)
    print('macOS ZIP: CRC pass, 17 entries, app/Info.plist/executable and embedded PCK present; no external 7za sidecar')
with tarfile.open(ROOT / 'Catapult-Dabubu-linux-unsigned.tar.gz', 'r:gz') as archive:
    members = archive.getmembers()
    by_name = {item.name: item for item in members}
    assert len(members) == 5
    assert by_name['linux/Catapult-Dabubu.x86_64'].mode & 0o111
    assert by_name['linux/utils/7za'].mode & 0o111
    assert by_name['linux/utils/7-ZIP_LICENSE'].isfile()
    with archive.extractfile(by_name['linux/Catapult-Dabubu.x86_64']) as binary:
        assert binary.read(4) == b'\x7fELF'
    print('Linux tar.gz: 5 entries; executable ELF app + executable 7za and license present')
with zipfile.ZipFile(ROOT / 'Catapult-Dabubu-windows-unsigned.zip') as archive:
    names = archive.namelist()
    assert archive.testzip() is None
    assert len(names) == 3
    assert archive.read('Catapult-Dabubu.exe')[:2] == b'MZ'
    assert 'utils/7za.exe' in names and 'utils/7-ZIP_LICENSE' in names
    print('Windows ZIP: CRC pass, 3 entries; root MZ executable, 7za.exe and license present')
assert MANIFEST['source_binding']['candidate_commit'] == '495ac43ef62f36685a941840405a4a8e25a1f03f'
assert MANIFEST['independent_archive_validation']['checksums_match_manifest_and_sha256sums']
print('Source commit binding and all package SHA-256 checks passed')
