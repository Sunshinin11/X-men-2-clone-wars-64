"""Extract validated GEMS data for the native N64 audio asset pipeline.

This exports data only: no Z80 program or chip implementation is exported.
Run from the game checkout with python3 tools/n64_audio_assets.py.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

from common import BUILD, ROM_SHA1, ensure_rom

BANKS = (("patch", 0x1618BC, 0x163BF2),
         ("envelope", 0x163BF2, 0x1641BE),
         ("sequence", 0x1641BE, 0x17B8AA),
         ("sample", 0x17B8AA, 0x1FE66C))


def sequence_catalog(bank: bytes) -> list[dict]:
    """Validate all 256 GEMS 2.8 song headers and 24-bit track offsets."""
    if len(bank) < 512:
        raise ValueError("truncated sequence offset table")
    songs = []
    for song in range(256):
        offset = int.from_bytes(bank[song * 2:song * 2 + 2], "little")
        if not 512 <= offset < len(bank):
            raise ValueError(f"song {song:02x}: invalid header offset {offset:x}")
        count = bank[offset]
        if offset + 1 + count * 3 > len(bank):
            raise ValueError(f"song {song:02x}: truncated track table")
        tracks = []
        for track in range(count):
            pos = offset + 1 + track * 3
            address = int.from_bytes(bank[pos:pos + 3], "little")
            if not 512 <= address < len(bank):
                raise ValueError(f"song {song:02x} track {track}: invalid offset {address:x}")
            tracks.append(address)
        songs.append({"id": song, "offset": offset, "tracks": tracks})
    return songs


def extract(rom: bytes, destination: Path) -> dict:
    if hashlib.sha1(rom).hexdigest() != ROM_SHA1:
        raise ValueError("unsupported source cartridge")
    catalog = sequence_catalog(rom[0x1641BE:0x17B8AA])
    manifest = {"source_sha1": ROM_SHA1, "format": "GEMS 2.8", "banks": {}, "songs": catalog}
    destination.mkdir(parents=True, exist_ok=True)
    for name, start, end in BANKS:
        data = rom[start:end]
        (destination / (name + ".bin")).write_bytes(data)
        manifest["banks"][name] = {"source_offset": start, "bytes": len(data),
                                    "sha256": hashlib.sha256(data).hexdigest()}
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


if __name__ == "__main__":
    result = extract(ensure_rom().read_bytes(), BUILD / "n64" / "native_audio")
    print(f"GEMS data validated: {len(result['songs'])} songs, "
          f"{sum(len(s['tracks']) for s in result['songs'])} track references; no executable driver exported")
