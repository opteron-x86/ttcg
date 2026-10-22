"""Every shipped native Papyrus declaration must have a DLL registration."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
declared = set()
for path in (root / 'Scripts/Source').glob('*.psc'):
    source = path.read_text()
    script = re.search(r'^Scriptname\s+(\w+)', source, re.M | re.I)[1]
    for name in re.findall(r'\bFunction\s+(\w+)\([^\n]*\)[^\n]*\bNative\b', source, re.I):
        declared.add((script, name))
registered = {(script, name) for name, script in re.findall(
    r'vm->RegisterFunction\("(\w+)","(\w+)"', (root / 'src/Plugin.cpp').read_text() + (root / 'include/NativeExchange.h').read_text())}
assert declared == registered, (declared - registered, registered - declared)
print(f'Papyrus: all {len(declared)} native declarations match DLL registrations')
