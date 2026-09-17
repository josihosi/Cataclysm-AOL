#!/usr/bin/env python3
"""Build the C-AOL creature atlas without altering its decoded RGBA pixels."""

import struct
import subprocess
import zlib
from pathlib import Path


ROOT = Path( __file__ ).resolve().parents[2]
PACKAGE = ROOT / "data/mods/caol_creature_sprites_tileset"
SOURCES = [
    PACKAGE / "assets/mon_writhing_stalker.png",
    PACKAGE / "assets/mon_zombie_rider.png",
]


def rgba( path: Path ) -> bytes:
    result = subprocess.run( [ "magick", str( path ), "-depth", "8", "rgba:-" ], check=True,
                             stdout=subprocess.PIPE )
    if len( result.stdout ) != 32 * 32 * 4:
        raise RuntimeError( f"expected one 32x32 RGBA sprite: {path}" )
    return result.stdout


def chunk( kind: bytes, content: bytes ) -> bytes:
    return ( struct.pack( ">I", len( content ) ) + kind + content +
             struct.pack( ">I", zlib.crc32( kind + content ) & 0xffffffff ) )


def main() -> None:
    sprites = [ rgba( path ) for path in SOURCES ]
    rows = b"".join( b"\0" + sprites[0][row * 128:( row + 1 ) * 128] +
                       sprites[1][row * 128:( row + 1 ) * 128] for row in range( 32 ) )
    png = ( b"\x89PNG\r\n\x1a\n" + chunk( b"IHDR", struct.pack( ">IIBBBBB", 64, 32, 8, 6, 0, 0, 0 ) ) +
            chunk( b"IDAT", zlib.compress( rows, level=9 ) ) + chunk( b"IEND", b"" ) )
    ( PACKAGE / "creatures.png" ).write_bytes( png )


if __name__ == "__main__":
    main()
