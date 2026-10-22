# Pack asset authoring

The standard and tournament packs are original Blender meshes with procedurally authored paper, gilt, cloth, cord, and wax textures. The game uses two Skyrim SE NIFs and six DDS textures.

Rebuild from the repository root:

```bash
python3 tools/build-pack-assets.py --preview
python3 tools/build-plugin.py
python3 tools/install-mod.py --target /path/to/mod
```

Authoring requires Blender, Python with NumPy and Pillow, ImageMagick with DDS support, Git, CMake, Ninja, and a C++17 compiler. The command fetches and builds a pinned [nifly](https://github.com/ousnius/nifly) revision locally. It uses the existing nlohmann JSON headers or downloads the 3.12.0 standalone header. Tools stay under `build/tools` and are not shipped.

- `textures.py` creates the artwork and compressed DDS maps with complete mip chains. Normal-map alpha carries the specular mask.
- `model.py` builds the folded wrappers, binding, wax seals, and stamps, then exports evaluated geometry, split normals, and UVs. Degenerate bevel triangles are removed before NIF conversion.
- `nif-export.cpp` writes opaque, unskinned Skyrim SE `BSTriShape` geometry, tangent-space materials, an inventory marker, and a dynamic cloth-material box collider.
- `verify.py` checks reopened geometry against the Blender export, normal and UV precision, object bounds, collision metadata, asset paths, DDS compression, and every mip level.

The build also renders two transparent UI images from the reopened NIFs. `model.py -- ui` regenerates only these images under `TesseraUI/assets/packs/`. The album uses them for both the pinned pack tiles and the larger inspector preview. The inventory marker is independent of the camera used for these renders.

Runtime assets are in `meshes/Tessera/packs/` and `textures/Tessera/packs/`. Source `.blend` files, PNG texture sources, mesh interchange data, and verification reports stay in `build/pack-assets/`. Front and rear renders under `build/previews/` use geometry and textures read back from the exported NIFs. Their studio lighting is a preview, not a Skyrim screenshot.

`tools/pack_assets.py` owns both item model assignments and the exact runtime asset list. Plugin generation reapplies those assignments after cloning the tournament pack. The installer and release packager include these assets in the main mod download, never the card-art archive. Prices, weights, opening behavior, and pack contents are independent of the model changes.

After changing geometry, check inventory orientation and scale, dropped-object collision and pickup, both pack types, and lighting in Skyrim. Static validation does not exercise the game's physics or inventory camera.
