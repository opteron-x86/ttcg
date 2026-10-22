#!/usr/bin/env python3
"""Package the optional collection SDK separately from the game downloads."""
import importlib.util
from runtime_files import ROOT, hashes

spec = importlib.util.spec_from_file_location('release_package', ROOT / 'tools/package.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)

files = {name: ROOT / name for name in (
    'docs/collection-api.md', 'docs/exchange-api.md', 'include/Tessera/CollectionAPI.h',
    'Scripts/Source/Tessera_Collection.psc', 'Scripts/Source/Tessera_Exchange.psc',
    'examples/collection-api/CollectionExample.cpp',
    'examples/collection-api/TesseraCollectionExample.psc')}
destination = ROOT / 'dist/authoring/Tessera-Collection-API.zip'
package.write_archive(files, hashes(files), destination)
print(f'Verified {destination} ({len(files)} files)')
