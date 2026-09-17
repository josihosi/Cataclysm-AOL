# C-AOL Creature Sprites

This mod supplies two independent sprite overrides through the engine's `mod_tileset` loader.  `UltimateCataclysm` is its main rendering target; `Larwick Overmap` is also declared because this profile loads it as its separate overmap tileset and the loader otherwise rejects every enabled mod tileset during that pass.  Neither base tileset is modified.

Enable **C-AOL Creature Sprites** when creating or editing a world, then select UltimateCataclysm.  The mod maps `mon_writhing_stalker` to atlas cell `(0, 0)` and `mon_zombie_rider` to `(1, 0)` in `creatures.png`; cells are 32×32 pixels.

`assets/` retains the supplied individual draft exports verbatim.  `creatures.png` is their tracked, left-to-right atlas used by the game.  Regenerate it from those exports with `python3 tools/gfx_tools/build_caol_creature_sprites.py`; the builder preserves decoded RGBA pixels exactly.  The owner wording "zombie stalker" is intentionally interpreted here as the supplied zombie rider asset and `mon_zombie_rider`.

Run `python3 tools/gfx_tools/validate_caol_creature_sprites.py` from the repository root to verify the package layout, IDs, atlas coordinates, source hashes, decoded RGBA transparency, and atlas pixels.
