#!/usr/bin/env python3
"""Deterministically validate the C-AOL separate creature mod-tileset package."""

import hashlib
import json
import subprocess
import sys
from pathlib import Path


ROOT = Path( __file__ ).resolve().parents[2]
PACKAGE = ROOT / "data/mods/caol_creature_sprites_tileset"
EXPECTED_HASHES = {
    "mon_writhing_stalker.png": "414c7e2dfc4da99b0d4687c68c29bce19ed5a6e5f67a0d4c932d1c6f714483e1",
    "mon_zombie_rider.png": "1dec553fc1d87b4fcfccc200c1548239e371d15967bc1e4cc5824b473a3ca9a4",
}
EXPECTED_MAPPING = {
    "mon_writhing_stalker": 0,
    "mon_zombie_rider": 1,
}


def command( *args: str ) -> bytes:
    return subprocess.run( args, check=True, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE ).stdout


def image_rgba( path: Path ) -> tuple[int, int, bytes]:
    dimensions = command( "magick", "identify", "-format", "%w %h", str( path ) ).decode().split()
    if len( dimensions ) != 2:
        raise AssertionError( f"could not read dimensions for {path}" )
    width, height = map( int, dimensions )
    rgba = command( "magick", str( path ), "-depth", "8", "rgba:-" )
    if len( rgba ) != width * height * 4:
        raise AssertionError( f"{path} did not decode as 8-bit RGBA" )
    return width, height, rgba


def main() -> None:
    modinfo = json.loads( ( PACKAGE / "modinfo.json" ).read_text() )
    assert modinfo[0]["type"] == "MOD_INFO"
    assert modinfo[0]["id"] == "caol_creature_sprites_tileset"

    config = json.loads( ( PACKAGE / "mod_tileset.json" ).read_text() )
    entry = config[0]
    assert entry["type"] == "mod_tileset"
    assert entry["compatibility"] == [ "UltimateCataclysm", "Larwick Overmap" ]
    sheet = entry["tiles-new"][0]
    assert sheet["file"] == "creatures.png"
    assert ( sheet["sprite_width"], sheet["sprite_height"] ) == ( 32, 32 )
    mappings = { tile["id"]: tile["fg"] for tile in sheet["tiles"] }
    assert mappings == EXPECTED_MAPPING
    assert len( set( mappings.values() ) ) == len( mappings )

    source_pixels = []
    for filename, expected_hash in EXPECTED_HASHES.items():
        path = PACKAGE / "assets" / filename
        assert hashlib.sha256( path.read_bytes() ).hexdigest() == expected_hash
        width, height, rgba = image_rgba( path )
        assert ( width, height ) == ( 32, 32 )
        alpha = rgba[3::4]
        assert 0 in alpha and any( channel != 0 for channel in alpha )
        source_pixels.append( rgba )

    width, height, atlas = image_rgba( PACKAGE / sheet["file"] )
    assert ( width, height ) == ( 64, 32 )
    for row in range( 32 ):
        atlas_row = atlas[row * 64 * 4:( row + 1 ) * 64 * 4]
        assert atlas_row[:32 * 4] == source_pixels[0][row * 32 * 4:( row + 1 ) * 32 * 4]
        assert atlas_row[32 * 4:] == source_pixels[1][row * 32 * 4:( row + 1 ) * 32 * 4]
    print( "C-AOL creature sprite package: OK (2 IDs, 2 distinct 32x32 RGBA atlas cells)" )


if __name__ == "__main__":
    try:
        main()
    except ( AssertionError, KeyError, IndexError, json.JSONDecodeError, subprocess.CalledProcessError ) as error:
        print( f"C-AOL creature sprite package: FAILED: {error}", file=sys.stderr )
        raise SystemExit( 1 )
