#!/usr/bin/env python3
"""Repository contracts for OpenClaw scenario save fixtures."""

from __future__ import annotations

from contextlib import redirect_stdout
import ctypes
import ctypes.util
import io
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from typing import Any, Dict, Iterable, List, Set
from unittest import mock

from flatbuffers import flexbuffers

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))
ZZIP_HEADER_MAGIC = b"\x5f\x2a\x4d\x18"
ZZIP_FILENAME_MAGIC = b"\x50\x2a\x4d\x18"
ZZIP_CHECKSUM_MAGIC = b"\x51\x2a\x4d\x18"
ZSTD_FRAME_MAGIC = b"\x28\xb5\x2f\xfd"
ZZIP_CHECKSUM_SEED = 0x1337C0DE
ZZIP_DELETED_CHECKSUM = 0xDE1337ED
MAX_PLAYER_SAVE_SIZE = 512 * 1024 * 1024
RELEASE_GATE_SCENARIOS = (
    "basecamp.organic_board_speech_probe_mcw",
    "writhing_stalker.live_high_threat_allied_light_retreat_stalk_mcw",
    "zombie_rider.live_camp_light_band_mcw",
    "zombie_rider.live_no_camp_light_control_mcw",
    "locker.weather_wait",
)

from startup_harness import (  # noqa: E402
    StartupPlan,
    load_profile_config,
    load_scenario,
    resolve_fixture_payload,
    resolve_profile_name,
    resolve_startup_config_profile,
    run_launch_only_handoff,
    run_probe_mode,
    scenarios_root,
)


class SaveValidationError(RuntimeError):
    """A stable, user-facing reason a player save cannot be trusted."""


class ScenarioStartupProfileContractTest(unittest.TestCase):
    def test_isolated_userdir_keeps_scenario_startup_policy(self) -> None:
        scenario = {"profile": "dev-harness"}
        target_profile = "mac-verification-isolated"

        config_profile = resolve_startup_config_profile(scenario, target_profile)
        scenario_config = load_profile_config(config_profile)
        direct_master_config = load_profile_config("master")

        self.assertEqual(config_profile, "dev-harness")
        self.assertEqual(scenario_config["startup"]["post_lastworld_continue_keys"], [])
        self.assertEqual(direct_master_config["startup"]["post_lastworld_continue_keys"], ["return"])

    def test_probe_passes_scenario_config_profile_to_isolated_start(self) -> None:
        args = SimpleNamespace(
            scenario="test.isolated_profile",
            profile="mac-verification-isolated",
            world="",
            fixture=None,
            replace_existing_worlds=False,
            advance_turns=None,
            settle_seconds=None,
            artifact_pattern="",
            test_command="",
            dry_run=True,
        )
        scenario = {
            "name": "test.isolated_profile",
            "profile": "dev-harness",
            "steps": [],
        }
        stdout = io.StringIO()

        with (
            mock.patch("startup_harness.load_scenario", return_value=scenario),
            mock.patch("startup_harness.run_json_command", return_value=(0, {}, "", "")) as run_command,
            redirect_stdout(stdout),
        ):
            self.assertEqual(run_probe_mode(args), 0)

        start_command = run_command.call_args.args[0]
        self.assertEqual(
            start_command[start_command.index("--profile") + 1],
            "mac-verification-isolated",
        )
        self.assertEqual(
            start_command[start_command.index("--config-profile") + 1],
            "dev-harness",
        )

    def test_launch_only_dry_run_records_both_profile_identities(self) -> None:
        args = SimpleNamespace(
            scenario="test.launch_only",
            dry_run=True,
            compact_stdout=False,
        )
        plan = StartupPlan(
            profile="mac-verification-isolated",
            userdir=".userdata/mac-verification-isolated",
            executable="cataclysm-tiles",
            strategy="load_world",
            reason="test",
            target_world="McWilliams",
            existing_worlds=[],
            fixture="fixture",
            run_dir=".userdata/mac-verification-isolated/harness_runs/test",
        )
        stdout = io.StringIO()

        with (
            mock.patch("startup_harness.zzip_binary", return_value=Path("zzip")),
            mock.patch("startup_harness.build_plan", return_value=plan),
            redirect_stdout(stdout),
        ):
            rc = run_launch_only_handoff(
                args,
                scenario={"name": "test.launch_only"},
                profile="mac-verification-isolated",
                config_profile="dev-harness",
                world="McWilliams",
                fixture="fixture",
                fixture_profile="live-debug",
                profile_snapshot="snapshot",
                profile_snapshot_profile="live-debug",
                replace_existing_worlds=True,
                advance_count=0,
                settle_seconds=0.0,
                artifact_source="debug.log",
                artifact_patterns=[],
                recommended_test_command="",
                steps=[],
                capture_world_after=False,
                portal_storm_policy={},
            )

        payload = json.loads(stdout.getvalue())
        self.assertEqual(rc, 0)
        self.assertEqual(payload["resolved_contract"]["profile"], "mac-verification-isolated")
        self.assertEqual(payload["resolved_contract"]["config_profile"], "dev-harness")


def _rotate_left_64(value: int, count: int) -> int:
    mask = (1 << 64) - 1
    return ((value << count) | (value >> (64 - count))) & mask


def xxh64(data: bytes, seed: int = 0) -> int:
    """Return the XXH64 used by ``src/zzip.cpp``, without a Python dependency."""

    mask = (1 << 64) - 1
    prime_1 = 11400714785074694791
    prime_2 = 14029467366897019727
    prime_3 = 1609587929392839161
    prime_4 = 9650029242287828579
    prime_5 = 2870177450012600261

    def round_value(accumulator: int, lane: int) -> int:
        accumulator = (accumulator + lane * prime_2) & mask
        accumulator = _rotate_left_64(accumulator, 31)
        return (accumulator * prime_1) & mask

    length = len(data)
    offset = 0
    if length >= 32:
        accumulators = [
            (seed + prime_1 + prime_2) & mask,
            (seed + prime_2) & mask,
            seed & mask,
            (seed - prime_1) & mask,
        ]
        while offset <= length - 32:
            for lane_index in range(4):
                lane = struct.unpack_from("<Q", data, offset + lane_index * 8)[0]
                accumulators[lane_index] = round_value(accumulators[lane_index], lane)
            offset += 32
        result = (
            _rotate_left_64(accumulators[0], 1)
            + _rotate_left_64(accumulators[1], 7)
            + _rotate_left_64(accumulators[2], 12)
            + _rotate_left_64(accumulators[3], 18)
        ) & mask
        for accumulator in accumulators:
            result ^= round_value(0, accumulator)
            result = (result * prime_1 + prime_4) & mask
    else:
        result = (seed + prime_5) & mask

    result = (result + length) & mask
    while offset <= length - 8:
        lane = struct.unpack_from("<Q", data, offset)[0]
        result ^= round_value(0, lane)
        result = (_rotate_left_64(result, 27) * prime_1 + prime_4) & mask
        offset += 8
    if offset <= length - 4:
        result ^= (struct.unpack_from("<I", data, offset)[0] * prime_1) & mask
        result &= mask
        result = (_rotate_left_64(result, 23) * prime_2 + prime_3) & mask
        offset += 4
    while offset < length:
        result ^= (data[offset] * prime_5) & mask
        result &= mask
        result = (_rotate_left_64(result, 11) * prime_1) & mask
        offset += 1

    result ^= result >> 33
    result = (result * prime_2) & mask
    result ^= result >> 29
    result = (result * prime_3) & mask
    result ^= result >> 32
    return result & mask


def _zstd_library_candidates() -> Iterable[str]:
    discovered = ctypes.util.find_library("zstd")
    if discovered:
        yield discovered

    zstd_cli = shutil.which("zstd")
    if zstd_cli:
        binary_dir = Path(zstd_cli).resolve().parent
        for name in ("zstd.dll", "libzstd.dll", "libzstd.dylib", "libzstd.so.1"):
            candidate = binary_dir / name
            if candidate.is_file():
                yield str(candidate)

    if os.name == "nt":
        yield "zstd.dll"
        yield "libzstd.dll"
    elif sys.platform == "darwin":
        yield "/opt/homebrew/lib/libzstd.dylib"
        yield "/usr/local/lib/libzstd.dylib"
        yield "libzstd.dylib"
    else:
        yield "libzstd.so.1"
        yield "libzstd.so"


class ZstdDecoder:
    """Small ctypes adapter over the production zstd frame APIs."""

    CONTENT_SIZE_UNKNOWN = (1 << 64) - 1
    CONTENT_SIZE_ERROR = (1 << 64) - 2

    def __init__(self) -> None:
        errors: List[str] = []
        library = None
        seen: Set[str] = set()
        for candidate in _zstd_library_candidates():
            if candidate in seen:
                continue
            seen.add(candidate)
            try:
                library = ctypes.CDLL(candidate)
                break
            except OSError as exc:
                errors.append(f"{candidate}: {exc}")
        if library is None:
            detail = "; ".join(errors) if errors else "no library candidates found"
            raise SaveValidationError(f"zstd runtime unavailable ({detail})")

        self.library = library
        self.library.ZSTD_findFrameCompressedSize.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
        self.library.ZSTD_findFrameCompressedSize.restype = ctypes.c_size_t
        self.library.ZSTD_getFrameContentSize.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
        self.library.ZSTD_getFrameContentSize.restype = ctypes.c_ulonglong
        self.library.ZSTD_decompress.argtypes = [
            ctypes.c_void_p,
            ctypes.c_size_t,
            ctypes.c_void_p,
            ctypes.c_size_t,
        ]
        self.library.ZSTD_decompress.restype = ctypes.c_size_t
        self.library.ZSTD_isError.argtypes = [ctypes.c_size_t]
        self.library.ZSTD_isError.restype = ctypes.c_uint
        self.library.ZSTD_getErrorName.argtypes = [ctypes.c_size_t]
        self.library.ZSTD_getErrorName.restype = ctypes.c_char_p

    def _checked_size(self, result: int, context: str) -> int:
        if self.library.ZSTD_isError(result):
            error_name = self.library.ZSTD_getErrorName(result).decode("utf-8", errors="replace")
            raise SaveValidationError(f"{context}: {error_name}")
        return result

    def frame_size(self, source: bytes) -> int:
        if not source:
            raise SaveValidationError("missing .sav.zzip compressed entry")
        source_buffer = ctypes.create_string_buffer(source)
        result = self.library.ZSTD_findFrameCompressedSize(source_buffer, len(source))
        return self._checked_size(result, "invalid or truncated .sav.zzip compressed entry")

    def decompress(self, frame: bytes) -> bytes:
        source_buffer = ctypes.create_string_buffer(frame)
        content_size = self.library.ZSTD_getFrameContentSize(source_buffer, len(frame))
        if content_size == self.CONTENT_SIZE_ERROR:
            raise SaveValidationError("invalid .sav.zzip compressed entry header")
        if content_size == self.CONTENT_SIZE_UNKNOWN:
            raise SaveValidationError(".sav.zzip compressed entry omits its decoded size")
        if content_size > MAX_PLAYER_SAVE_SIZE:
            raise SaveValidationError(
                f".sav.zzip player payload exceeds {MAX_PLAYER_SAVE_SIZE} bytes"
            )
        destination = ctypes.create_string_buffer(max(1, content_size))
        result = self.library.ZSTD_decompress(
            destination,
            content_size,
            source_buffer,
            len(frame),
        )
        actual_size = self._checked_size(result, "cannot decompress .sav.zzip entry")
        if actual_size != content_size:
            raise SaveValidationError(
                f".sav.zzip entry decoded to {actual_size} bytes, expected {content_size}"
            )
        return destination.raw[:actual_size]


_ZSTD_DECODER: ZstdDecoder | None = None


def zstd_decoder() -> ZstdDecoder:
    global _ZSTD_DECODER
    if _ZSTD_DECODER is None:
        _ZSTD_DECODER = ZstdDecoder()
    return _ZSTD_DECODER


def repository_git_dir(repo: Path) -> Path:
    dot_git = repo / ".git"
    if dot_git.is_dir():
        return dot_git
    pointer = dot_git.read_text(encoding="utf-8").strip()
    prefix = "gitdir: "
    if not pointer.startswith(prefix):
        raise RuntimeError(f"Unrecognized linked-worktree pointer: {dot_git}")
    raw_path = pointer[len(prefix):]
    windows_absolute = re.fullmatch(r"([A-Za-z]):[\\/](.*)", raw_path)
    if windows_absolute and sys.platform != "win32":
        drive, remainder = windows_absolute.groups()
        return Path("/mnt") / drive.lower() / Path(remainder.replace("\\", "/"))
    git_dir = Path(raw_path)
    return git_dir if git_dir.is_absolute() else (repo / git_dir).resolve()


def tracked_paths(repo: Path) -> Set[str]:
    result = subprocess.run(
        [
            "git",
            "--git-dir",
            str(repository_git_dir(repo)),
            "--work-tree",
            str(repo),
            "ls-files",
            "-z",
            "--",
            "tools/openclaw_harness/fixtures",
        ],
        check=True,
        capture_output=True,
    )
    return {
        entry.decode("utf-8").replace("\\", "/")
        for entry in result.stdout.split(b"\0")
        if entry
    }


def player_save_paths(world_dir: Path) -> List[Path]:
    if not world_dir.is_dir():
        return []
    return sorted(
        path
        for path in world_dir.iterdir()
        if path.is_file() and (path.name.endswith(".sav") or path.name.endswith(".sav.zzip"))
    )


def _read_skippable_frame(
    data: bytes,
    offset: int,
    limit: int,
    expected_magic: bytes,
    description: str,
) -> tuple[bytes, int]:
    if offset < 0 or limit > len(data) or offset + 8 > limit:
        raise SaveValidationError(f"truncated {description}")
    if data[offset:offset + 4] != expected_magic:
        raise SaveValidationError(f"missing {description}")
    payload_size = struct.unpack_from("<I", data, offset + 4)[0]
    payload_start = offset + 8
    payload_end = payload_start + payload_size
    if payload_end > limit:
        raise SaveValidationError(f"truncated {description}")
    return data[payload_start:payload_end], payload_end


def _validate_player_json(payload: bytes) -> None:
    try:
        text = payload.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise SaveValidationError(f"player save is not UTF-8: {exc}") from exc
    try:
        save = json.loads(text)
    except json.JSONDecodeError as exc:
        raise SaveValidationError(
            f"player save is not valid JSON at line {exc.lineno} column {exc.colno}"
        ) from exc
    if not isinstance(save, dict):
        raise SaveValidationError("player save JSON root is not an object")
    loading_version = save.get("savegame_loading_version")
    if isinstance(loading_version, bool) or not isinstance(loading_version, int):
        raise SaveValidationError("player save has no integer savegame_loading_version")
    player = save.get("player")
    if not isinstance(player, dict):
        raise SaveValidationError("player save has no player object")
    if not isinstance(player.get("name"), str) or not player["name"].strip():
        raise SaveValidationError("player save has no player name")
    location = player.get("location")
    if (
        not isinstance(location, list)
        or len(location) != 3
        or any(
            isinstance(coordinate, bool) or not isinstance(coordinate, int)
            for coordinate in location
        )
    ):
        raise SaveValidationError("player save has no integer player location tripoint")


def _footer_nonnegative_integer(value: Any, description: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise SaveValidationError(f".sav.zzip footer {description} is not a nonnegative integer")
    return value


def _parse_zzip_footer(footer: bytes) -> tuple[Dict[str, Dict[str, int]], int, int]:
    try:
        root = flexbuffers.Loads(footer)
    except Exception as exc:
        raise SaveValidationError(f"invalid .sav.zzip FlexBuffers footer: {exc}") from exc
    if not isinstance(root, dict):
        raise SaveValidationError(".sav.zzip FlexBuffers footer root is not a map")

    entries_value = root.get("entries")
    if not isinstance(entries_value, dict):
        raise SaveValidationError(".sav.zzip footer has no entries map")
    entries: Dict[str, Dict[str, int]] = {}
    for filename, entry_value in entries_value.items():
        if not isinstance(filename, str) or not filename:
            raise SaveValidationError(".sav.zzip footer contains an invalid entry filename")
        if not isinstance(entry_value, dict):
            raise SaveValidationError(f".sav.zzip footer entry {filename} is not a map")
        entry_offset = _footer_nonnegative_integer(
            entry_value.get("offset"), f"entry {filename} offset"
        )
        entry_length = _footer_nonnegative_integer(
            entry_value.get("len"), f"entry {filename} length"
        )
        if entry_length == 0:
            raise SaveValidationError(f".sav.zzip footer entry {filename} has zero length")
        entries[filename] = {"offset": entry_offset, "len": entry_length}

    meta_value = root.get("meta")
    if not isinstance(meta_value, dict):
        raise SaveValidationError(".sav.zzip footer has no meta map")
    content_end = _footer_nonnegative_integer(
        meta_value.get("content_end"), "meta content_end"
    )
    total_content_size = _footer_nonnegative_integer(
        meta_value.get("total_content_size"), "meta total_content_size"
    )
    return entries, content_end, total_content_size


def _validate_zzip_player_save(path: Path, data: bytes) -> None:
    """Mirror the integrity chain in ``src/zzip.cpp`` and decode the live save entry."""

    header_payload, offset = _read_skippable_frame(
        data,
        0,
        len(data),
        ZZIP_HEADER_MAGIC,
        ".sav.zzip header",
    )
    if len(header_payload) != 16:
        raise SaveValidationError("invalid .sav.zzip header payload size")
    footer_size, footer_checksum = struct.unpack("<QQ", header_payload)
    if footer_size == 0 or footer_size > len(data) - offset:
        raise SaveValidationError("invalid .sav.zzip footer length")
    footer_start = len(data) - footer_size
    footer = data[footer_start:]
    if xxh64(footer, ZZIP_CHECKSUM_SEED) != footer_checksum:
        raise SaveValidationError("corrupt .sav.zzip footer checksum")
    footer_entries, footer_content_end, footer_total_content_size = _parse_zzip_footer(footer)

    expected_filename = path.name[:-len(".zzip")]
    header_end = offset
    scanned_entries: Dict[tuple[int, int], Dict[str, Any]] = {}
    live_scanned_entries: Dict[str, Dict[str, Any]] = {}
    while offset < footer_start:
        if data[offset] == 0:
            if any(data[offset:footer_start]):
                raise SaveValidationError("nonzero data between .sav.zzip entries and footer")
            break

        entry_start = offset
        filename_bytes, offset = _read_skippable_frame(
            data,
            offset,
            footer_start,
            ZZIP_FILENAME_MAGIC,
            ".sav.zzip entry filename metadata",
        )
        if not filename_bytes:
            raise SaveValidationError("empty .sav.zzip entry filename metadata")
        try:
            filename = filename_bytes.decode("utf-8")
        except UnicodeDecodeError as exc:
            raise SaveValidationError(f"invalid UTF-8 .sav.zzip entry filename: {exc}") from exc

        checksum_bytes, offset = _read_skippable_frame(
            data,
            offset,
            footer_start,
            ZZIP_CHECKSUM_MAGIC,
            ".sav.zzip entry checksum metadata",
        )
        if len(checksum_bytes) != 8:
            raise SaveValidationError("invalid .sav.zzip entry checksum metadata")
        stored_checksum = struct.unpack("<Q", checksum_bytes)[0]
        if stored_checksum == 0:
            raise SaveValidationError("zero .sav.zzip entry checksum")

        frame_size = zstd_decoder().frame_size(data[offset:footer_start])
        frame_end = offset + frame_size
        if frame_end > footer_start:
            raise SaveValidationError("truncated .sav.zzip compressed entry")
        frame = data[offset:frame_end]
        if xxh64(frame, ZZIP_CHECKSUM_SEED) != stored_checksum:
            if stored_checksum != ZZIP_DELETED_CHECKSUM:
                raise SaveValidationError(f"corrupt .sav.zzip compressed checksum for {filename}")
        decoded = zstd_decoder().decompress(frame)
        if stored_checksum == ZZIP_DELETED_CHECKSUM:
            if decoded:
                raise SaveValidationError(f"nonempty .sav.zzip deletion entry for {filename}")
            deleted = True
        else:
            deleted = False
        encoded_length = frame_end - entry_start
        scanned_entry = {
            "filename": filename,
            "offset": entry_start,
            "len": encoded_length,
            "decoded": decoded,
            "deleted": deleted,
        }
        scanned_entries[(entry_start, encoded_length)] = scanned_entry
        if deleted:
            live_scanned_entries.pop(filename, None)
        else:
            live_scanned_entries[filename] = scanned_entry
        offset = frame_end

    scanned_content_end = offset
    if footer_content_end != scanned_content_end:
        raise SaveValidationError(
            ".sav.zzip footer meta content_end "
            f"{footer_content_end} does not match scanned content end {scanned_content_end}"
        )
    if footer_content_end < header_end or footer_content_end > footer_start:
        raise SaveValidationError(".sav.zzip footer meta content_end is outside content bounds")

    indexed_entries: Dict[str, Dict[str, Any]] = {}
    indexed_total_content_size = 0
    for filename, entry in footer_entries.items():
        entry_offset = entry["offset"]
        entry_length = entry["len"]
        entry_end = entry_offset + entry_length
        if entry_offset < header_end or entry_end > footer_content_end:
            raise SaveValidationError(
                f".sav.zzip footer entry {filename} is outside content bounds"
            )
        scanned_entry = scanned_entries.get((entry_offset, entry_length))
        if scanned_entry is None or scanned_entry["filename"] != filename:
            raise SaveValidationError(
                f".sav.zzip footer entry {filename} does not match the scanned encoded body"
            )
        if scanned_entry["deleted"]:
            raise SaveValidationError(f".sav.zzip footer indexes deleted entry {filename}")
        indexed_entries[filename] = scanned_entry
        indexed_total_content_size += entry_length

    if indexed_total_content_size != footer_total_content_size:
        raise SaveValidationError(
            ".sav.zzip footer meta total_content_size "
            f"{footer_total_content_size} does not match indexed total {indexed_total_content_size}"
        )
    if set(indexed_entries) != set(live_scanned_entries):
        raise SaveValidationError(
            ".sav.zzip footer live entry set does not match the scanned encoded body"
        )

    player_entry = indexed_entries.get(expected_filename)
    if player_entry is None:
        raise SaveValidationError(
            f".sav.zzip footer has no live player entry named {expected_filename}"
        )
    _validate_player_json(player_entry["decoded"])


def player_save_error(path: Path) -> str:
    if path.stat().st_size <= 0:
        return "empty file"
    try:
        data = path.read_bytes()
        if path.name.endswith(".sav.zzip"):
            _validate_zzip_player_save(path, data)
        else:
            _validate_player_json(data)
    except SaveValidationError as exc:
        return str(exc)
    return ""


class ScenarioFixtureContractTest(unittest.TestCase):
    @staticmethod
    def sample_zzip_save() -> Path:
        candidates = sorted(
            (HARNESS_DIR / "fixtures" / "saves").rglob("*.sav.zzip"),
            key=lambda path: path.as_posix(),
        )
        if not candidates:
            raise AssertionError("No repository .sav.zzip fixture is available for decoder tests")
        return candidates[0]

    @staticmethod
    def write_resolved_fixture_chain(
        temp_dir: str,
        source_transforms: List[Dict[str, Any]],
        derived_transforms: List[Dict[str, Any]],
    ) -> None:
        fixture_root = Path(temp_dir) / "live-debug"
        source_fixture = fixture_root / "source"
        (source_fixture / "save" / "World").mkdir(parents=True)
        (source_fixture / "manifest.json").write_text(
            json.dumps({"save_transforms": source_transforms}),
            encoding="utf-8",
        )
        derived_fixture = fixture_root / "derived"
        derived_fixture.mkdir()
        (derived_fixture / "manifest.json").write_text(
            json.dumps({
                "source_fixture": "source",
                "source_profile": "live-debug",
                "save_transforms": derived_transforms,
            }),
            encoding="utf-8",
        )

    def test_xxh64_matches_reference_vector(self) -> None:
        self.assertEqual(xxh64(b""), 0xEF46DB3751D8E999)

    def test_truncated_zzip_header_is_not_a_valid_player_save(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            save_path = Path(temp_dir) / "player.sav.zzip"
            save_path.write_bytes(ZZIP_HEADER_MAGIC)

            error = player_save_error(save_path)

        self.assertEqual(error, "truncated .sav.zzip header")

    def test_repository_zzip_is_fully_decodable_player_json(self) -> None:
        self.assertEqual(player_save_error(self.sample_zzip_save()), "")

    def test_resolved_fixture_rejects_remove_then_clone_across_manifest_chain(self) -> None:
        for clone_follower_template in (False, True):
            with self.subTest(clone_follower_template=clone_follower_template):
                with tempfile.TemporaryDirectory() as temp_dir:
                    self.write_resolved_fixture_chain(
                        temp_dir,
                        [{
                            "kind": "remove_overmap_npcs",
                            "player_save": "player.sav.zzip",
                            "scan_all_overmaps": True,
                        }],
                        [{
                            "kind": "overmap_npcs_near_player",
                            "player_save": "player.sav.zzip",
                            "offsets_ms": [[1, 0, 0]],
                            "clone_follower_template": clone_follower_template,
                        }],
                    )

                    with mock.patch(
                        "startup_harness.profile_fixture_root",
                        side_effect=lambda profile: Path(temp_dir) / profile,
                    ):
                        with self.assertRaisesRegex(
                            SystemExit,
                            r"live-debug:derived -> live-debug:source.*remove_overmap_npcs.*overmap_npcs_near_player",
                        ):
                            resolve_fixture_payload("derived", "live-debug")

    def test_resolved_fixture_allows_clone_when_source_npcs_were_not_removed(self) -> None:
        for clone_follower_template in (False, True):
            with self.subTest(clone_follower_template=clone_follower_template):
                with tempfile.TemporaryDirectory() as temp_dir:
                    self.write_resolved_fixture_chain(
                        temp_dir,
                        [],
                        [{
                            "kind": "overmap_npcs_near_player",
                            "player_save": "player.sav.zzip",
                            "offsets_ms": [[1, 0, 0]],
                            "clone_follower_template": clone_follower_template,
                        }],
                    )

                    with mock.patch(
                        "startup_harness.profile_fixture_root",
                        side_effect=lambda profile: Path(temp_dir) / profile,
                    ):
                        resolved = resolve_fixture_payload("derived", "live-debug")

                    self.assertEqual(
                        resolved["save_transforms"][-1]["clone_follower_template"],
                        clone_follower_template,
                    )

    def test_resolved_fixture_allows_clone_after_partial_remove_and_relocation(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            self.write_resolved_fixture_chain(
                temp_dir,
                [{
                    "kind": "remove_overmap_npcs",
                    "player_save": "player.sav.zzip",
                    "scan_all_overmaps": False,
                }],
                [
                    {
                        "kind": "player_location_offset_ms",
                        "player_save": "player.sav.zzip",
                        "offset_ms": [4320, 0, 0],
                    },
                    {
                        "kind": "overmap_npcs_near_player",
                        "player_save": "player.sav.zzip",
                        "offsets_ms": [[1, 0, 0]],
                        "clone_follower_template": True,
                        "scan_all_overmaps_for_ids": True,
                    },
                ],
            )

            with mock.patch(
                "startup_harness.profile_fixture_root",
                side_effect=lambda profile: Path(temp_dir) / profile,
            ):
                resolved = resolve_fixture_payload("derived", "live-debug")

        self.assertEqual(
            [transform["kind"] for transform in resolved["save_transforms"]],
            [
                "remove_overmap_npcs",
                "player_location_offset_ms",
                "overmap_npcs_near_player",
            ],
        )

    def test_resolved_fixture_rejects_clone_after_unproven_relocation(self) -> None:
        relocations = [
            {
                "kind": "player_location_offset_ms",
                "player_save": "player.sav.zzip",
                "offset_ms": [0, 0, 0],
            },
            {
                "kind": "player_location_offset_ms",
                "player_save": "player.sav.zzip",
                "offset_ms": [4319, 0, 0],
            },
            {
                "kind": "player_near_overmap_special",
                "player_save": "player.sav.zzip",
                "special_id": "evac_center_18",
                "site_index": 1,
                "offset_omt": [0, 0, 0],
            },
        ]
        for relocation in relocations:
            with self.subTest(relocation=relocation):
                with tempfile.TemporaryDirectory() as temp_dir:
                    self.write_resolved_fixture_chain(
                        temp_dir,
                        [{
                            "kind": "remove_overmap_npcs",
                            "player_save": "player.sav.zzip",
                            "scan_all_overmaps": False,
                        }],
                        [
                            relocation,
                            {
                                "kind": "overmap_npcs_near_player",
                                "player_save": "player.sav.zzip",
                                "offsets_ms": [[1, 0, 0]],
                                "clone_follower_template": True,
                            },
                        ],
                    )

                    with mock.patch(
                        "startup_harness.profile_fixture_root",
                        side_effect=lambda profile: Path(temp_dir) / profile,
                    ):
                        with self.assertRaisesRegex(
                            SystemExit,
                            r"remove_overmap_npcs.*overmap_npcs_near_player",
                        ):
                            resolve_fixture_payload("derived", "live-debug")

    def test_resolved_fixture_allows_other_player_template_after_partial_remove(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            self.write_resolved_fixture_chain(
                temp_dir,
                [{
                    "kind": "remove_overmap_npcs",
                    "player_save": "first-player.sav.zzip",
                    "scan_all_overmaps": False,
                }],
                [{
                    "kind": "overmap_npcs_near_player",
                    "player_save": "second-player.sav.zzip",
                    "offsets_ms": [[1, 0, 0]],
                    "clone_follower_template": True,
                    "scan_all_overmaps_for_ids": True,
                }],
            )

            with mock.patch(
                "startup_harness.profile_fixture_root",
                side_effect=lambda profile: Path(temp_dir) / profile,
            ):
                resolved = resolve_fixture_payload("derived", "live-debug")

        self.assertEqual(
            resolved["save_transforms"][-1]["player_save"],
            "second-player.sav.zzip",
        )

    def test_global_id_scan_does_not_restore_template_removed_from_target_overmap(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            self.write_resolved_fixture_chain(
                temp_dir,
                [{
                    "kind": "remove_overmap_npcs",
                    "player_save": "player.sav.zzip",
                    "scan_all_overmaps": False,
                }],
                [{
                    "kind": "overmap_npcs_near_player",
                    "player_save": "player.sav.zzip",
                    "offsets_ms": [[1, 0, 0]],
                    "clone_follower_template": True,
                    "scan_all_overmaps_for_ids": True,
                }],
            )

            with mock.patch(
                "startup_harness.profile_fixture_root",
                side_effect=lambda profile: Path(temp_dir) / profile,
            ):
                with self.assertRaisesRegex(SystemExit, r"remove_overmap_npcs"):
                    resolve_fixture_payload("derived", "live-debug")

    def test_high_threat_allied_stalker_fixture_keeps_source_followers(self) -> None:
        resolved = resolve_fixture_payload(
            "mcwilliams_live_debug_noon_high_threat_allies_stalker_2026-05-03",
            "live-debug",
        )

        self.assertEqual(
            [name for _profile, name in resolved["source_chain"]],
            [
                "mcwilliams_live_debug_noon_high_threat_allies_stalker_2026-05-03",
                "mcwilliams_live_debug_noon_stalker_2026-04-30",
                "mcwilliams_live_debug_2026-04-07",
            ],
        )
        transforms = resolved["save_transforms"]
        self.assertEqual(
            [transform["kind"] for transform in transforms],
            ["game_turn", "active_monsters_near_player", "overmap_npcs_near_player"],
        )
        self.assertEqual(transforms[0]["turn"], 5227200)
        self.assertEqual(
            [monster["typeid"] for monster in transforms[1]["monsters"]],
            ["mon_writhing_stalker"],
        )
        self.assertEqual(transforms[1]["monsters"][0]["offset_ms"], [7, 0, 0])
        self.assertEqual(transforms[2]["offsets_ms"], [[2, -2, 0]])
        self.assertTrue(transforms[2]["clone_follower_template"])

        scenario = load_scenario(
            "writhing_stalker.live_high_threat_allied_light_retreat_stalk_mcw"
        )
        steps_by_label = {
            step["label"]: step
            for step in scenario["steps"]
        }
        follower_guard = steps_by_label[
            "audit_saved_three_allied_followers_before_high_threat_light"
        ]
        self.assertEqual(follower_guard["required_observed_npc_count"], 3)
        self.assertEqual(
            [npc["name"] for npc in follower_guard["required_npcs"]],
            ["Katharina Leach", "Robbie Knox", "OpenClaw Ally 1"],
        )
        self.assertTrue(all(
            npc["my_fac"] == "your_followers"
            for npc in follower_guard["required_npcs"]
        ))
        stalker_guard = steps_by_label[
            "audit_saved_writhing_stalker_before_high_threat_allied_scene"
        ]
        self.assertEqual(
            stalker_guard["required_monsters"],
            [{"typeid": "mon_writhing_stalker", "offset_ms": [7, 0, 0]}],
        )

    def test_zzip_truncated_after_zstd_magic_is_rejected(self) -> None:
        source = self.sample_zzip_save()
        data = source.read_bytes()
        header, offset = _read_skippable_frame(
            data, 0, len(data), ZZIP_HEADER_MAGIC, ".sav.zzip header"
        )
        footer_size = struct.unpack_from("<Q", header)[0]
        footer = data[-footer_size:]
        _, offset = _read_skippable_frame(
            data,
            offset,
            len(data) - footer_size,
            ZZIP_FILENAME_MAGIC,
            ".sav.zzip entry filename metadata",
        )
        _, offset = _read_skippable_frame(
            data,
            offset,
            len(data) - footer_size,
            ZZIP_CHECKSUM_MAGIC,
            ".sav.zzip entry checksum metadata",
        )
        self.assertEqual(data[offset:offset + 4], ZSTD_FRAME_MAGIC)

        with tempfile.TemporaryDirectory() as temp_dir:
            save_path = Path(temp_dir) / source.name
            save_path.write_bytes(data[:offset + 4] + footer)
            error = player_save_error(save_path)

        self.assertIn("invalid or truncated .sav.zzip compressed entry", error)

    def test_zzip_footer_checksum_corruption_is_rejected(self) -> None:
        source = self.sample_zzip_save()
        data = bytearray(source.read_bytes())
        data[-1] ^= 0x01

        with tempfile.TemporaryDirectory() as temp_dir:
            save_path = Path(temp_dir) / source.name
            save_path.write_bytes(data)
            error = player_save_error(save_path)

        self.assertEqual(error, "corrupt .sav.zzip footer checksum")

    def test_zzip_rejects_valid_footer_transplanted_from_another_player(self) -> None:
        source = self.sample_zzip_save()
        donor = next(
            path
            for path in sorted(
                (HARNESS_DIR / "fixtures" / "saves").rglob("*.sav.zzip"),
                key=lambda path: path.as_posix(),
            )
            if path.name != source.name
        )
        source_data = source.read_bytes()
        donor_data = donor.read_bytes()
        source_header, source_body_start = _read_skippable_frame(
            source_data, 0, len(source_data), ZZIP_HEADER_MAGIC, ".sav.zzip header"
        )
        donor_header, donor_body_start = _read_skippable_frame(
            donor_data, 0, len(donor_data), ZZIP_HEADER_MAGIC, ".sav.zzip header"
        )
        source_footer_size = struct.unpack_from("<Q", source_header)[0]
        donor_footer_size = struct.unpack_from("<Q", donor_header)[0]
        hybrid = (
            donor_data[:donor_body_start]
            + source_data[source_body_start:len(source_data) - source_footer_size]
            + donor_data[len(donor_data) - donor_footer_size:]
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            save_path = Path(temp_dir) / source.name
            save_path.write_bytes(hybrid)
            error = player_save_error(save_path)

        self.assertIn(".sav.zzip footer", error)

    def test_zzip_compressed_frame_corruption_is_rejected(self) -> None:
        source = self.sample_zzip_save()
        data = bytearray(source.read_bytes())
        header, offset = _read_skippable_frame(
            data, 0, len(data), ZZIP_HEADER_MAGIC, ".sav.zzip header"
        )
        footer_size = struct.unpack_from("<Q", header)[0]
        footer_start = len(data) - footer_size
        _, offset = _read_skippable_frame(
            data,
            offset,
            footer_start,
            ZZIP_FILENAME_MAGIC,
            ".sav.zzip entry filename metadata",
        )
        _, offset = _read_skippable_frame(
            data,
            offset,
            footer_start,
            ZZIP_CHECKSUM_MAGIC,
            ".sav.zzip entry checksum metadata",
        )
        frame_size = zstd_decoder().frame_size(bytes(data[offset:footer_start]))
        data[offset + frame_size - 1] ^= 0x01

        with tempfile.TemporaryDirectory() as temp_dir:
            save_path = Path(temp_dir) / source.name
            save_path.write_bytes(data)
            error = player_save_error(save_path)

        self.assertIn("corrupt .sav.zzip compressed checksum", error)

    def test_plain_save_requires_valid_player_json(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            save_path = Path(temp_dir) / "player.sav"
            save_path.write_text("not json", encoding="utf-8")
            malformed_error = player_save_error(save_path)
            save_path.write_text("{}", encoding="utf-8")
            missing_fields_error = player_save_error(save_path)
            save_path.write_text(
                json.dumps({"savegame_loading_version": 39}),
                encoding="utf-8",
            )
            missing_player_error = player_save_error(save_path)
            save_path.write_text(
                json.dumps(
                    {
                        "savegame_loading_version": 39,
                        "player": {"name": "Test Player", "location": [0, 0, 0]},
                    }
                ),
                encoding="utf-8",
            )
            valid_error = player_save_error(save_path)

        self.assertIn("not valid JSON", malformed_error)
        self.assertEqual(
            missing_fields_error,
            "player save has no integer savegame_loading_version",
        )
        self.assertEqual(missing_player_error, "player save has no player object")
        self.assertEqual(valid_error, "")

    def test_release_gate_screens_use_state_guards_or_deferred_proof(self) -> None:
        failures: List[str] = []
        for scenario_name in RELEASE_GATE_SCENARIOS:
            scenario: Dict[str, Any] = load_scenario(scenario_name)
            steps = list(scenario.get("steps", []))
            label_indices = {
                str(step.get("label", "")).strip(): index
                for index, step in enumerate(steps)
            }
            for index, step in enumerate(steps):
                if not bool(step.get("capture_after", False)):
                    continue
                label = str(step.get("label", "")).strip()
                deferred = str(step.get("proof_deferred_to_label", "")).strip()
                screen_guarded = bool(
                    step.get("expected_screen_text_contains")
                    or step.get("expected_screen_text_after_contains")
                )
                if deferred:
                    target_index = label_indices.get(deferred)
                    if target_index is None or target_index <= index:
                        failures.append(
                            f"{scenario_name}:{label}: deferred guard {deferred!r} is missing or not later"
                        )
                elif not screen_guarded:
                    failures.append(
                        f"{scenario_name}:{label}: capture-only step has no OCR or deferred state guard"
                    )

        self.assertFalse(failures, "\n".join(failures))

    def test_staged_worlds_have_tracked_player_save(self) -> None:
        repo = HARNESS_DIR.parents[1]
        tracked = tracked_paths(repo)
        failures: List[str] = []

        for scenario_file in sorted(scenarios_root().glob("*.json"), key=lambda path: path.name.lower()):
            scenario_name = scenario_file.stem
            scenario: Dict[str, Any] = load_scenario(scenario_name)
            fixture = str(scenario.get("fixture", "")).strip()
            if not fixture:
                continue

            scenario_profile = str(scenario.get("profile", "")).strip()
            fixture_profile = str(scenario.get("fixture_profile", "")).strip() or scenario_profile
            source_profile = resolve_profile_name(fixture_profile)
            try:
                resolved = resolve_fixture_payload(fixture, source_profile)
            except SystemExit as exc:
                failures.append(f"{scenario_name}: fixture resolution failed: {exc}")
                continue

            world = str(scenario.get("world", "")).strip()
            world_dir = Path(resolved["save_src"]) / world
            candidates = player_save_paths(world_dir)
            tracked_saves = {
                path: player_save_error(path)
                for path in candidates
                if path.relative_to(repo).as_posix() in tracked
            }
            valid_tracked_saves = [
                path for path, validation_error in tracked_saves.items() if not validation_error
            ]
            if valid_tracked_saves:
                continue

            source_chain = " -> ".join(
                f"{profile}:{name}" for profile, name in resolved.get("source_chain", [])
            )
            present = ", ".join(path.name for path in candidates) or "none"
            invalid = ", ".join(
                f"{path.name}: {validation_error}"
                for path, validation_error in tracked_saves.items()
                if validation_error
            ) or "none"
            failures.append(
                f"{scenario_name}: fixture={source_profile}:{fixture}, world={world or '<unspecified>'}, "
                f"source_chain={source_chain or '<empty>'}, transforms={len(resolved.get('save_transforms', []))}, "
                f"present_top_level_player_saves={present}, invalid_tracked_saves={invalid}"
            )

        self.assertFalse(
            failures,
            "Scenario fixtures whose final staged world has no tracked .sav or .sav.zzip:\n"
            + "\n".join(f"- {failure}" for failure in failures),
        )


if __name__ == "__main__":
    unittest.main()
