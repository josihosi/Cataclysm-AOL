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
from typing import Any, Dict, Iterable, List, Optional, Set
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
    EOC_POPUP_TRACE_READ_CAP_BYTES,
    OMAPX,
    OMAPY,
    StartupPlan,
    acknowledge_blocking_interruptions,
    audit_fresh_ecology_incident_pair,
    audit_local_reality_safety_preflight_text,
    audit_log_contains,
    audit_log_not_contains,
    audit_saved_bandit_live_world_state,
    apply_bandit_camp_map_lead_transform,
    apply_bandit_clear_site_evidence_transform,
    apply_bandit_clone_site_transform,
    apply_fixture_save_transforms,
    apply_game_turn_to_payload,
    apply_overmap_terrain_id_at_abs_omt_transform,
    apply_option_overrides_to_file,
    apply_player_mutations_transform,
    apply_repair_basecamp_npc_assignments_transform,
    apply_remove_overmap_npcs_transform,
    artifact_delta_matches_all_patterns,
    classify_blocking_interruption,
    classify_wait_screen_text,
    classify_wait_step_ledger,
    cleanup_extracted_overmap,
    capture_wait_artifact_delta,
    debug_map_editor_place_field,
    debug_map_editor_place_item,
    decode_overmap_layer,
    encode_overmap_layer,
    extract_clock_or_turn_evidence,
    extract_overmap_payload,
    execute_long_wait_action,
    execute_probe_steps,
    ecology_incident_artifact_baseline,
    evaluate_screen_text_expectation,
    launch_game,
    latest_now_minutes_marker,
    load_player_abs_omt,
    load_profile_config,
    load_scenario,
    normalize_screen_text_patterns,
    normalize_fixture_save_transforms,
    overmap_file_coords_from_abs_omt,
    overmap_flat_index,
    overmap_layer_index,
    peekaboo_focus_pid_with_retry,
    peekaboo_press_sequence,
    peekaboo_switch_app_for_pid,
    poll_wait_artifact_completion,
    provision_llm_api_key_environment,
    read_active_eoc_popup_trace,
    read_active_activity_query_trace,
    read_secure_llm_credential,
    resolve_configured_python_command,
    resolve_fixture_payload,
    resolve_game_runtime_python,
    resolve_profile_name,
    resolve_profile_snapshot_payload,
    resolve_scenario_profile_option_overrides,
    resolve_startup_config_profile,
    run_launch_only_handoff,
    run_probe_mode,
    scenarios_root,
    store_secure_llm_credential,
    summarize_bandit_live_world_site,
    validate_enabled_api_runtime,
    write_macos_llm_credential,
)
from bandit_live_world_audit import zzip_binary as bandit_zzip_binary  # noqa: E402


class SaveValidationError(RuntimeError):
    """A stable, user-facing reason a player save cannot be trusted."""


class PeekabooPressSequenceContractTest(unittest.TestCase):
    def test_batches_consecutive_special_keys_in_one_press_command(self) -> None:
        command = mock.Mock(side_effect=lambda args, **_: list(args))
        interaction = mock.Mock()
        with (
            mock.patch("startup_harness.peekaboo_command", command),
            mock.patch("startup_harness.run_peekaboo_interaction", interaction),
        ):
            peekaboo_press_sequence(42, ["right"] * 60, delay_ms=123)

        expected_command = [
            "press",
            *(["right"] * 60),
            "--pid",
            "42",
            "--delay",
            "123",
        ]
        command.assert_called_once_with(expected_command, channel="input")
        interaction.assert_called_once_with(42, expected_command)

    def test_preserves_mixed_text_hotkey_and_special_key_order(self) -> None:
        command = mock.Mock(side_effect=lambda args, **_: list(args))
        interaction = mock.Mock()
        type_text = mock.Mock()
        hotkey = mock.Mock()
        ordered_calls = mock.Mock()
        ordered_calls.attach_mock(interaction, "interaction")
        ordered_calls.attach_mock(type_text, "type_text")
        ordered_calls.attach_mock(hotkey, "hotkey")
        with (
            mock.patch("startup_harness.peekaboo_command", command),
            mock.patch("startup_harness.run_peekaboo_interaction", interaction),
            mock.patch("startup_harness.peekaboo_type_text", type_text),
            mock.patch("startup_harness.peekaboo_hotkey", hotkey),
        ):
            peekaboo_press_sequence(
                42,
                ["a", "b", "right", "down", "C", "!", "x", "enter", "left"],
                delay_ms=123,
            )

        self.assertEqual(
            ordered_calls.mock_calls,
            [
                mock.call.type_text(42, "ab", delay_ms=123),
                mock.call.interaction(
                    42,
                    ["press", "right", "down", "--pid", "42", "--delay", "123"],
                ),
                mock.call.hotkey(42, "shift,c", hold_ms=123),
                mock.call.hotkey(42, "shift,1", hold_ms=123),
                mock.call.type_text(42, "x", delay_ms=123),
                mock.call.interaction(
                    42,
                    ["press", "return", "left", "--pid", "42", "--delay", "123"],
                ),
            ],
        )

    def test_routes_bracket_after_batched_cursor_movement_as_physical_hotkey(self) -> None:
        command = mock.Mock(side_effect=lambda args, **_: list(args))
        interaction = mock.Mock()
        type_text = mock.Mock()
        hotkey = mock.Mock()
        ordered_calls = mock.Mock()
        ordered_calls.attach_mock(interaction, "interaction")
        ordered_calls.attach_mock(hotkey, "hotkey")
        with (
            mock.patch("startup_harness.peekaboo_command", command),
            mock.patch("startup_harness.run_peekaboo_interaction", interaction),
            mock.patch("startup_harness.peekaboo_type_text", type_text),
            mock.patch("startup_harness.peekaboo_hotkey", hotkey),
        ):
            peekaboo_press_sequence(
                42,
                ["right", "right", "up", "up", "["],
                delay_ms=123,
            )

        expected_command = [
            "press",
            "right",
            "right",
            "up",
            "up",
            "--pid",
            "42",
            "--delay",
            "123",
        ]
        command.assert_called_once_with(expected_command, channel="input")
        self.assertEqual(
            ordered_calls.mock_calls,
            [
                mock.call.interaction(42, expected_command),
                mock.call.hotkey(42, "[", hold_ms=123),
            ],
        )
        type_text.assert_not_called()

    def test_empty_sequence_does_not_issue_input(self) -> None:
        with (
            mock.patch("startup_harness.peekaboo_command") as command,
            mock.patch("startup_harness.run_peekaboo_interaction") as interaction,
            mock.patch("startup_harness.peekaboo_type_text") as type_text,
            mock.patch("startup_harness.peekaboo_hotkey") as hotkey,
        ):
            peekaboo_press_sequence(42, [])

        command.assert_not_called()
        interaction.assert_not_called()
        type_text.assert_not_called()
        hotkey.assert_not_called()


class BlockingInterruptionClassifierContractTest(unittest.TestCase):
    def test_activity_distraction_prompt_accepts_observed_shortcut_ocr(self) -> None:
        result = classify_blocking_interruption({
            "ok": True,
            "text": (
                "Hostile survivor spotted! Stop waiting? (Case Sensitive)\n"
                "Open [M]anager\n[I]gnore this distraction and continue"
            ),
        })

        self.assertEqual(result["status"], "known_prompt")
        self.assertEqual(result["classification"], "activity_distraction_prompt")
        self.assertEqual(result["response_key"], "I")

        observed_ocr = classify_blocking_interruption({
            "ok": True,
            "text": (
                "Hostilel survivor spotted! Stop waiting? (Case Sensitivel\n"
                "Open [Mlanager\n[llgnore this distraction and continue"
            ),
        })
        self.assertEqual(observed_ocr["status"], "known_prompt")
        self.assertEqual(observed_ocr["classification"], "activity_distraction_prompt")
        self.assertEqual(observed_ocr["response_key"], "I")

    def test_in_progress_wait_overlay_is_clear_across_ocr_layouts(self) -> None:
        cases = (
            "Waiting 42%\nPress | to interrupt waiting\nTiles",
            "Press . to interrupt waiting",
            "Press\nwaiting:\nor 5 to interrupt\n25%",
            "Press\nwaiting:\nor\n62%\nARM\nto\nHEAD\ninterrupt\nTiles",
            "Press\nwaiting:\nor\n882\nARM\nto\ninterrupt\nTiles",
            "Press . or 5 to interrupt\nwaiting:\n632",
        )
        for body in cases:
            with self.subTest(body=body):
                result = classify_blocking_interruption({"ok": True, "text": body})

                self.assertEqual(result["status"], "clear")
                self.assertEqual(result["classification"], "wait_activity_in_progress")
                self.assertEqual(result["response_key"], "")

    def test_split_active_wait_progress_is_clear_without_sending_input(self) -> None:
        observed_ocr = "Press\nwaiting:\nor\n5\n5%\nto\ninterrup"
        direct = classify_blocking_interruption({"ok": True, "text": observed_ocr})

        self.assertEqual(direct["status"], "clear")
        self.assertEqual(direct["classification"], "wait_activity_in_progress")
        self.assertEqual(direct["response_key"], "")

        with (
            tempfile.TemporaryDirectory() as temp_dir,
            mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
            mock.patch(
                "startup_harness.capture_screen_text_artifact",
                return_value={"ok": True, "text": observed_ocr},
            ),
            mock.patch("startup_harness.peekaboo_press_sequence") as press,
        ):
            result = acknowledge_blocking_interruptions(
                42,
                Path(temp_dir),
                "wait_contract.split_progress",
                stop_on_unknown=True,
            )

        self.assertEqual(result["status"], "clear")
        self.assertEqual(result["acknowledgement_count"], 0)
        self.assertEqual(
            result["final_classification"]["classification"],
            "wait_activity_in_progress",
        )
        press.assert_not_called()

    def test_active_wait_banner_wins_over_safe_mode_hud_history(self) -> None:
        observed_ocr = (
            "Press . or 5 to interrupt\n"
            "waiting:\n382\n"
            "Safe: Off\n"
            "Safe mode OFF!\n"
            "Tiles"
        )

        result = classify_blocking_interruption({"ok": True, "text": observed_ocr})

        self.assertEqual(result["status"], "clear")
        self.assertEqual(result["classification"], "wait_activity_in_progress")
        self.assertEqual(result["response_key"], "")

    def test_bare_press_without_active_wait_progress_remains_blocked(self) -> None:
        result = classify_blocking_interruption({
            "ok": True,
            "text": "Press\nAn unfamiliar prompt is waiting for a choice.",
        })

        self.assertEqual(result["status"], "unknown_prompt")
        self.assertEqual(result["classification"], "partial_safe_mode_spotted_hostile_prompt")
        self.assertEqual(result["response_key"], "")

    def test_genuine_safe_mode_prompts_remain_fail_closed(self) -> None:
        cases = (
            (
                "Safe mode\nPress a key\nTiles",
                "known_prompt",
                "safe_mode_spotted_hostile_prompt",
            ),
            (
                "Spotted hostile while waiting\nPress | to interrupt waiting",
                "unknown_prompt",
                "partial_safe_mode_spotted_hostile_prompt",
            ),
            (
                "Safe mode\nPress\nTiles\nPress . to interrupt waiting",
                "known_prompt",
                "safe_mode_spotted_hostile_prompt",
            ),
            (
                "Safe mode\nPress . to interrupt waiting",
                "unknown_prompt",
                "partial_safe_mode_spotted_hostile_prompt",
            ),
        )
        for body, expected_status, expected_classification in cases:
            with self.subTest(body=body):
                result = classify_blocking_interruption({"ok": True, "text": body})

                self.assertEqual(result["status"], expected_status)
                self.assertEqual(result["classification"], expected_classification)

    def test_scattered_wait_words_remain_fail_closed(self) -> None:
        body = "Press any key.\n" + ( "unrelated status text " * 12 ) + "\nwaiting for changes to interrupt"

        result = classify_blocking_interruption({"ok": True, "text": body})

        self.assertEqual(result["status"], "unknown_prompt")
        self.assertEqual(result["classification"], "partial_safe_mode_spotted_hostile_prompt")
        self.assertEqual(result["response_key"], "")

    def test_lifeless_grass_wilderness_flavor_popup_is_known(self) -> None:
        body = (
            "You suddenly realize this area seems almost devoid of life. The few bits of "
            "blackened grass you can see are barely recognizable as grass, even discounting "
            "the night. Sure the zombies might be tearing people apart, but what happened to "
            "the grass? What is the grass coming to life to eat people too? You snort at the "
            "idea, but it doesn't feel safe out here."
        )

        result = classify_blocking_interruption({"ok": True, "text": body})

        self.assertEqual(result["status"], "known_prompt")
        self.assertEqual(
            result["classification"],
            "shadow_warning_wilderness_flavor_popup",
        )
        self.assertEqual(result["response_key"], "space")
        self.assertFalse(result["release_blocking"])
        self.assertFalse(result["contaminating"])

    def test_howling_wind_wilderness_flavor_popup_is_exact_and_fail_closed(self) -> None:
        exact_text = "The wind sure is howling tonight"
        exact = classify_blocking_interruption({"ok": True, "text": exact_text})
        partial = classify_blocking_interruption({
            "ok": True,
            "text": "The wind sure is howling",
        })
        unknown_confirmation = classify_blocking_interruption({
            "ok": True,
            "text": f"{exact_text}\nApply changes? (y/n)",
        })

        self.assertEqual(exact["status"], "known_prompt")
        self.assertEqual(
            exact["classification"],
            "shadow_warning_wilderness_flavor_popup",
        )
        self.assertEqual(exact["response_key"], "space")
        self.assertFalse(exact["release_blocking"])
        self.assertFalse(exact["contaminating"])
        self.assertEqual(partial["status"], "clear")
        self.assertEqual(partial["classification"], "no_known_prompt")
        self.assertEqual(partial["response_key"], "")
        self.assertEqual(unknown_confirmation["status"], "unknown_prompt")
        self.assertEqual(
            unknown_confirmation["classification"],
            "unhandled_blocking_menu",
        )
        self.assertEqual(unknown_confirmation["response_key"], "")

    def test_howling_wind_wilderness_flavor_popup_acknowledges_with_space(self) -> None:
        screen_reads = iter([
            {"ok": True, "text": "The wind sure is howling tonight"},
            {"ok": True, "text": ""},
        ])
        with (
            tempfile.TemporaryDirectory() as temp_dir,
            mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
            mock.patch(
                "startup_harness.capture_screen_text_artifact",
                side_effect=lambda *_args, **_kwargs: next(screen_reads),
            ),
            mock.patch("startup_harness.peekaboo_press_sequence") as press,
            mock.patch("startup_harness.time.sleep"),
        ):
            result = acknowledge_blocking_interruptions(
                42,
                Path(temp_dir),
                "wait_contract.howling_wind",
                stop_on_unknown=True,
            )

        self.assertEqual(result["status"], "clear")
        self.assertEqual(result["acknowledgement_count"], 1)
        self.assertEqual(
            result["acknowledgements"][0]["classification"]["classification"],
            "shadow_warning_wilderness_flavor_popup",
        )
        self.assertEqual(result["acknowledgements"][0]["response_key"], "space")
        press.assert_called_once_with(42, ["space"], delay_ms=200)

    def test_partial_lifeless_grass_wilderness_flavor_remains_unknown(self) -> None:
        result = classify_blocking_interruption({
            "ok": True,
            "text": "You suddenly realize this area seems almost devoid of life.",
        })

        self.assertEqual(result["status"], "unknown_prompt")
        self.assertEqual(
            result["classification"],
            "partial_lifeless_grass_wilderness_flavor_popup",
        )
        self.assertEqual(result["response_key"], "")
        self.assertFalse(result["release_blocking"])
        self.assertFalse(result["contaminating"])

    def test_observed_multiline_lifeless_grass_popup_is_known(self) -> None:
        result = classify_blocking_interruption({
            "ok": True,
            "text": (
                "blackened grass you can\nYou suddenly realize\nthis\n"
                "area\nseems almost devoid\nof\nlife\n"
                "what happened to the grass? What, is the grass coming to\n"
                "discounting the night.\nWaiting 79%\nPress . or 5 to interrupt"
            ),
        })

        self.assertEqual(result["status"], "known_prompt")
        self.assertEqual(
            result["classification"],
            "shadow_warning_wilderness_flavor_popup",
        )
        self.assertEqual(result["response_key"], "space")
        self.assertEqual(
            result["matched_markers"],
            ["almost devoid of life", "what happened to the grass"],
        )

    def test_lifeless_grass_text_cannot_override_safe_mode_prompt(self) -> None:
        result = classify_blocking_interruption({
            "ok": True,
            "text": (
                "This area seems almost devoid of life. What happened to the grass? "
                "Is the grass coming to life to eat people too? Spotted hostile."
            ),
        })

        self.assertEqual(result["status"], "unknown_prompt")
        self.assertEqual(
            result["classification"],
            "partial_safe_mode_spotted_hostile_prompt",
        )
        self.assertEqual(result["response_key"], "")

    def test_all_early_shadow_warning_snippets_are_known_flavor_popups(self) -> None:
        snippets = (
            (
                "Even out here you can still see the burning cities lighting up the horizon.  What scares you most is not the gunshots "
                "you sometimes hear in the far distance, but the idea of what people keep shooting at.  Tonight you hear automatic "
                "gunfire winding down into single shots.\n\nJust what could make them shoot that much, all the way out here?"
            ),
            (
                "You suddenly realize this area seems almost devoid of life.  The few bits of blackened grass you can see are barely "
                "recognizable as grass, even discounting the night.  Sure the zombies might be tearing people apart, but what happened "
                "to the grass?  What, is the grass coming to life to eat people too?\nYou snort at the idea, but it doesn't feel safe out here."
            ),
            "You have a vague feeling of being watched.",
            (
                "Your ears perk up as something rustles, but it just turns out to be unnaturally withered and blackened vegetation you "
                "stepped on.  Wait, this can not have happened on its own."
            ),
            "The night feels longer than usual.",
            "You suddenly yearn for a beautiful sunrise.",
            "The wind faintly cries into the night.",
            "The darkness makes you nervous.",
        )

        for snippet in snippets:
            with self.subTest(snippet=snippet):
                result = classify_blocking_interruption({"ok": True, "text": snippet})

                self.assertEqual(result["status"], "known_prompt")
                self.assertEqual(
                    result["classification"],
                    "shadow_warning_wilderness_flavor_popup",
                )
                self.assertEqual(result["response_key"], "space")
                self.assertFalse(result["release_blocking"])
                self.assertFalse(result["contaminating"])

    def test_observed_shadow_warning_variants_override_wait_progress_only(self) -> None:
        for warning in (
            "The wind faintly cries into the night.",
            "The darkness makes you nervous.",
        ):
            with self.subTest(warning=warning):
                result = classify_blocking_interruption({
                    "ok": True,
                    "text": f"{warning}\nWaiting 25%\nPress | to interrupt waiting",
                })

                self.assertEqual(result["status"], "known_prompt")
                self.assertEqual(
                    result["classification"],
                    "shadow_warning_wilderness_flavor_popup",
                )
                self.assertEqual(result["response_key"], "space")

    def test_observed_garbled_vague_watched_prompt_overrides_wait_progress(self) -> None:
        result = classify_blocking_interruption({
            "ok": True,
            "text": (
                "ur hove a veuee vod ny ur ueiny satcheu\n"
                "Waiting 25%\nPress | to interrupt waiting\nTiles"
            ),
        })

        self.assertEqual(result["status"], "known_prompt")
        self.assertEqual(
            result["classification"],
            "shadow_warning_wilderness_flavor_popup",
        )
        self.assertEqual(result["response_key"], "space")
        self.assertEqual(
            result["matched_markers"],
            ["garbled vague feeling of being watched"],
        )

    def test_fragmented_wait_banner_does_not_require_ocr_percent(self) -> None:
        result = classify_blocking_interruption({
            "ok": True,
            "text": "waiting:\n71-\nPress\nor\n712\nto\ninterrupt",
        })

        self.assertEqual(result["status"], "clear")
        self.assertEqual(result["classification"], "wait_activity_in_progress")
        self.assertEqual(result["matched_markers"], ["waiting", "to interrupt"])

    def test_garbled_shadow_warning_cannot_override_safety_prompts(self) -> None:
        cases = (
            (
                "ur hove a veuee vod ny ur ueiny satcheu\nSafe mode\nPress a key\nTiles",
                "known_prompt",
                "safe_mode_spotted_hostile_prompt",
                False,
                "'",
            ),
            (
                "ur hove a veuee vod ny ur ueiny satcheu\nSpotted hostile while waiting.",
                "unknown_prompt",
                "partial_safe_mode_spotted_hostile_prompt",
                False,
                "",
            ),
            (
                "ur hove a veuee vod ny ur ueiny satcheu\n"
                "There is a buzzing in your senses. A tiny cataclysm has begun.",
                "known_prompt",
                "portal_storm_notice",
                True,
                "space",
            ),
            (
                "ur hove a veuee vod ny ur ueiny satcheu\nApply changes? (y/n)",
                "unknown_prompt",
                "unhandled_blocking_menu",
                False,
                "",
            ),
        )
        for body, expected_status, expected_classification, contaminating, response_key in cases:
            with self.subTest(body=body):
                result = classify_blocking_interruption({"ok": True, "text": body})

                self.assertEqual(result["status"], expected_status)
                self.assertEqual(result["classification"], expected_classification)
                self.assertEqual(result["contaminating"], contaminating)
                self.assertEqual(result["response_key"], response_key)

    def test_partial_garbled_warning_does_not_override_wait_text(self) -> None:
        result = classify_blocking_interruption({
            "ok": True,
            "text": "ur hove a veuee\nWaiting 25%\nPress | to interrupt waiting\nTiles",
        })

        self.assertEqual(result["status"], "clear")
        self.assertEqual(result["classification"], "wait_activity_in_progress")
        self.assertEqual(result["response_key"], "")

    def test_shadow_warning_text_cannot_override_safe_mode_prompt(self) -> None:
        result = classify_blocking_interruption({
            "ok": True,
            "text": "The darkness makes you nervous. Spotted hostile while waiting.",
        })

        self.assertEqual(result["status"], "unknown_prompt")
        self.assertEqual(
            result["classification"],
            "partial_safe_mode_spotted_hostile_prompt",
        )
        self.assertEqual(result["response_key"], "")

    def test_shadow_warning_text_cannot_override_unknown_confirmation(self) -> None:
        result = classify_blocking_interruption({
            "ok": True,
            "text": "The darkness makes you nervous.\nApply changes? (y/n)",
        })

        self.assertEqual(result["status"], "unknown_prompt")
        self.assertEqual(result["classification"], "unhandled_blocking_menu")
        self.assertEqual(result["response_key"], "")

    def test_shadow_warning_text_cannot_override_portal_storm_notice(self) -> None:
        full = classify_blocking_interruption({
            "ok": True,
            "text": (
                "The darkness makes you nervous.\n"
                "There is a buzzing in your senses. A tiny cataclysm has begun."
            ),
        })
        partial = classify_blocking_interruption({
            "ok": True,
            "text": "The darkness makes you nervous.\nThere is a buzzing in your senses.",
        })

        self.assertEqual(full["classification"], "portal_storm_notice")
        self.assertTrue(full["contaminating"])
        self.assertEqual(partial["classification"], "partial_portal_storm_notice")
        self.assertEqual(partial["status"], "unknown_prompt")
        self.assertTrue(partial["contaminating"])


class ScreenTextExpectationSpatialRowContractTest(unittest.TestCase):
    EXPECTED = "Armed BD-DF9E73"

    @staticmethod
    def observation(text: str, x: float, y: float, height: float) -> Dict[str, Any]:
        return {
            "text": text,
            "x": x,
            "y": y,
            "w": 0.04,
            "h": height,
        }

    def evaluate(self, observations: List[Dict[str, Any]]) -> Dict[str, Any]:
        identity_line = next(
            str(observation.get("text", ""))
            for observation in observations
            if str(observation.get("text", "")).startswith("BD-")
        )
        payload = {
            "ok": True,
            "lines": [
                "Armed",
                "Fail on trigger",
                "Inspect",
                "edit",
                "selected member",
                identity_line,
            ],
            "text": (
                "Armed\nFail on trigger\nInspect\nedit\nselected member\n"
                f"{identity_line}"
            ),
            "observations": observations,
        }
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "screen_text.json"
            path.write_text(json.dumps(payload), encoding="utf-8")
            return evaluate_screen_text_expectation(
                {"json_path": str(path)},
                [self.EXPECTED],
            )

    def preserved_observations(self, identity: str = "BD-DF9E73") -> List[Dict[str, Any]]:
        return [
            self.observation("Armed", 0.3807189548, 0.7325581399, 0.0087209298),
            self.observation(
                f"{identity} (selected_phase_change, continue_capture, deadline 8884)",
                0.4150326929,
                0.7295138886,
                0.0118055556,
            ),
        ]

    def test_overlapping_left_to_right_observations_match_full_identity_phrase(self) -> None:
        result = self.evaluate(self.preserved_observations())

        self.assertEqual(result["status"], "matched")
        self.assertEqual(result["missing"], [])
        self.assertEqual(result["matches"][0]["source"], "spatial_ocr_row")
        self.assertIn(self.EXPECTED, result["matches"][0]["line"])

    def test_wrong_identity_does_not_match(self) -> None:
        result = self.evaluate(self.preserved_observations("BD-OTHER"))

        self.assertEqual(result["status"], "missing")
        self.assertEqual(result["missing"], [self.EXPECTED])

    def test_non_overlapping_rows_do_not_match(self) -> None:
        observations = self.preserved_observations()
        observations[1]["y"] = 0.75
        result = self.evaluate(observations)

        self.assertEqual(result["status"], "missing")
        self.assertEqual(result["missing"], [self.EXPECTED])

    def test_reversed_horizontal_order_does_not_match(self) -> None:
        observations = self.preserved_observations()
        observations[0]["x"] = 0.5
        observations[1]["x"] = 0.4
        result = self.evaluate(observations)

        self.assertEqual(result["status"], "missing")
        self.assertEqual(result["missing"], [self.EXPECTED])


class WaitStepLedgerContractTest(unittest.TestCase):
    def test_wait_completion_phrase_does_not_require_ocr_punctuation(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            screen_text_path = Path(temp_dir) / "screen_text.json"
            screen_text_path.write_text(json.dumps({
                "lines": ["The darkness makes you nervous.", "You finish waiting"],
            }), encoding="utf-8")
            report = classify_wait_screen_text(
                {"json_path": str(screen_text_path)},
                ["You finish waiting"],
                ["Stop moving?"],
            )

        self.assertEqual(report["status"], "completed")
        self.assertEqual(
            report["complete_matches"],
            [{"pattern": "You finish waiting", "line": "You finish waiting"}],
        )

    @staticmethod
    def artifact_report() -> Dict[str, Any]:
        return {
            "patterns": ["endpoint"],
            "matches_by_pattern": [{"pattern": "endpoint", "lines": ["endpoint"]}],
            "path": "endpoint.log",
        }

    @staticmethod
    def exact_endpoint_artifact_report(
        *,
        start_size: int = 100,
        end_size: int = 200,
    ) -> Dict[str, Any]:
        patterns = ["scheduler_hour=149", "now_minutes=8940"]
        return {
            "status": "captured",
            "start_size": start_size,
            "end_size": end_size,
            "patterns": patterns,
            "matches_by_pattern": [
                {"pattern": pattern, "lines": [f"cadence {pattern}"]}
                for pattern in patterns
            ],
            "path": "endpoint.log",
        }

    def classify(
        self,
        choice_key: str,
        expected_duration: str,
        before: str,
        after: str,
        *,
        artifact_report: Optional[Dict[str, Any]] = None,
        wait_status: str = "completed",
        allow_meridiem_ambiguity: bool = False,
    ) -> Dict[str, Any]:
        return classify_wait_step_ledger(
            label="wait_contract",
            choice_key=choice_key,
            expected_duration=expected_duration,
            before_text={"text": before},
            menu_text={"text": "Wait a while: 5 minutes, 1 hour, 6 hours"},
            after_text={"text": after},
            wait_classification={"status": wait_status},
            artifact_after_wait=artifact_report or self.artifact_report(),
            allow_artifact_elapsed_without_menu_ocr=True,
            allow_exact_artifact_meridiem_ambiguity=allow_meridiem_ambiguity,
        )

    def test_rejects_choice_key_and_elapsed_duration_mismatch(self) -> None:
        report = self.classify("3", "1h", "Time: 10:00:00PM", "Time: 10:05:00PM")

        self.assertEqual(report["verdict"], "yellow_wait_elapsed_or_menu_not_fully_proven")
        self.assertIn("choice_key_does_not_match_expected_duration", report["issues"])
        self.assertIn("clock_delta_does_not_match_expected_duration", report["issues"])

    def test_accepts_six_hours_across_midnight_in_twelve_hour_clock(self) -> None:
        report = self.classify("8", "6h", "Time: 10:00:00PM", "Time: 4:00:00AM")

        self.assertEqual(report["elapsed"]["delta_seconds"], 6 * 60 * 60)
        self.assertEqual(report["verdict"], "green_wait_step_proven")

    def test_accepts_split_hud_meridiem_after_weekday(self) -> None:
        before = "4:00:00\nThursday,\nPM\nMay 20"
        after = "10:00:00\nThursday,\nPM\nMay 20"

        parsed_before = extract_clock_or_turn_evidence({"text": before})
        parsed_24_hour = extract_clock_or_turn_evidence({"text": "Time: 16:00:00"})
        report = self.classify("8", "6h", before, after)

        self.assertEqual(
            parsed_before["clock_matches"][0]["seconds_since_midnight"],
            16 * 60 * 60,
        )
        self.assertEqual(
            parsed_24_hour["clock_matches"][0]["seconds_since_midnight"],
            16 * 60 * 60,
        )
        self.assertEqual(report["elapsed"]["delta_seconds"], 6 * 60 * 60)
        self.assertEqual(report["verdict"], "green_wait_step_proven")

    def test_accepts_split_hud_meridiem_after_month(self) -> None:
        before = "Time: Friday, 12:00:00PM"
        after = "Time: 1:00:00\nMay\nPM\n21"

        parsed_after = extract_clock_or_turn_evidence({"text": after})
        report = self.classify("5", "1h", before, after)

        self.assertEqual(
            parsed_after["clock_matches"][0]["seconds_since_midnight"],
            13 * 60 * 60,
        )
        self.assertEqual(report["elapsed"]["delta_seconds"], 60 * 60)
        self.assertEqual(report["verdict"], "green_wait_step_proven")

    def test_exact_artifacts_can_confirm_bare_after_clock_meridiem_ambiguity(self) -> None:
        before = "Time: 12:00:00PM"
        after = (
            "Time: 1:00:00\nMay 21\nWind: 3 mph\nLight: bright\n"
            "Air: comfortable\nPM"
        )
        report = self.classify(
            "5",
            "1h",
            before,
            after,
            artifact_report=self.exact_endpoint_artifact_report(),
            allow_meridiem_ambiguity=True,
        )

        self.assertEqual(report["after_clock_or_turn"]["clock_matches"][0]["meridiem"], "")
        self.assertEqual(report["elapsed"]["raw_delta_seconds"], 13 * 60 * 60)
        self.assertEqual(report["elapsed"]["alternative_delta_seconds"], 60 * 60)
        self.assertEqual(
            report["elapsed"]["status"],
            "artifact_confirmed_meridiem_ambiguity",
        )
        self.assertIn("omitted AM/PM", report["elapsed"]["note"])
        self.assertTrue(report["meridiem_ambiguity_confirmed"])
        self.assertEqual(report["verdict"], "green_wait_step_proven")

    def test_exact_artifacts_can_confirm_bare_before_clock_meridiem_ambiguity(self) -> None:
        report = self.classify(
            "8",
            "6h",
            "Time: 4:01:00\nMay 21\nWind: calm",
            "Time: 10:01:00PM",
            artifact_report=self.exact_endpoint_artifact_report(),
            allow_meridiem_ambiguity=True,
        )

        self.assertEqual(report["before_clock_or_turn"]["clock_matches"][0]["meridiem"], "")
        self.assertEqual(report["elapsed"]["raw_delta_seconds"], 18 * 60 * 60)
        self.assertEqual(report["elapsed"]["alternative_delta_seconds"], 6 * 60 * 60)
        self.assertTrue(report["meridiem_ambiguity_confirmed"])
        self.assertEqual(report["verdict"], "green_wait_step_proven")

    def test_bare_after_clock_meridiem_ambiguity_requires_explicit_opt_in(self) -> None:
        report = self.classify(
            "5",
            "1h",
            "Time: 12:00:00PM",
            "Time: 1:00:00\nMay 21\nWind: calm\nLight: bright\nAir: clear\nPM",
            artifact_report=self.exact_endpoint_artifact_report(),
        )

        self.assertEqual(report["elapsed"]["status"], "clock_delta_parsed")
        self.assertIn("clock_delta_does_not_match_expected_duration", report["issues"])
        self.assertEqual(report["verdict"], "yellow_wait_elapsed_or_menu_not_fully_proven")

    def test_explicit_after_meridiem_is_not_treated_as_ambiguous(self) -> None:
        report = self.classify(
            "5",
            "1h",
            "Time: 12:00:00PM",
            "Time: 1:00:00AM",
            artifact_report=self.exact_endpoint_artifact_report(),
            allow_meridiem_ambiguity=True,
        )

        self.assertEqual(report["after_clock_or_turn"]["clock_matches"][0]["meridiem"], "am")
        self.assertFalse(report["meridiem_ambiguity_confirmed"])
        self.assertEqual(report["verdict"], "yellow_wait_elapsed_or_menu_not_fully_proven")

    def test_non_twelve_hour_clock_mismatch_stays_yellow(self) -> None:
        report = self.classify(
            "5",
            "1h",
            "Time: 12:00:00PM",
            "Time: 2:00:00\nMay 21\nWind: calm\nLight: bright\nAir: clear\nPM",
            artifact_report=self.exact_endpoint_artifact_report(),
            allow_meridiem_ambiguity=True,
        )

        self.assertIsNone(report["elapsed"]["alternative_delta_seconds"])
        self.assertFalse(report["meridiem_ambiguity_confirmed"])
        self.assertEqual(report["verdict"], "yellow_wait_elapsed_or_menu_not_fully_proven")

    def test_meridiem_ambiguity_rejects_wrong_choice_key(self) -> None:
        report = self.classify(
            "3",
            "1h",
            "Time: 12:00:00PM",
            "Time: 1:00:00\nMay 21\nWind: calm\nLight: bright\nAir: clear\nPM",
            artifact_report=self.exact_endpoint_artifact_report(),
            allow_meridiem_ambiguity=True,
        )

        self.assertFalse(report["meridiem_ambiguity_confirmed"])
        self.assertIn("choice_key_does_not_match_expected_duration", report["issues"])
        self.assertEqual(report["verdict"], "yellow_wait_elapsed_or_menu_not_fully_proven")

    def test_meridiem_ambiguity_rejects_generic_artifacts(self) -> None:
        generic_artifact = {
            "status": "captured",
            "start_size": 100,
            "end_size": 200,
            "patterns": ["endpoint"],
            "matches_by_pattern": [{"pattern": "endpoint", "lines": ["endpoint"]}],
            "path": "endpoint.log",
        }
        report = self.classify(
            "5",
            "1h",
            "Time: 12:00:00PM",
            "Time: 1:00:00\nMay 21\nWind: calm\nLight: bright\nAir: clear\nPM",
            artifact_report=generic_artifact,
            allow_meridiem_ambiguity=True,
        )

        self.assertFalse(report["meridiem_ambiguity_confirmed"])
        self.assertEqual(report["verdict"], "yellow_wait_elapsed_or_menu_not_fully_proven")

    def test_meridiem_ambiguity_accepts_artifact_completed_and_rejects_interrupted_status(self) -> None:
        for wait_status, expected_verdict in (
            ("unknown_after_wait", "green_wait_step_proven"),
            ("interrupted_or_prompt_visible", "blocked_wait_interrupted_or_prompt_visible"),
        ):
            with self.subTest(wait_status=wait_status):
                report = self.classify(
                    "5",
                    "1h",
                    "Time: 12:00:00PM",
                    "Time: 1:00:00\nMay 21\nWind: calm\nLight: bright\nAir: clear\nPM",
                    artifact_report=self.exact_endpoint_artifact_report(),
                    wait_status=wait_status,
                    allow_meridiem_ambiguity=True,
                )

                self.assertEqual(
                    report["meridiem_ambiguity_confirmed"],
                    wait_status == "unknown_after_wait",
                )
                self.assertEqual(report["verdict"], expected_verdict)

    def test_meridiem_ambiguity_requires_new_artifact_bytes(self) -> None:
        report = self.classify(
            "5",
            "1h",
            "Time: 12:00:00PM",
            "Time: 1:00:00\nMay 21\nWind: calm\nLight: bright\nAir: clear\nPM",
            artifact_report=self.exact_endpoint_artifact_report(
                start_size=100,
                end_size=100,
            ),
            allow_meridiem_ambiguity=True,
        )

        self.assertFalse(report["meridiem_ambiguity_confirmed"])
        self.assertEqual(report["verdict"], "yellow_wait_elapsed_or_menu_not_fully_proven")

    def test_does_not_attach_split_meridiem_across_unrelated_hud_prose(self) -> None:
        parsed = extract_clock_or_turn_evidence({
            "text": "Time: 1:00:00\nYou hear a noise nearby.\nPM",
        })
        distant = extract_clock_or_turn_evidence({
            "text": "Time: 1:00:00" + ( "\n" * 12 ) + "PM",
        })

        self.assertEqual(
            parsed["clock_matches"][0]["seconds_since_midnight"],
            60 * 60,
        )
        self.assertEqual(
            distant["clock_matches"][0]["seconds_since_midnight"],
            60 * 60,
        )

    def test_rejects_incorrect_turn_delta(self) -> None:
        report = self.classify("4", "30m", "turn 100", "turn 101")

        self.assertEqual(report["verdict"], "yellow_wait_elapsed_or_menu_not_fully_proven")
        self.assertIn("turn_delta_does_not_match_expected_duration", report["issues"])

    def test_poll_excludes_artifacts_before_post_choice_baseline(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            artifact_log = run_dir / "debug.log"
            artifact_log.write_text("endpoint\n", encoding="utf-8")
            post_choice_baseline = artifact_log.stat().st_size

            report = poll_wait_artifact_completion(
                artifact_log,
                run_dir,
                "wait_contract",
                post_choice_baseline,
                ["endpoint"],
                timeout_seconds=0.01,
                poll_seconds=0.001,
                filter_debug_noise=False,
            )

        self.assertEqual(report["status"], "timed_out")
        self.assertEqual(report["match"]["missing_patterns"], ["endpoint"])

    def test_poll_does_not_combine_quiet_state_and_clock_from_different_scans(self) -> None:
        expected = (
            "bandit_live_world signal scan: signal_packet=no "
            "kind=smoke/fire/light now_minutes=10050"
        )
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            artifact_log = run_dir / "debug.log"
            artifact_log.write_text("baseline\n", encoding="utf-8")
            post_choice_baseline = artifact_log.stat().st_size
            with artifact_log.open("a", encoding="utf-8") as stream:
                stream.write(
                    "bandit_live_world signal scan: signal_packet=no "
                    "kind=smoke/fire/light now_minutes=10049\n"
                    "bandit_live_world signal scan: signal_packet=yes "
                    "kind=smoke/fire/light now_minutes=10050\n"
                )

            report = poll_wait_artifact_completion(
                artifact_log,
                run_dir,
                "wait_contract",
                post_choice_baseline,
                [expected],
                timeout_seconds=0.01,
                poll_seconds=0.001,
                filter_debug_noise=False,
            )

        self.assertEqual(report["status"], "timed_out")
        self.assertEqual(report["match"]["missing_patterns"], [expected])

    def test_poll_requires_relative_authoritative_wait_elapsed_minutes(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            artifact_log = run_dir / "debug.log"
            artifact_log.write_text("now_minutes=100\n", encoding="utf-8")
            post_choice_baseline = artifact_log.stat().st_size
            with artifact_log.open("a", encoding="utf-8") as stream:
                stream.write("bandit_live_world perf: sites=1\nnow_minutes=460\n")

            report = poll_wait_artifact_completion(
                artifact_log,
                run_dir,
                "wait_contract",
                post_choice_baseline,
                ["bandit_live_world perf:"],
                timeout_seconds=0.01,
                poll_seconds=0.001,
                filter_debug_noise=False,
                baseline_now_minutes=100,
                minimum_elapsed_minutes=360,
            )

        self.assertEqual(report["status"], "matched")
        self.assertEqual(
            report["match"]["artifact_now_minutes"],
            {
                "baseline": 100,
                "latest": 460,
                "observed_elapsed": 360,
                "minimum_elapsed": 360,
                "matched": True,
            },
        )

    def test_poll_rejects_early_authoritative_wait_marker(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            artifact_log = run_dir / "debug.log"
            artifact_log.write_text(
                "now_minutes=100\nbandit_live_world perf: sites=1\nnow_minutes=459\n",
                encoding="utf-8",
            )
            start_size = len("now_minutes=100\n".encode("utf-8"))
            report = poll_wait_artifact_completion(
                artifact_log,
                run_dir,
                "wait_contract",
                start_size,
                ["bandit_live_world perf:"],
                timeout_seconds=0.01,
                poll_seconds=0.001,
                filter_debug_noise=False,
                baseline_now_minutes=100,
                minimum_elapsed_minutes=360,
            )

        self.assertEqual(report["status"], "timed_out")
        self.assertIn("now_minutes_delta>=360", report["match"]["missing_patterns"])

    def test_poll_observes_completion_written_at_timeout_boundary(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            artifact_log = run_dir / "debug.log"
            artifact_log.write_text("before\n", encoding="utf-8")
            start_size = artifact_log.stat().st_size
            clock_values = iter((0.0, 1.0, 1.0))
            wrote_boundary_artifact = False

            def monotonic() -> float:
                nonlocal wrote_boundary_artifact
                value = next(clock_values)
                if value >= 1.0 and not wrote_boundary_artifact:
                    with artifact_log.open("a", encoding="utf-8") as stream:
                        stream.write("endpoint\n")
                    wrote_boundary_artifact = True
                return value

            with mock.patch("startup_harness.time.monotonic", side_effect=monotonic):
                report = poll_wait_artifact_completion(
                    artifact_log,
                    run_dir,
                    "wait_contract",
                    start_size,
                    ["endpoint"],
                    timeout_seconds=1.0,
                    poll_seconds=0.001,
                    filter_debug_noise=False,
                )

        self.assertEqual(report["status"], "matched")
        self.assertEqual(report["attempts"], 2)
        self.assertEqual(report["match"]["missing_patterns"], [])

    def test_latest_now_minutes_marker_respects_end_offset(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            artifact_log = Path(temp_dir) / "debug.log"
            artifact_log.write_text("now_minutes=100\n", encoding="utf-8")
            baseline = artifact_log.stat().st_size
            with artifact_log.open("a", encoding="utf-8") as stream:
                stream.write("now_minutes=460\n")

            self.assertEqual(
                latest_now_minutes_marker(artifact_log, end_offset=baseline),
                100,
            )
            self.assertEqual(latest_now_minutes_marker(artifact_log), 460)

    def test_long_wait_captures_relative_elapsed_baseline_before_choice(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            artifact_log = run_dir / "debug.log"
            artifact_log.write_text("now_minutes=100\n", encoding="utf-8")

            def press(_pid: int, keys: List[str], **_kwargs: Any) -> None:
                if keys == ["5"]:
                    with artifact_log.open("a", encoding="utf-8") as stream:
                        stream.write("bandit_live_world perf: sites=1\nnow_minutes=160\n")

            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch(
                    "startup_harness.capture_screen_text_artifact",
                    return_value={"ok": True, "text": "Wait a while: 1 hour"},
                ),
                mock.patch("startup_harness.peekaboo_press_sequence", side_effect=press),
                mock.patch("startup_harness.time.sleep"),
            ):
                report = execute_long_wait_action(
                    42,
                    run_dir,
                    "wait_contract",
                    {
                        "choice_key": "5",
                        "expected_duration": "1h",
                        "completion_artifact_timeout_seconds": 0.01,
                        "completion_artifact_poll_seconds": 0.001,
                        "artifact_state_patterns": ["bandit_live_world perf:"],
                        "minimum_artifact_elapsed_minutes": 60,
                    },
                    artifact_log=artifact_log,
                )

        self.assertEqual(report["completion_artifact_baseline_now_minutes"], 100)
        self.assertEqual(report["completion_artifact_poll"]["status"], "matched")
        self.assertEqual(
            report["completion_artifact_poll"]["match"]["artifact_now_minutes"][
                "observed_elapsed"
            ],
            60,
        )

    def test_active_eoc_popup_trace_round_trips_escaped_multiline_text(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            trace_log = Path(temp_dir) / "debug.log"
            trace_log.write_text("before\n", encoding="utf-8")
            start_offset = trace_log.stat().st_size
            with trace_log.open("a", encoding="utf-8") as stream:
                stream.write(
                    'openclaw_harness_ui_trace: component=eoc_popup event=open '
                    'message="You have a vague feeling\\nof being \\"watched\\"." '
                    'truncated=no popup_flag=0\n'
                )

            active = read_active_eoc_popup_trace(trace_log, start_offset)
            self.assertIsNotNone(active)
            self.assertEqual(
                active["message"],
                'You have a vague feeling\nof being "watched".',
            )

            with trace_log.open("a", encoding="utf-8") as stream:
                stream.write(
                    'openclaw_harness_ui_trace: component=eoc_popup event=return '
                    'message="You have a vague feeling\\nof being \\"watched\\"." '
                    'truncated=no popup_flag=0\n'
                )
            self.assertIsNone(read_active_eoc_popup_trace(trace_log, start_offset))

    def test_active_activity_query_trace_closes_on_returned_action(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            trace_log = Path(temp_dir) / "debug.log"
            trace_log.write_text("before\n", encoding="utf-8")
            start_offset = trace_log.stat().st_size
            open_line = (
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=open type=eoc text="" truncated=no action=none\n'
            )
            with trace_log.open("a", encoding="utf-8") as stream:
                stream.write(open_line)

            active = read_active_activity_query_trace(trace_log, start_offset)
            self.assertIsNotNone(active)
            self.assertEqual(active["type"], "eoc")
            self.assertEqual(active["action"], "none")

            with trace_log.open("a", encoding="utf-8") as stream:
                stream.write(open_line.replace("event=open", "event=return").replace(
                    "action=none", "action=IGNORE"
                ))
            self.assertIsNone(read_active_activity_query_trace(trace_log, start_offset))

    def test_structured_eoc_then_partial_activity_query_acknowledges_once_each(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            trace_log = run_dir / "debug.log"
            trace_log.write_text("before\n", encoding="utf-8")
            start_offset = trace_log.stat().st_size
            popup_open = (
                'openclaw_harness_ui_trace: component=eoc_popup event=open '
                'message="You have a vague feeling of being watched." '
                'truncated=no popup_flag=0\n'
            )
            query_open = (
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=open type=eoc text="" truncated=no action=none\n'
            )
            with trace_log.open("a", encoding="utf-8") as stream:
                stream.write(popup_open)

            screens = iter((
                {"ok": True, "text": ""},
                {"ok": True, "text": "Open Manager"},
                {"ok": True, "text": ""},
            ))

            def press_and_return(_pid: int, keys: List[str], **_kwargs: Any) -> None:
                with trace_log.open("a", encoding="utf-8") as stream:
                    if keys == ["space"]:
                        stream.write(popup_open.replace("event=open", "event=return"))
                        stream.write(query_open)
                    elif keys == ["I"]:
                        stream.write(query_open.replace("event=open", "event=return").replace(
                            "action=none", "action=IGNORE"
                        ))
                    else:
                        self.fail(f"unexpected response keys: {keys}")

            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch(
                    "startup_harness.capture_screen_text_artifact",
                    side_effect=lambda *_args, **_kwargs: next(screens),
                ),
                mock.patch(
                    "startup_harness.peekaboo_press_sequence",
                    side_effect=press_and_return,
                ) as press,
                mock.patch("startup_harness.time.sleep"),
            ):
                result = acknowledge_blocking_interruptions(
                    42,
                    run_dir,
                    "wait_contract.structured_sequence",
                    stop_on_unknown=True,
                    structured_popup_trace_log=trace_log,
                    structured_popup_trace_start_offset=start_offset,
                )

        self.assertEqual(result["status"], "clear")
        self.assertEqual(result["acknowledgement_count"], 2)
        self.assertEqual([call.args[1] for call in press.call_args_list], [["space"], ["I"]])
        activity_receipt = result["acknowledgements"][1]["classification"]
        self.assertEqual(
            activity_receipt["provenance"],
            "structured_activity_distraction_query_trace",
        )
        self.assertEqual(activity_receipt["structured_activity_query_trace"]["type"], "eoc")
        self.assertEqual(activity_receipt["response_action"], "IGNORE")

    def test_structured_activity_query_malformed_truncated_unknown_and_stale_fail_closed(self) -> None:
        cases = (
            (
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=open type=eoc text=not-json truncated=no action=none\n',
                None,
            ),
            (
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=open type=eoc text="cut" truncated=yes action=none\n',
                "blocked_unknown_prompt",
            ),
            (
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=open type=unknown_future text="" truncated=no action=none\n',
                "blocked_unknown_prompt",
            ),
        )
        for trace_line, expected_status in cases:
            with self.subTest(trace_line=trace_line), tempfile.TemporaryDirectory() as temp_dir:
                run_dir = Path(temp_dir)
                trace_log = run_dir / "debug.log"
                trace_log.write_text(trace_line, encoding="utf-8")
                with (
                    mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                    mock.patch(
                        "startup_harness.capture_screen_text_artifact",
                        return_value={"ok": True, "text": ""},
                    ),
                    mock.patch("startup_harness.peekaboo_press_sequence") as press,
                ):
                    result = acknowledge_blocking_interruptions(
                        42,
                        run_dir,
                        "wait_contract.bad_activity_query",
                        stop_on_unknown=True,
                        structured_popup_trace_log=trace_log,
                    )
                self.assertEqual(result["status"], expected_status or "clear")
                press.assert_not_called()

        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            trace_log = run_dir / "debug.log"
            trace_log.write_text(
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=open type=eoc text="" truncated=no action=none\n',
                encoding="utf-8",
            )
            stale_offset = trace_log.stat().st_size
            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch(
                    "startup_harness.capture_screen_text_artifact",
                    return_value={"ok": True, "text": ""},
                ),
                mock.patch("startup_harness.peekaboo_press_sequence") as press,
            ):
                result = acknowledge_blocking_interruptions(
                    42,
                    run_dir,
                    "wait_contract.stale_activity_query",
                    stop_on_unknown=True,
                    structured_popup_trace_log=trace_log,
                    structured_popup_trace_start_offset=stale_offset,
                )
            self.assertEqual(result["status"], "clear")
            press.assert_not_called()

    def test_structured_activity_query_does_not_override_unrelated_unknown_ocr(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            trace_log = run_dir / "debug.log"
            trace_log.write_text(
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=open type=eoc text="" truncated=no action=none\n',
                encoding="utf-8",
            )
            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch(
                    "startup_harness.capture_screen_text_artifact",
                    return_value={"ok": True, "text": "Apply changes? (y/n)"},
                ),
                mock.patch("startup_harness.peekaboo_press_sequence") as press,
            ):
                result = acknowledge_blocking_interruptions(
                    42,
                    run_dir,
                    "wait_contract.unrelated_unknown",
                    stop_on_unknown=True,
                    structured_popup_trace_log=trace_log,
                )
        self.assertEqual(result["status"], "blocked_unknown_prompt")
        self.assertEqual(result["final_classification"]["classification"], "unhandled_blocking_menu")
        press.assert_not_called()

    def test_structured_shadow_popup_acknowledges_once_without_ocr_text(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            trace_log = run_dir / "debug.log"
            trace_log.write_text("before\n", encoding="utf-8")
            start_offset = trace_log.stat().st_size
            trace_line = (
                'openclaw_harness_ui_trace: component=eoc_popup event=open '
                'message="You have a vague feeling of being watched.\\nStay alert." '
                'truncated=no popup_flag=0\n'
            )
            trace_log.write_text("before\n" + trace_line, encoding="utf-8")

            def press_and_return(_pid: int, keys: List[str], **_kwargs: Any) -> None:
                self.assertEqual(keys, ["space"])
                with trace_log.open("a", encoding="utf-8") as stream:
                    stream.write(trace_line.replace("event=open", "event=return"))

            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch(
                    "startup_harness.capture_screen_text_artifact",
                    return_value={"ok": True, "text": ""},
                ),
                mock.patch("startup_harness.peekaboo_press_sequence", side_effect=press_and_return) as press,
                mock.patch("startup_harness.time.sleep"),
            ):
                result = acknowledge_blocking_interruptions(
                    42,
                    run_dir,
                    "wait_contract.interruption",
                    stop_on_unknown=True,
                    structured_popup_trace_log=trace_log,
                    structured_popup_trace_start_offset=start_offset,
                )

        self.assertEqual(result["status"], "clear")
        self.assertEqual(result["acknowledgement_count"], 1)
        self.assertEqual(
            result["acknowledgements"][0]["classification"]["provenance"],
            "structured_eoc_popup_trace",
        )
        press.assert_called_once()

    def test_returned_structured_shadow_popup_text_is_suppressed_without_input(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            trace_log = run_dir / "debug.log"
            trace_log.write_text("before current run\n", encoding="utf-8")
            run_start_offset = trace_log.stat().st_size
            message = "You have a vague feeling of being watched."
            open_line = (
                "openclaw_harness_ui_trace: component=eoc_popup event=open "
                f"message={json.dumps(message)} truncated=no popup_flag=0\n"
            )
            with trace_log.open("a", encoding="utf-8") as stream:
                stream.write(open_line)
                stream.write(open_line.replace("event=open", "event=return"))

            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch(
                    "startup_harness.capture_screen_text_artifact",
                    return_value={
                        "ok": True,
                        "text": f"Weather: Clear\n{message}\nMove: 100",
                    },
                ),
                mock.patch("startup_harness.peekaboo_press_sequence") as press,
            ):
                result = acknowledge_blocking_interruptions(
                    42,
                    run_dir,
                    "completed_editor_interaction",
                    stop_on_unknown=True,
                    suppress_inactive_structured_shadow_warning=True,
                    structured_popup_trace_log=trace_log,
                    structured_popup_trace_start_offset=run_start_offset,
                )

        self.assertEqual(result["status"], "clear")
        self.assertEqual(result["acknowledgement_count"], 0)
        self.assertEqual(
            result["final_classification"]["classification"],
            "inactive_structured_popup_text_retained_on_hud",
        )
        self.assertEqual(
            result["final_classification"]["provenance"],
            "bounded_structured_eoc_popup_trace",
        )
        self.assertEqual(
            result["final_classification"]["structured_popup_trace_start_offset"],
            run_start_offset,
        )
        press.assert_not_called()

    def test_active_structured_shadow_popup_is_acknowledged_before_retained_text_suppression(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            trace_log = run_dir / "debug.log"
            trace_log.write_text("before current run\n", encoding="utf-8")
            run_start_offset = trace_log.stat().st_size
            message = "You have a vague feeling of being watched."
            open_line = (
                "openclaw_harness_ui_trace: component=eoc_popup event=open "
                f"message={json.dumps(message)} truncated=no popup_flag=0\n"
            )
            with trace_log.open("a", encoding="utf-8") as stream:
                stream.write(open_line)

            def press_and_return(_pid: int, keys: List[str], **_kwargs: Any) -> None:
                self.assertEqual(keys, ["space"])
                with trace_log.open("a", encoding="utf-8") as stream:
                    stream.write(open_line.replace("event=open", "event=return"))

            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch(
                    "startup_harness.capture_screen_text_artifact",
                    return_value={"ok": True, "text": message},
                ),
                mock.patch(
                    "startup_harness.peekaboo_press_sequence",
                    side_effect=press_and_return,
                ) as press,
                mock.patch("startup_harness.time.sleep"),
            ):
                result = acknowledge_blocking_interruptions(
                    42,
                    run_dir,
                    "completed_editor_interaction",
                    stop_on_unknown=True,
                    suppress_inactive_structured_shadow_warning=True,
                    structured_popup_trace_log=trace_log,
                    structured_popup_trace_start_offset=run_start_offset,
                )

        self.assertEqual(result["status"], "clear")
        self.assertEqual(result["acknowledgement_count"], 1)
        self.assertEqual(
            result["acknowledgements"][0]["classification"]["provenance"],
            "structured_eoc_popup_trace",
        )
        self.assertEqual(
            result["final_classification"]["classification"],
            "inactive_structured_popup_text_retained_on_hud",
        )
        press.assert_called_once()

    def test_inactive_structured_shadow_suppression_keeps_other_prompts_fail_closed(self) -> None:
        cases = (
            ("Save and quit?\nThe wind sure is howling tonight.", "blocked_unsafe_prompt"),
            ("SAFE MODE\nThe wind sure is howling tonight.", "blocked_unknown_prompt"),
            ("Apply changes? (y/n)\nThe wind sure is howling tonight.", "blocked_unknown_prompt"),
        )
        for body, expected_status in cases:
            with self.subTest(body=body), tempfile.TemporaryDirectory() as temp_dir:
                run_dir = Path(temp_dir)
                trace_log = run_dir / "debug.log"
                trace_log.write_text("current run has no active EOC popup\n", encoding="utf-8")
                with (
                    mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                    mock.patch(
                        "startup_harness.capture_screen_text_artifact",
                        return_value={"ok": True, "text": body},
                    ),
                    mock.patch("startup_harness.peekaboo_press_sequence") as press,
                ):
                    result = acknowledge_blocking_interruptions(
                        42,
                        run_dir,
                        "completed_editor_interaction",
                        stop_on_unknown=True,
                        suppress_inactive_structured_shadow_warning=True,
                        structured_popup_trace_log=trace_log,
                    )

                self.assertEqual(result["status"], expected_status)
                self.assertEqual(result["acknowledgement_count"], 0)
                press.assert_not_called()

    def test_structured_lifeless_grass_popup_resolves_partial_ocr(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            trace_log = run_dir / "debug.log"
            message = (
                "You suddenly realize this area seems almost devoid of life. The few bits of "
                "blackened grass you can see are barely recognizable as grass, even discounting "
                "the night. Sure the zombies might be tearing people apart, but what happened to "
                "the grass? What is the grass coming to life to eat people too? You snort at the "
                "idea, but it doesn't feel safe out here."
            )
            trace_line = (
                "openclaw_harness_ui_trace: component=eoc_popup event=open "
                f"message={json.dumps(message)} truncated=no popup_flag=0\n"
            )
            trace_log.write_text(trace_line, encoding="utf-8")

            def press_and_return(_pid: int, keys: List[str], **_kwargs: Any) -> None:
                self.assertEqual(keys, ["space"])
                with trace_log.open("a", encoding="utf-8") as stream:
                    stream.write(trace_line.replace("event=open", "event=return"))

            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch(
                    "startup_harness.capture_screen_text_artifact",
                    return_value={
                        "ok": True,
                        "text": (
                            "Press | to interrupt waiting\n"
                            "This area seems almost devoid of life."
                        ),
                    },
                ),
                mock.patch("startup_harness.peekaboo_press_sequence", side_effect=press_and_return) as press,
                mock.patch("startup_harness.time.sleep"),
            ):
                result = acknowledge_blocking_interruptions(
                    42,
                    run_dir,
                    "wait_contract.lifeless_grass_partial_ocr",
                    stop_on_unknown=True,
                    structured_popup_trace_log=trace_log,
                )

        self.assertEqual(result["status"], "clear")
        self.assertEqual(result["acknowledgement_count"], 1)
        self.assertEqual(
            result["acknowledgements"][0]["classification"]["provenance"],
            "structured_eoc_popup_trace",
        )
        press.assert_called_once()

    def test_structured_shadow_popup_uses_complete_open_in_bounded_noisy_tail(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            trace_log = run_dir / "debug.log"
            trace_log.write_bytes(b"x" * (EOC_POPUP_TRACE_READ_CAP_BYTES + 1024))
            with trace_log.open("a", encoding="utf-8") as stream:
                stream.write(
                    '\nopenclaw_harness_ui_trace: component=eoc_popup event=open '
                    'message="You have a vague feeling of being watched." '
                    'truncated=no popup_flag=0\n'
                )

            def press_and_return(_pid: int, keys: List[str], **_kwargs: Any) -> None:
                self.assertEqual(keys, ["space"])
                with trace_log.open("a", encoding="utf-8") as stream:
                    stream.write(
                        'openclaw_harness_ui_trace: component=eoc_popup event=return '
                        'message="You have a vague feeling of being watched." '
                        'truncated=no popup_flag=0\n'
                    )

            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch(
                    "startup_harness.capture_screen_text_artifact",
                    return_value={"ok": True, "text": "Press | to interrupt waiting 55%"},
                ),
                mock.patch("startup_harness.peekaboo_press_sequence", side_effect=press_and_return) as press,
                mock.patch("startup_harness.time.sleep"),
            ):
                result = acknowledge_blocking_interruptions(
                    42,
                    run_dir,
                    "wait_contract.noisy_tail",
                    stop_on_unknown=True,
                    structured_popup_trace_log=trace_log,
                    structured_popup_trace_start_offset=0,
                )

        self.assertEqual(result["status"], "clear")
        self.assertEqual(result["acknowledgement_count"], 1)
        self.assertTrue(
            result["acknowledgements"][0]["classification"]
            ["structured_popup_trace"]["read_truncated"]
        )
        press.assert_called_once()

    def test_structured_unknown_and_portal_popups_remain_fail_closed(self) -> None:
        messages = (
            "An unfamiliar story popup asks for attention.",
            "There is a buzzing in your senses. A tiny cataclysm has begun.",
        )
        for message in messages:
            with self.subTest(message=message), tempfile.TemporaryDirectory() as temp_dir:
                run_dir = Path(temp_dir)
                trace_log = run_dir / "debug.log"
                trace_log.write_text(
                    'openclaw_harness_ui_trace: component=eoc_popup event=open '
                    f'message={json.dumps(message)} truncated=no popup_flag=0\n',
                    encoding="utf-8",
                )
                with (
                    mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                    mock.patch(
                        "startup_harness.capture_screen_text_artifact",
                        return_value={"ok": True, "text": ""},
                    ),
                    mock.patch("startup_harness.peekaboo_press_sequence") as press,
                ):
                    result = acknowledge_blocking_interruptions(
                        42,
                        run_dir,
                        "wait_contract.interruption",
                        stop_on_unknown=True,
                        structured_popup_trace_log=trace_log,
                    )

                self.assertEqual(result["status"], "blocked_unknown_prompt")
                self.assertEqual(result["acknowledgement_count"], 0)
                self.assertEqual(
                    result["final_classification"]["classification"],
                    "structured_eoc_popup_not_auto_acknowledged",
                )
                press.assert_not_called()

    def test_long_wait_poll_consumes_structured_shadow_popup_trace(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            trace_log = run_dir / "debug.log"
            trace_log.write_text("before\n", encoding="utf-8")
            open_line = (
                'openclaw_harness_ui_trace: component=eoc_popup event=open '
                'message="You have a vague feeling of being watched.\\nStay alert." '
                'truncated=no popup_flag=0\n'
            )

            def press(_pid: int, keys: List[str], **_kwargs: Any) -> None:
                if keys == ["5"]:
                    with trace_log.open("a", encoding="utf-8") as stream:
                        stream.write(open_line)
                elif keys == ["space"]:
                    with trace_log.open("a", encoding="utf-8") as stream:
                        stream.write(open_line.replace("event=open", "event=return"))
                        stream.write("endpoint\n")

            def screen_text(
                _run_dir: Path,
                label: str,
                _capture: Dict[str, Any],
                **_kwargs: Any,
            ) -> Dict[str, Any]:
                if label.endswith(".wait_menu"):
                    return {"ok": True, "text": "Wait a while: 1 hour"}
                if label.endswith(".initial_wait_menu"):
                    return {"ok": True, "text": "Set an alarm or wait"}
                return {"ok": True, "text": ""}

            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch("startup_harness.capture_screen_text_artifact", side_effect=screen_text),
                mock.patch("startup_harness.peekaboo_press_sequence", side_effect=press) as keypress,
                mock.patch("startup_harness.time.sleep"),
            ):
                report = execute_long_wait_action(
                    42,
                    run_dir,
                    "wait_contract",
                    {
                        "choice_key": "5",
                        "expected_duration": "1h",
                        "completion_artifact_timeout_seconds": 1.0,
                        "completion_artifact_poll_seconds": 0.001,
                        "artifact_state_patterns": ["endpoint"],
                    },
                    artifact_log=trace_log,
                    action_trace_log=trace_log,
                )

        self.assertEqual(report["completion_artifact_poll"]["status"], "matched")
        self.assertEqual(report["interruption_handling"]["acknowledgement_count"], 1)
        self.assertEqual(report["interruption_handling"]["response_keys"], ["space"])
        self.assertEqual(
            [call.args[1] for call in keypress.call_args_list].count(["space"]),
            1,
        )

    def test_poll_recovers_shadow_then_activity_prompt_before_endpoint(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            artifact_log = run_dir / "debug.log"
            artifact_log.write_text("before\n", encoding="utf-8")
            start_size = artifact_log.stat().st_size
            screens = iter((
                {
                    "ok": True,
                    "text": (
                        "You suddenly realize this area seems almost devoid of life. "
                        "What happened to the grass?\n"
                        "Waiting 25%\nPress | to interrupt waiting"
                    ),
                },
                {
                    "ok": True,
                    "text": (
                        "Stop waiting? (Case Sensitive)\n"
                        "Open [M]anager\n[I]gnore this distraction and continue"
                    ),
                },
                {
                    "ok": True,
                    "text": (
                        "You suddenly realize this area seems almost devoid of life.\n"
                        "Waiting 31%\nPress | to interrupt waiting"
                    ),
                },
                {
                    "ok": True,
                    "text": (
                        "You suddenly realize this area seems almost devoid of life.\n"
                        "Waiting 43%\nPress | to interrupt waiting"
                    ),
                },
            ))
            handler_calls = 0
            shadow_warning_acknowledged = False

            def handle_interruption() -> Dict[str, Any]:
                nonlocal handler_calls, shadow_warning_acknowledged
                handler_calls += 1
                report = acknowledge_blocking_interruptions(
                    42,
                    run_dir,
                    "wait_contract.interruption",
                    stop_on_unknown=True,
                    suppress_retained_shadow_warning=shadow_warning_acknowledged,
                )
                shadow_warning_acknowledged = shadow_warning_acknowledged or any(
                    str(entry.get("classification", {}).get("classification", ""))
                    == "shadow_warning_wilderness_flavor_popup"
                    for entry in report.get("acknowledgements", [])
                )
                if handler_calls == 2:
                    with artifact_log.open("a", encoding="utf-8") as stream:
                        stream.write("endpoint\n")
                return report

            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch("startup_harness.capture_screen_text_artifact", side_effect=lambda *_args, **_kwargs: next(screens)),
                mock.patch("startup_harness.peekaboo_press_sequence") as press,
                mock.patch("startup_harness.time.sleep"),
            ):
                result = poll_wait_artifact_completion(
                    artifact_log,
                    run_dir,
                    "wait_contract",
                    start_size,
                    ["endpoint"],
                    timeout_seconds=1.0,
                    poll_seconds=0.001,
                    filter_debug_noise=False,
                    interruption_handler=handle_interruption,
                )

        self.assertEqual(result["status"], "matched")
        self.assertFalse(result["aborted"])
        self.assertEqual(result["interruption_handling"]["response_keys"], ["space", "I"])
        self.assertEqual(result["interruption_handling"]["acknowledgement_count"], 2)
        self.assertEqual([call.args[1] for call in press.call_args_list], [["space"], ["I"]])

    def test_retained_shadow_suppression_cannot_override_unknown_confirmation(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch(
                    "startup_harness.capture_screen_text_artifact",
                    return_value={
                        "ok": True,
                        "text": (
                            "You suddenly realize this area seems almost devoid of life.\n"
                            "Apply changes? (y/n)"
                        ),
                    },
                ),
                mock.patch("startup_harness.peekaboo_press_sequence") as press,
            ):
                result = acknowledge_blocking_interruptions(
                    42,
                    run_dir,
                    "wait_contract.interruption",
                    stop_on_unknown=True,
                    suppress_retained_shadow_warning=True,
                )

        self.assertEqual(result["status"], "blocked_unknown_prompt")
        self.assertEqual(
            result["final_classification"]["classification"],
            "unhandled_blocking_menu",
        )
        self.assertEqual(result["acknowledgement_count"], 0)
        press.assert_not_called()

    def test_completed_wait_shadow_message_is_suppressed_without_input(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch(
                    "startup_harness.capture_screen_text_artifact",
                    return_value={
                        "ok": True,
                        "text": (
                            "You finish waiting.\n"
                            "The wind faintly cries into the night."
                        ),
                    },
                ),
                mock.patch("startup_harness.peekaboo_press_sequence") as press,
            ):
                result = acknowledge_blocking_interruptions(
                    42,
                    run_dir,
                    "wait_contract.interruption",
                    stop_on_unknown=True,
                    suppress_retained_shadow_warning=True,
                )

        self.assertEqual(result["status"], "clear")
        self.assertEqual(result["acknowledgement_count"], 0)
        self.assertEqual(
            result["final_classification"]["classification"],
            "acknowledged_prompt_text_retained_on_hud",
        )
        press.assert_not_called()

    def test_poll_aborts_unknown_interruption_without_response_input(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            artifact_log = run_dir / "debug.log"
            artifact_log.write_text("before\n", encoding="utf-8")
            start_size = artifact_log.stat().st_size
            def handle_interruption() -> Dict[str, Any]:
                return acknowledge_blocking_interruptions(
                    42,
                    run_dir,
                    "wait_contract.interruption",
                    stop_on_unknown=True,
                )

            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch(
                    "startup_harness.capture_screen_text_artifact",
                    return_value={"ok": True, "text": "Apply changes? (y/n)"},
                ),
                mock.patch("startup_harness.peekaboo_press_sequence") as press,
            ):
                result = poll_wait_artifact_completion(
                    artifact_log,
                    run_dir,
                    "wait_contract",
                    start_size,
                    ["endpoint"],
                    timeout_seconds=1.0,
                    poll_seconds=0.001,
                    filter_debug_noise=False,
                    interruption_handler=handle_interruption,
                )

        self.assertEqual(result["status"], "aborted_interruption")
        self.assertTrue(result["aborted"])
        self.assertEqual(result["abort_reason"], "blocked_unknown_prompt")
        self.assertEqual(result["interruption_handling"]["response_keys"], [])
        self.assertEqual(result["interruption_handling"]["acknowledgement_count"], 0)
        press.assert_not_called()

    def test_execute_long_wait_aggregates_mid_poll_recovery_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            artifact_log = run_dir / "debug.log"
            artifact_log.write_text("before\n", encoding="utf-8")
            mid_poll_report = {
                "status": "matched",
                "aborted": False,
                "abort_reason": "",
                "match": {"matched": True, "missing_patterns": []},
                "interruption_handling": {
                    "status": "recovered_known_interruptions",
                    "acknowledgement_count": 2,
                    "response_keys": ["space", "I"],
                    "release_blocking": False,
                    "contaminating": False,
                    "reports": [{"status": "clear", "acknowledgement_count": 2}],
                },
            }
            clear_interruption = {
                "status": "clear",
                "acknowledgement_count": 0,
                "acknowledgements": [],
                "release_blocking": False,
                "contaminating": False,
            }

            def screen_text(_run_dir: Path, label: str, _capture: Dict[str, Any], **_kwargs: Any) -> Dict[str, Any]:
                if label.endswith(".wait_menu"):
                    return {"ok": True, "text": "Wait a while: 6 hours"}
                if label.endswith(".initial_wait_menu"):
                    return {"ok": True, "text": "Set an alarm or wait"}
                return {"ok": True, "text": "Time: 4:00:00PM"}

            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch("startup_harness.capture_screen_text_artifact", side_effect=screen_text),
                mock.patch("startup_harness.peekaboo_press_sequence"),
                mock.patch("startup_harness.time.sleep"),
                mock.patch(
                    "startup_harness.poll_wait_artifact_completion",
                    return_value=mid_poll_report,
                ),
                mock.patch(
                    "startup_harness.acknowledge_blocking_interruptions",
                    return_value=clear_interruption,
                ),
            ):
                report = execute_long_wait_action(
                    42,
                    run_dir,
                    "wait_contract",
                    {
                        "choice_key": "8",
                        "expected_duration": "6h",
                        "completion_artifact_timeout_seconds": 1.0,
                        "artifact_state_patterns": ["endpoint"],
                    },
                    artifact_log=artifact_log,
                )

        self.assertEqual(report["interruption_handling"]["status"], "recovered_known_interruptions")
        self.assertEqual(report["interruption_handling"]["acknowledgement_count"], 2)
        self.assertEqual(report["interruption_handling"]["response_keys"], ["space", "I"])
        self.assertFalse(report["stop_after_step"] if "stop_after_step" in report else False)

    def test_execute_real_poll_aborts_unknown_without_response_input(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            artifact_log = run_dir / "debug.log"
            artifact_log.write_text("before\n", encoding="utf-8")

            def screen_text(scan_dir: Path, label: str, _capture: Dict[str, Any], **_kwargs: Any) -> Dict[str, Any]:
                if "interrupt-scan-" in str(scan_dir):
                    return {"ok": True, "text": "Apply changes? (y/n)"}
                if label.endswith(".wait_menu"):
                    return {"ok": True, "text": "Wait a while: 1 hour"}
                if label.endswith(".initial_wait_menu"):
                    return {"ok": True, "text": "Set an alarm or wait"}
                return {"ok": True, "text": "Time: 10:00:00PM"}

            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch("startup_harness.capture_screen_text_artifact", side_effect=screen_text),
                mock.patch("startup_harness.peekaboo_press_sequence") as press,
                mock.patch("startup_harness.time.sleep"),
            ):
                report = execute_long_wait_action(
                    42,
                    run_dir,
                    "wait_contract",
                    {
                        "choice_key": "5",
                        "expected_duration": "1h",
                        "pre_menu_choice_key": "w",
                        "completion_wait_seconds": 0.01,
                        "completion_artifact_timeout_seconds": 1.0,
                        "artifact_state_patterns": ["endpoint"],
                    },
                    artifact_log=artifact_log,
                )

        self.assertTrue(report["stop_after_step"])
        self.assertEqual(report["abort"]["guard"], "completion_artifact_interruption")
        self.assertEqual(report["completion_artifact_poll"]["abort_reason"], "blocked_unknown_prompt")
        self.assertEqual([call.args[1] for call in press.call_args_list], [["|"], ["w"], ["5"]])

    def test_endpoint_always_gets_fail_closed_scan_after_ack_budget_is_exhausted(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            artifact_log = run_dir / "debug.log"
            artifact_log.write_text("before\n", encoding="utf-8")
            mid_poll_report = {
                "status": "matched",
                "aborted": False,
                "abort_reason": "",
                "match": {"matched": True, "missing_patterns": []},
                "interruption_handling": {
                    "status": "recovered_known_interruptions",
                    "acknowledgement_count": 6,
                    "response_keys": ["space", "I", "space", "I", "space", "I"],
                    "release_blocking": False,
                    "contaminating": False,
                    "reports": [],
                },
            }
            blocked_boundary = {
                "status": "blocked_acknowledgement_limit",
                "acknowledgement_count": 0,
                "acknowledgements": [],
                "release_blocking": False,
                "contaminating": False,
            }

            def screen_text(_run_dir: Path, label: str, _capture: Dict[str, Any], **_kwargs: Any) -> Dict[str, Any]:
                if label.endswith(".wait_menu"):
                    return {"ok": True, "text": "Wait a while: 6 hours"}
                if label.endswith(".initial_wait_menu"):
                    return {"ok": True, "text": "Set an alarm or wait"}
                return {"ok": True, "text": "Time: 4:00:00PM"}

            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch("startup_harness.capture_screen_text_artifact", side_effect=screen_text),
                mock.patch("startup_harness.peekaboo_press_sequence"),
                mock.patch("startup_harness.time.sleep"),
                mock.patch(
                    "startup_harness.poll_wait_artifact_completion",
                    return_value=mid_poll_report,
                ),
                mock.patch(
                    "startup_harness.acknowledge_blocking_interruptions",
                    return_value=blocked_boundary,
                ) as boundary_scan,
            ):
                report = execute_long_wait_action(
                    42,
                    run_dir,
                    "wait_contract",
                    {
                        "choice_key": "8",
                        "expected_duration": "6h",
                        "completion_artifact_timeout_seconds": 1.0,
                        "artifact_state_patterns": ["endpoint"],
                        "max_interrupt_responses": 6,
                    },
                    artifact_log=artifact_log,
                )

        boundary_scan.assert_called_once()
        self.assertEqual(boundary_scan.call_args.kwargs["max_acknowledgements"], 0)
        self.assertTrue(report["stop_after_step"])
        self.assertEqual(report["abort"]["status"], "blocked_acknowledgement_limit")

    def test_real_poll_timeout_excludes_menu_artifact_and_prevents_second_wait(self) -> None:
        steps = [
            {
                "kind": "long_wait",
                "label": "first",
                "choice_key": "5",
                "expected_duration": "1h",
                "pre_menu_choice_key": "w",
                "completion_wait_seconds": 0.01,
                "completion_artifact_timeout_seconds": 1.0,
                "artifact_state_patterns": ["endpoint"],
            },
            {"kind": "long_wait", "label": "second", "choice_key": "5"},
        ]
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            artifact_log = run_dir / "debug.log"
            artifact_log.write_text("before\n", encoding="utf-8")

            def screen_text(_run_dir: Path, label: str, _capture: Dict[str, Any], **_kwargs: Any) -> Dict[str, Any]:
                if label.endswith(".wait_menu"):
                    with artifact_log.open("a", encoding="utf-8") as stream:
                        stream.write("endpoint emitted before duration choice\n")
                    return {"ok": True, "text": "Wait a while: 1 hour"}
                if label.endswith(".initial_wait_menu"):
                    return {"ok": True, "text": "Set an alarm or wait"}
                return {"ok": True, "text": "Time: 10:00:00PM"}

            timed_out_poll = {
                "status": "timed_out",
                "attempts": 1,
                "elapsed_seconds": 1.0,
                "timeout_seconds": 1.0,
                "poll_seconds": 2.0,
                "start_size": 0,
                "match": {
                    "matched": False,
                    "patterns": ["endpoint"],
                    "matched_patterns": [],
                    "missing_patterns": ["endpoint"],
                    "artifact_path": "",
                    "source_log": str(artifact_log),
                },
            }
            clear_interruption = {
                "status": "clear",
                "acknowledgement_count": 0,
                "release_blocking": False,
                "contaminating": False,
            }
            with (
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch("startup_harness.capture_screen_text_artifact", side_effect=screen_text),
                mock.patch("startup_harness.peekaboo_press_sequence") as press,
                mock.patch("startup_harness.time.sleep"),
                mock.patch(
                    "startup_harness.acknowledge_blocking_interruptions",
                    return_value=clear_interruption,
                ),
                mock.patch(
                    "startup_harness.poll_wait_artifact_completion",
                    return_value=timed_out_poll,
                ) as poll,
            ):
                reports = execute_probe_steps(
                    42,
                    run_dir,
                    steps,
                    profile="dev-harness",
                    world="McWilliams",
                    artifact_log=artifact_log,
                )

            completion_start_size = poll.call_args.args[3]
            post_menu_size = artifact_log.stat().st_size

        self.assertEqual(completion_start_size, post_menu_size)
        self.assertEqual(len(reports), 1)
        self.assertEqual(reports[0]["label"], "first")
        self.assertTrue(reports[0]["stop_after_step"])
        self.assertEqual(reports[0]["abort"]["guard"], "completion_artifact_timeout")
        self.assertEqual(
            [call.args[1] for call in press.call_args_list],
            [["|"], ["w"], ["5"]],
        )


class FreshEcologyIncidentPairContractTest(unittest.TestCase):
    SCENARIO = "bandit.scout_to_decision_observer_live_mcw"
    COMMIT = "f46f8f45ca"
    BINARY = "cataclysm-tiles"
    TURN = 5249094
    SELECTED_ID = "dispatch/camp/frontier_probe:0/1"
    VALID_PNG = (
        b"\x89PNG\r\n\x1a\n"
        b"\x00\x00\x00\rIHDR"
        b"\x00\x00\x00\x01\x00\x00\x00\x01\x08\x06\x00\x00\x00"
        b"\x00\x00\x00\x00"
    )

    def payload(self, run_identity: str) -> Dict[str, Any]:
        return {
            "schema": "c-aol.ecology.incident",
            "version": 2,
            "identity": {
                "turn": self.TURN,
                "scenario": self.SCENARIO,
                "commit": self.COMMIT + "+SDL3",
                "binary": self.BINARY,
                "run_identity": run_identity,
            },
            "selected": {
                "token": {"canonical_id": self.SELECTED_ID},
                "id": self.SELECTED_ID,
                "provenance": "natural",
            },
            "deltas": {
                "schema": "c-aol.ecology.delta",
                "version": 1,
                "records": [
                    {
                        "entity_id": self.SELECTED_ID,
                        "provenance": "natural",
                    }
                ],
            },
            "interventions": [],
        }

    def write_pair(
        self,
        run_dir: Path,
        *,
        payload: Optional[Dict[str, Any]] = None,
        png: Optional[bytes] = None,
        write_json: bool = True,
        write_png: bool = True,
    ) -> None:
        stem = f"ecology_incident_{self.TURN}"
        if write_json:
            (run_dir / f"{stem}.json").write_text(
                json.dumps(payload if payload is not None else self.payload(run_dir.name)),
                encoding="utf-8",
            )
        if write_png:
            (run_dir / f"{stem}.png").write_bytes(self.VALID_PNG if png is None else png)

    def audit(self, run_dir: Path, baseline_names: Optional[List[str]] = None) -> Dict[str, Any]:
        return audit_fresh_ecology_incident_pair(
            run_dir,
            baseline_names=baseline_names or [],
            expected_scenario=self.SCENARIO,
            expected_commit=self.COMMIT,
            expected_binary=self.BINARY,
        )

    def assert_red(self, report: Dict[str, Any], issue: str) -> None:
        self.assertEqual(report["status"], "required_state_missing")
        self.assertIn(issue, report["missing_required_fields"])

    def test_accepts_one_fresh_identity_bound_natural_pair(self) -> None:
        with tempfile.TemporaryDirectory(prefix="incident_contract_") as temp_dir:
            run_dir = Path(temp_dir)
            self.write_pair(run_dir)
            report = self.audit(run_dir)

        self.assertEqual(report["status"], "required_state_present")
        self.assertEqual(report["observed_selected_id"], self.SELECTED_ID)
        self.assertEqual(report["observed_delta_count"], 1)

    def test_rejects_missing_pair(self) -> None:
        with tempfile.TemporaryDirectory(prefix="incident_contract_") as temp_dir:
            report = self.audit(Path(temp_dir))
        self.assert_red(report, "fresh_incident_pair_missing")

    def test_rejects_stale_pair_recorded_in_prepublication_baseline(self) -> None:
        with tempfile.TemporaryDirectory(prefix="incident_contract_") as temp_dir:
            run_dir = Path(temp_dir)
            self.write_pair(run_dir)
            baseline = ecology_incident_artifact_baseline(run_dir)
            report = self.audit(run_dir, baseline)
        self.assert_red(report, "fresh_incident_pair_missing")

    def test_rejects_orphan_json(self) -> None:
        with tempfile.TemporaryDirectory(prefix="incident_contract_") as temp_dir:
            run_dir = Path(temp_dir)
            self.write_pair(run_dir, write_png=False)
            report = self.audit(run_dir)
        self.assert_red(report, "fresh_incident_png_missing")

    def test_rejects_malformed_json(self) -> None:
        with tempfile.TemporaryDirectory(prefix="incident_contract_") as temp_dir:
            run_dir = Path(temp_dir)
            (run_dir / f"ecology_incident_{self.TURN}.json").write_text("{", encoding="utf-8")
            (run_dir / f"ecology_incident_{self.TURN}.png").write_bytes(self.VALID_PNG)
            report = self.audit(run_dir)
        self.assert_red(report, "incident_json_malformed")

    def test_rejects_wrong_incident_or_delta_schema_version(self) -> None:
        mutations = (
            ("incident_schema", lambda payload: payload.__setitem__("schema", "wrong"), "incident_schema_invalid"),
            ("incident_version", lambda payload: payload.__setitem__("version", 1), "incident_version_invalid"),
            ("delta_schema", lambda payload: payload["deltas"].__setitem__("schema", "wrong"), "incident_delta_schema_invalid"),
            ("delta_version", lambda payload: payload["deltas"].__setitem__("version", 2), "incident_delta_schema_invalid"),
        )
        for label, mutate, expected_issue in mutations:
            with self.subTest(label=label), tempfile.TemporaryDirectory(
                prefix="incident_contract_"
            ) as temp_dir:
                run_dir = Path(temp_dir)
                payload = self.payload(run_dir.name)
                mutate(payload)
                self.write_pair(run_dir, payload=payload)
                report = self.audit(run_dir)
                self.assert_red(report, expected_issue)

    def test_rejects_wrong_scenario(self) -> None:
        with tempfile.TemporaryDirectory(prefix="incident_contract_") as temp_dir:
            run_dir = Path(temp_dir)
            payload = self.payload(run_dir.name)
            payload["identity"]["scenario"] = "wrong.scenario"
            self.write_pair(run_dir, payload=payload)
            report = self.audit(run_dir)
        self.assert_red(report, "incident_scenario_mismatch")

    def test_rejects_wrong_run_identity(self) -> None:
        with tempfile.TemporaryDirectory(prefix="incident_contract_") as temp_dir:
            run_dir = Path(temp_dir)
            self.write_pair(run_dir, payload=self.payload("another_run"))
            report = self.audit(run_dir)
        self.assert_red(report, "incident_run_identity_mismatch")

    def test_rejects_wrong_commit(self) -> None:
        with tempfile.TemporaryDirectory(prefix="incident_contract_") as temp_dir:
            run_dir = Path(temp_dir)
            payload = self.payload(run_dir.name)
            payload["identity"]["commit"] = "0000000000+SDL3"
            self.write_pair(run_dir, payload=payload)
            report = self.audit(run_dir)
        self.assert_red(report, "incident_commit_mismatch")

    def test_rejects_wrong_binary(self) -> None:
        with tempfile.TemporaryDirectory(prefix="incident_contract_") as temp_dir:
            run_dir = Path(temp_dir)
            payload = self.payload(run_dir.name)
            payload["identity"]["binary"] = "another-binary"
            self.write_pair(run_dir, payload=payload)
            report = self.audit(run_dir)
        self.assert_red(report, "incident_binary_mismatch")

    def test_rejects_selected_token_mismatch(self) -> None:
        with tempfile.TemporaryDirectory(prefix="incident_contract_") as temp_dir:
            run_dir = Path(temp_dir)
            payload = self.payload(run_dir.name)
            payload["selected"]["token"]["canonical_id"] = "dispatch/other"
            self.write_pair(run_dir, payload=payload)
            report = self.audit(run_dir)
        self.assert_red(report, "incident_selection_token_mismatch")

    def test_rejects_non_natural_selection_or_delta_provenance(self) -> None:
        for field in ("selection", "delta"):
            with self.subTest(field=field), tempfile.TemporaryDirectory(
                prefix="incident_contract_"
            ) as temp_dir:
                run_dir = Path(temp_dir)
                payload = self.payload(run_dir.name)
                if field == "selection":
                    payload["selected"]["provenance"] = "debug_intervention"
                    expected_issue = "incident_selection_provenance_not_natural"
                else:
                    payload["deltas"]["records"][0]["provenance"] = "debug_intervention"
                    expected_issue = "incident_delta_provenance_not_natural"
                self.write_pair(run_dir, payload=payload)
                report = self.audit(run_dir)
                self.assert_red(report, expected_issue)

    def test_rejects_intervention_bearing_incident(self) -> None:
        with tempfile.TemporaryDirectory(prefix="incident_contract_") as temp_dir:
            run_dir = Path(temp_dir)
            payload = self.payload(run_dir.name)
            payload["interventions"] = [{"debug_intervention": True}]
            self.write_pair(run_dir, payload=payload)
            report = self.audit(run_dir)
        self.assert_red(report, "incident_interventions_present")

    def test_rejects_invalid_or_empty_png(self) -> None:
        for png, expected_issue in ((b"x" * 33, "png_signature_invalid"), (b"", "png_truncated")):
            with self.subTest(png=png), tempfile.TemporaryDirectory(
                prefix="incident_contract_"
            ) as temp_dir:
                run_dir = Path(temp_dir)
                self.write_pair(run_dir, png=png)
                report = self.audit(run_dir)
                self.assert_red(report, expected_issue)

    def test_failed_classifier_aborts_before_later_credit_step(self) -> None:
        steps = [
            {
                "kind": "press",
                "label": "record_post_window_ecology_incident",
                "keys": ["R"],
                "record_ecology_incident_baseline": True,
            },
            {
                "kind": "audit_fresh_ecology_incident_pair",
                "label": "audit_fresh_post_window_ecology_incident_pair",
                "publication_step_label": "record_post_window_ecology_incident",
                "abort_on_metadata_failure": True,
            },
            {
                "kind": "press",
                "label": "later_return_report_decision_credit",
                "keys": ["escape"],
            },
        ]
        with (
            tempfile.TemporaryDirectory(prefix="incident_contract_") as temp_dir,
            mock.patch("startup_harness.peekaboo_press_sequence") as press,
        ):
            reports = execute_probe_steps(
                42,
                Path(temp_dir),
                steps,
                profile="dev-harness",
                world="McWilliams",
                scenario_identity=self.SCENARIO,
                runtime_commit=self.COMMIT,
                runtime_binary=self.BINARY,
            )

        self.assertEqual([report["label"] for report in reports], [step["label"] for step in steps[:2]])
        self.assertEqual(reports[-1]["abort"]["guard"], "metadata")
        press.assert_called_once_with(42, ["R"], delay_ms=200)


class MapEditorFieldIntensityContractTest(unittest.TestCase):
    @staticmethod
    def execute_field_step(step: Dict[str, Any]) -> tuple[Dict[str, Any], mock.Mock]:
        clear_interruption = {
            "status": "clear",
            "acknowledgement_count": 0,
            "release_blocking": False,
            "contaminating": False,
        }
        input_calls = mock.Mock()
        with (
            tempfile.TemporaryDirectory() as temp_dir,
            mock.patch("startup_harness.run_debug_menu_shortcut_path"),
            mock.patch("startup_harness.apply_uilist_filter"),
            mock.patch("startup_harness.peekaboo_hotkey", input_calls.hotkey),
            mock.patch("startup_harness.peekaboo_press_sequence", input_calls.press),
            mock.patch("startup_harness.time.sleep"),
            mock.patch(
                "startup_harness.acknowledge_blocking_interruptions",
                return_value=clear_interruption,
            ),
        ):
            reports = execute_probe_steps(
                42,
                Path(temp_dir),
                [step],
                profile="dev-harness",
                world="McWilliams",
            )
        return reports[0], input_calls

    def test_default_intensity_preserves_existing_selection_path(self) -> None:
        report, input_calls = self.execute_field_step({
            "kind": "debug_map_editor_place_field",
            "label": "place_smoke",
            "field_query": "fd_smoke",
        })

        self.assertEqual(report["field_intensity"], 1)
        self.assertEqual(report["intensity_selection_path"], ["enter"])
        self.assertEqual(
            report["selection_path"],
            ["0", "0", "0", "e", "/", "fd_smoke", "enter", "enter", "enter"],
        )
        self.assertNotIn(
            "down",
            [key for call in input_calls.press.call_args_list for key in call.args[1]],
        )

    def test_intensity_three_moves_through_real_menu_and_records_path(self) -> None:
        report, input_calls = self.execute_field_step({
            "kind": "debug_map_editor_place_field",
            "label": "place_fire",
            "field_query": "fd_fire",
            "field_intensity": 3,
            "target_keys": ["right"],
        })

        self.assertEqual(report["field_intensity"], 3)
        self.assertEqual(report["intensity_selection_path"], ["down", "down", "enter"])
        self.assertEqual(
            report["selection_path"],
            [
                "0", "0", "0", "right", "e", "/", "fd_fire", "enter", "down", "down",
                "enter", "enter",
            ],
        )
        self.assertIn(
            ["down", "down"],
            [call.args[1] for call in input_calls.press.call_args_list],
        )

    def test_completed_field_interaction_uses_run_bounded_returned_eoc_lifecycle(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            trace_log = run_dir / "probe.feature_debug.log"
            trace_log.write_text("startup trace before run boundary\n", encoding="utf-8")
            run_start_offset = trace_log.stat().st_size
            message = "You have a vague feeling of being watched."
            open_line = (
                "openclaw_harness_ui_trace: component=eoc_popup event=open "
                f"message={json.dumps(message)} truncated=no popup_flag=0\n"
            )
            with trace_log.open("a", encoding="utf-8") as stream:
                stream.write(open_line)
                stream.write(open_line.replace("event=open", "event=return"))

            with (
                mock.patch("startup_harness.debug_map_editor_place_field") as place_field,
                mock.patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}),
                mock.patch(
                    "startup_harness.capture_screen_text_artifact",
                    return_value={
                        "ok": True,
                        "text": f"Weather: Clear\n{message}\nMove: 100",
                    },
                ),
                mock.patch("startup_harness.peekaboo_press_sequence") as press,
            ):
                reports = execute_probe_steps(
                    42,
                    run_dir,
                    [{
                        "kind": "debug_map_editor_place_field",
                        "label": "stage_physical_fire_on_target_omt",
                        "field_query": "fd_fire",
                        "field_intensity": 3,
                    }],
                    profile="dev-harness",
                    world="McWilliams",
                    action_trace_log=trace_log,
                    action_trace_baseline=run_start_offset,
                )

        self.assertEqual(len(reports), 1)
        self.assertNotIn("abort", reports[0])
        interruption = reports[0]["interruption_handling"]
        self.assertEqual(interruption["status"], "clear")
        self.assertEqual(interruption["acknowledgement_count"], 0)
        self.assertEqual(
            interruption["final_classification"]["classification"],
            "inactive_structured_popup_text_retained_on_hud",
        )
        self.assertEqual(
            interruption["final_classification"]["structured_popup_trace_start_offset"],
            run_start_offset,
        )
        place_field.assert_called_once()
        press.assert_not_called()

    def test_centers_cursor_before_field_target_keys(self) -> None:
        report, input_calls = self.execute_field_step({
            "kind": "debug_map_editor_place_field",
            "label": "place_smoke",
            "field_query": "fd_smoke",
            "target_keys": ["right", "right"],
        })

        self.assertEqual(
            input_calls.mock_calls[:4],
            [
                mock.call.hotkey(42, "0", hold_ms=200),
                mock.call.hotkey(42, "0", hold_ms=200),
                mock.call.hotkey(42, "0", hold_ms=200),
                mock.call.press(42, ["right", "right"], delay_ms=200),
            ],
        )
        self.assertEqual(report["selection_path"][:5], ["0", "0", "0", "right", "right"])

    def test_invalid_intensity_is_rejected_before_input(self) -> None:
        invalid_values = (0, 4, "3", 3.0, True)
        for value in invalid_values:
            with self.subTest(value=value):
                with mock.patch("startup_harness.debug_map_editor_place_field") as place_field:
                    with self.assertRaisesRegex(SystemExit, "field_intensity must be"):
                        execute_probe_steps(
                            42,
                            Path("unused"),
                            [{
                                "kind": "debug_map_editor_place_field",
                                "label": "invalid_field_intensity",
                                "field_query": "fd_fire",
                                "field_intensity": value,
                            }],
                            profile="dev-harness",
                            world="McWilliams",
                        )
                place_field.assert_not_called()

        with self.assertRaisesRegex(SystemExit, "field_intensity must be from 1 to 3"):
            debug_map_editor_place_field(
                42,
                field_query="fd_fire",
                field_intensity=0,
            )


class MapEditorItemPlacementContractTest(unittest.TestCase):
    def test_routes_one_item_to_target_tile_and_records_exact_path(self) -> None:
        clear_interruption = {
            "status": "clear",
            "acknowledgement_count": 0,
            "release_blocking": False,
            "contaminating": False,
        }
        input_calls = mock.Mock()
        with (
            tempfile.TemporaryDirectory() as temp_dir,
            mock.patch("startup_harness.run_debug_menu_shortcut_path") as debug_path,
            mock.patch("startup_harness.apply_uilist_filter") as apply_filter,
            mock.patch("startup_harness.peekaboo_hotkey", input_calls.hotkey),
            mock.patch("startup_harness.peekaboo_press_sequence", input_calls.press),
            mock.patch("startup_harness.time.sleep"),
            mock.patch(
                "startup_harness.acknowledge_blocking_interruptions",
                return_value=clear_interruption,
            ),
        ):
            reports = execute_probe_steps(
                42,
                Path(temp_dir),
                [{
                    "kind": "debug_map_editor_place_item",
                    "label": "place_active_c4",
                    "item_query": "  c4armed  ",
                    "target_keys": ["right", "right"],
                    "delay_ms": 123,
                    "type_delay_ms": 7,
                    "menu_settle_seconds": 0.1,
                    "prompt_settle_seconds": 0.2,
                }],
                profile="dev-harness",
                world="McWilliams",
            )

        report = reports[0]
        debug_path.assert_called_once_with(
            42,
            ["m", "M"],
            delay_ms=123,
            menu_settle_seconds=0.1,
        )
        apply_filter.assert_called_once_with(
            42,
            "c4armed",
            delay_ms=123,
            type_delay_ms=7,
            settle_seconds=0.2,
        )
        self.assertEqual(
            input_calls.mock_calls,
            [
                mock.call.hotkey(42, "0", hold_ms=123),
                mock.call.hotkey(42, "0", hold_ms=123),
                mock.call.hotkey(42, "0", hold_ms=123),
                mock.call.press(42, ["right", "right"], delay_ms=123),
                mock.call.press(42, ["i"], delay_ms=123),
                mock.call.press(42, ["a"], delay_ms=123),
                mock.call.press(42, ["enter"], delay_ms=123),
                mock.call.press(42, ["esc"], delay_ms=123),
                mock.call.press(42, ["esc"], delay_ms=123),
            ],
        )
        self.assertEqual(report["item_query"], "c4armed")
        self.assertEqual(report["target_keys"], ["right", "right"])
        self.assertEqual(report["debug_menu_path"], ["}", "m", "M"])
        self.assertEqual(
            report["selection_path"],
            [
                "0", "0", "0", "right", "right", "i", "a", "/", "c4armed", "enter",
                "enter", "esc", "esc",
            ],
        )
        self.assertEqual(report["spawn_target"], "map_editor_target_tile")

    def test_centers_cursor_before_item_target_keys(self) -> None:
        input_calls = mock.Mock()
        with (
            mock.patch("startup_harness.run_debug_menu_shortcut_path"),
            mock.patch("startup_harness.apply_uilist_filter"),
            mock.patch("startup_harness.peekaboo_hotkey", input_calls.hotkey),
            mock.patch("startup_harness.peekaboo_press_sequence", input_calls.press),
            mock.patch("startup_harness.time.sleep"),
        ):
            debug_map_editor_place_item(
                42,
                item_query="c4armed",
                target_keys=["left", "left"],
            )

        self.assertEqual(
            input_calls.mock_calls[:4],
            [
                mock.call.hotkey(42, "0", hold_ms=200),
                mock.call.hotkey(42, "0", hold_ms=200),
                mock.call.hotkey(42, "0", hold_ms=200),
                mock.call.press(42, ["left", "left"], delay_ms=200),
            ],
        )

    def test_missing_blank_and_wrong_type_queries_fail_before_input(self) -> None:
        invalid_steps = (
            {},
            {"item_query": ""},
            {"item_query": "   "},
            {"item_query": 3},
            {"item_query": ["c4armed"]},
            {"item_query": False},
        )
        for invalid in invalid_steps:
            with self.subTest(invalid=invalid):
                step = {
                    "kind": "debug_map_editor_place_item",
                    "label": "invalid_item_query",
                    **invalid,
                }
                with mock.patch("startup_harness.debug_map_editor_place_item") as place_item:
                    with self.assertRaisesRegex(
                        SystemExit,
                        "needs non-empty string item_query",
                    ):
                        execute_probe_steps(
                            42,
                            Path("unused"),
                            [step],
                            profile="dev-harness",
                            world="McWilliams",
                        )
                place_item.assert_not_called()

        with self.assertRaisesRegex(SystemExit, "needs non-empty string item_query"):
            debug_map_editor_place_item(42, item_query=3)  # type: ignore[arg-type]


class ScenarioStartupProfileContractTest(unittest.TestCase):
    def test_app_switch_focus_fallback_verifies_target_pid_is_active(self) -> None:
        before = {
            "data": {
                "apps": [{"name": "cataclysm-tiles", "pid": 42, "is_active": False}],
            },
        }
        switched = {"success": True}
        after = {
            "data": {
                "apps": [{"name": "cataclysm-tiles", "pid": 42, "is_active": True}],
            },
        }
        with (
            mock.patch("startup_harness.run_json", side_effect=[before, switched, after]),
            mock.patch("startup_harness.peekaboo_command", side_effect=lambda args, **_: list(args)),
            mock.patch("startup_harness.time.sleep") as sleep,
        ):
            result = peekaboo_switch_app_for_pid(42)

        self.assertTrue(result["ok"])
        self.assertEqual(result["app_name"], "cataclysm-tiles")
        self.assertEqual(
            result["switch_command"],
            ["app", "switch", "--to", "cataclysm-tiles", "--json"],
        )
        sleep.assert_called_once_with(0.1)

    def test_startup_focus_retry_preserves_failed_attempt_before_green_result(self) -> None:
        failed = {"ok": False, "returncode": 1, "stderr": "verification failed"}
        green = {"ok": True, "returncode": 0, "stderr": ""}
        with (
            mock.patch(
                "startup_harness.peekaboo_focus_pid",
                side_effect=[failed, green],
            ) as focus,
            mock.patch("startup_harness.time.sleep") as sleep,
        ):
            result = peekaboo_focus_pid_with_retry(42, retry_delay_seconds=0.25)

        self.assertTrue(result["ok"])
        self.assertEqual(result["attempt_count"], 2)
        self.assertEqual(result["attempts"], [failed, green])
        self.assertEqual(focus.call_count, 2)
        sleep.assert_called_once_with(0.25)

    def test_foreign_absolute_python_path_does_not_block_platform_fallback(self) -> None:
        with mock.patch("startup_harness.os.name", "posix"):
            mac_result = resolve_configured_python_command(
                r"C:\Users\josef\openvino_models\openvino_env"
            )
        with mock.patch("startup_harness.os.name", "nt"):
            windows_result = resolve_configured_python_command(
                "/Users/josefhorvath/ollama/api_env311"
            )

        self.assertEqual(mac_result, ([], ""))
        self.assertEqual(windows_result, ([], ""))

    def test_missing_same_platform_python_path_is_not_treated_as_executable(self) -> None:
        with mock.patch("startup_harness.os.name", "posix"):
            posix_result = resolve_configured_python_command(
                "/definitely/missing/caol-api-venv"
            )
        with mock.patch("startup_harness.os.name", "nt"):
            windows_result = resolve_configured_python_command(
                r"Z:\definitely\missing\caol-api-venv"
            )

        self.assertEqual(posix_result, ([], ""))
        self.assertEqual(windows_result, ([], ""))

    def test_python_command_name_requires_path_resolution(self) -> None:
        with mock.patch("startup_harness.shutil.which", return_value=None):
            missing_result = resolve_configured_python_command("python3")
        with mock.patch(
            "startup_harness.shutil.which", return_value="/usr/bin/python3"
        ):
            resolved_result = resolve_configured_python_command("python3")

        self.assertEqual(missing_result, ([], ""))
        self.assertEqual(
            resolved_result,
            (["/usr/bin/python3"], "/usr/bin/python3"),
        )

    def test_linux_runtime_reports_nonempty_macos_venv_as_invalid(self) -> None:
        with (
            mock.patch("startup_harness.os.name", "posix"),
            mock.patch("startup_harness.sys.platform", "linux"),
        ):
            result = resolve_game_runtime_python({
                "LLM_INTENT_BACKEND": "api",
                "LLM_INTENT_PYTHON": "/Users/josefhorvath/ollama/api_env311",
            })

        self.assertEqual(
            result,
            ([], "/Users/josefhorvath/ollama/api_env311", "configured_option_invalid"),
        )

    def test_linux_runtime_rejects_macos_home_after_tilde_expansion(self) -> None:
        with (
            mock.patch("startup_harness.os.name", "posix"),
            mock.patch("startup_harness.sys.platform", "linux"),
            mock.patch(
                "startup_harness.os.path.expanduser",
                return_value="/Users/josefhorvath/ollama/api_env311",
            ),
        ):
            result = resolve_configured_python_command("~/ollama/api_env311")

        self.assertEqual(result, ([], ""))

    def test_linux_empty_runner_option_uses_same_default_as_game(self) -> None:
        with (
            mock.patch("startup_harness.os.name", "posix"),
            mock.patch("startup_harness.sys.platform", "linux"),
            mock.patch("startup_harness.os.path.isfile", return_value=True),
            mock.patch("startup_harness.os.access", return_value=True),
        ):
            result = resolve_game_runtime_python({
                "LLM_INTENT_BACKEND": "api",
                "LLM_INTENT_PYTHON": "",
            })

        self.assertEqual(
            result,
            (["/usr/bin/python3"], "/usr/bin/python3", "api_platform_default"),
        )

    def test_empty_openvino_runner_option_stays_unresolved(self) -> None:
        self.assertEqual(
            resolve_game_runtime_python({
                "LLM_INTENT_BACKEND": "openvino",
                "LLM_INTENT_PYTHON": "",
            }),
            ([], "", "configured_option"),
        )

    def test_windows_empty_api_runner_fails_closed_like_the_game_path(self) -> None:
        with mock.patch("startup_harness.os.name", "nt"):
            result = resolve_game_runtime_python({
                "LLM_INTENT_BACKEND": "api",
                "LLM_INTENT_PYTHON": "",
            })

        self.assertEqual(
            result,
            ([], "python", "api_platform_default_invalid"),
        )

    def test_api_key_provisioning_prefers_configured_environment(self) -> None:
        with (
            mock.patch.dict(os.environ, {"CATA_API_KEY": "configured", "OPENAI_API_KEY": "fallback"}, clear=True),
            mock.patch("startup_harness.read_secure_llm_credential") as read_secure,
        ):
            result = provision_llm_api_key_environment({
                "LLM_INTENT_BACKEND": "api",
                "LLM_INTENT_ENABLE": "true",
                "LLM_INTENT_API_KEY_ENV": "CATA_API_KEY",
            })

        diagnostic, child_overlay = result
        self.assertEqual(diagnostic["status"], "ready")
        self.assertEqual(diagnostic["source"], "configured_environment")
        self.assertEqual(child_overlay, {"CATA_API_KEY": "configured"})
        read_secure.assert_not_called()

    def test_api_key_provisioning_maps_openai_environment_for_game_child(self) -> None:
        with (
            mock.patch.dict(os.environ, {"OPENAI_API_KEY": "fallback"}, clear=True),
            mock.patch("startup_harness.read_secure_llm_credential") as read_secure,
        ):
            result = provision_llm_api_key_environment({
                "LLM_INTENT_BACKEND": "api",
                "LLM_INTENT_ENABLE": "true",
                "LLM_INTENT_API_KEY_ENV": "CATA_API_KEY",
            })
            inherited_value = os.environ.get("CATA_API_KEY")

        diagnostic, child_overlay = result
        self.assertEqual(diagnostic["status"], "ready")
        self.assertEqual(diagnostic["source"], "environment:OPENAI_API_KEY")
        self.assertEqual(child_overlay, {"CATA_API_KEY": "fallback"})
        self.assertIsNone(inherited_value)
        read_secure.assert_not_called()

    def test_api_key_provisioning_reads_platform_secure_store(self) -> None:
        with (
            mock.patch.dict(os.environ, {}, clear=True),
            mock.patch(
                "startup_harness.read_secure_llm_credential",
                return_value=("stored", "test_secure_store"),
            ),
        ):
            result = provision_llm_api_key_environment({
                "LLM_INTENT_BACKEND": "api",
                "LLM_INTENT_ENABLE": "true",
                "LLM_INTENT_API_KEY_ENV": "CATA_API_KEY",
            })
            inherited_value = os.environ.get("CATA_API_KEY")

        diagnostic, child_overlay = result
        self.assertEqual(diagnostic["status"], "ready")
        self.assertEqual(diagnostic["source"], "test_secure_store")
        self.assertEqual(child_overlay, {"CATA_API_KEY": "stored"})
        self.assertIsNone(inherited_value)

    def test_non_openai_provider_does_not_consume_openai_environment_key(self) -> None:
        with (
            mock.patch.dict(os.environ, {"OPENAI_API_KEY": "unrelated"}, clear=True),
            mock.patch(
                "startup_harness.read_secure_llm_credential",
                return_value=("provider-stored", "test_secure_store"),
            ) as read_secure,
        ):
            diagnostic, child_overlay = provision_llm_api_key_environment({
                "LLM_INTENT_BACKEND": "api",
                "LLM_INTENT_ENABLE": "true",
                "LLM_INTENT_API_KEY_ENV": "CATA_API_KEY",
                "LLM_INTENT_API_PROVIDER": "anthropic",
            })

        self.assertEqual(diagnostic["status"], "ready")
        self.assertEqual(diagnostic["source"], "test_secure_store")
        self.assertEqual(child_overlay, {"CATA_API_KEY": "provider-stored"})
        read_secure.assert_called_once_with("CATA_API_KEY")

    def test_legacy_use_api_mode_provisions_secure_credential(self) -> None:
        with (
            mock.patch.dict(os.environ, {}, clear=True),
            mock.patch(
                "startup_harness.read_secure_llm_credential",
                return_value=("stored", "test_secure_store"),
            ),
        ):
            diagnostic, child_overlay = provision_llm_api_key_environment({
                "LLM_INTENT_BACKEND": "openvino",
                "LLM_INTENT_USE_API": "true",
                "LLM_INTENT_ENABLE": "true",
                "LLM_INTENT_API_KEY_ENV": "CATA_API_KEY",
            })

        self.assertEqual(diagnostic["status"], "ready")
        self.assertEqual(child_overlay, {"CATA_API_KEY": "stored"})

    def test_macos_keychain_writer_keeps_secret_out_of_process_arguments(self) -> None:
        with (
            mock.patch("startup_harness.os.name", "posix"),
            mock.patch("startup_harness.sys.platform", "darwin"),
            mock.patch("startup_harness.write_macos_llm_credential") as write_native,
            mock.patch("startup_harness.subprocess.run") as run,
        ):
            store = store_secure_llm_credential("CATA_API_KEY", "test-secret")

        self.assertEqual(store, "macos_keychain")
        write_native.assert_called_once_with("CATA_API_KEY", "test-secret")
        run.assert_not_called()

    def test_macos_keychain_native_writer_adds_missing_item(self) -> None:
        security = SimpleNamespace(
            SecKeychainFindGenericPassword=mock.MagicMock(return_value=-25300),
            SecKeychainAddGenericPassword=mock.MagicMock(return_value=0),
            SecKeychainItemModifyAttributesAndData=mock.MagicMock(return_value=0),
        )
        core_foundation = SimpleNamespace(CFRelease=mock.MagicMock())
        with (
            mock.patch(
                "ctypes.CDLL",
                side_effect=[security, core_foundation],
            ),
            mock.patch("startup_harness.subprocess.run") as run,
        ):
            write_macos_llm_credential("CATA_API_KEY", "test-secret")

        security.SecKeychainFindGenericPassword.assert_called_once()
        security.SecKeychainAddGenericPassword.assert_called_once()
        security.SecKeychainItemModifyAttributesAndData.assert_not_called()
        core_foundation.CFRelease.assert_not_called()
        run.assert_not_called()

    def test_macos_keychain_native_writer_updates_and_releases_existing_item(self) -> None:
        def find_existing(*args: Any) -> int:
            item_ref_pointer = ctypes.cast(
                args[-1], ctypes.POINTER(ctypes.c_void_p)
            )
            item_ref_pointer[0] = ctypes.c_void_p(123)
            return 0

        security = SimpleNamespace(
            SecKeychainFindGenericPassword=mock.MagicMock(side_effect=find_existing),
            SecKeychainAddGenericPassword=mock.MagicMock(return_value=0),
            SecKeychainItemModifyAttributesAndData=mock.MagicMock(return_value=0),
        )
        core_foundation = SimpleNamespace(CFRelease=mock.MagicMock())
        with mock.patch(
            "ctypes.CDLL",
            side_effect=[security, core_foundation],
        ):
            write_macos_llm_credential("CATA_API_KEY", "test-secret")

        security.SecKeychainFindGenericPassword.assert_called_once()
        security.SecKeychainAddGenericPassword.assert_not_called()
        security.SecKeychainItemModifyAttributesAndData.assert_called_once()
        core_foundation.CFRelease.assert_called_once()

    def test_macos_keychain_read_timeout_is_secret_free(self) -> None:
        timeout = subprocess.TimeoutExpired(
            cmd=["/usr/bin/security"],
            timeout=15.0,
            output="partial-secret",
        )
        with (
            mock.patch("startup_harness.os.name", "posix"),
            mock.patch("startup_harness.sys.platform", "darwin"),
            mock.patch("startup_harness.subprocess.run", side_effect=timeout),
        ):
            result = read_secure_llm_credential("CATA_API_KEY")

        self.assertEqual(result, ("", "macos_keychain_timeout"))
        self.assertNotIn("partial-secret", repr(result))

    def test_macos_keychain_write_failure_is_secret_free(self) -> None:
        with (
            mock.patch("startup_harness.os.name", "posix"),
            mock.patch("startup_harness.sys.platform", "darwin"),
            mock.patch(
                "startup_harness.write_macos_llm_credential",
                side_effect=OSError(-25308, "interaction unavailable"),
            ),
            self.assertRaisesRegex(RuntimeError, "OSStatus -25308") as caught,
        ):
            store_secure_llm_credential("CATA_API_KEY", "test-secret")

        self.assertNotIn("interaction unavailable", str(caught.exception))
        self.assertNotIn("test-secret", str(caught.exception))

    def test_disabled_api_backend_does_not_require_credential(self) -> None:
        with (
            mock.patch.dict(os.environ, {}, clear=True),
            mock.patch("startup_harness.read_secure_llm_credential") as read_secure,
        ):
            diagnostic, child_overlay = provision_llm_api_key_environment({
                "LLM_INTENT_BACKEND": "api",
                "LLM_INTENT_ENABLE": "false",
                "LLM_INTENT_API_KEY_ENV": "CATA_API_KEY",
            })

        self.assertEqual(diagnostic["status"], "not_required")
        self.assertEqual(child_overlay, {})
        read_secure.assert_not_called()

    def test_enabled_api_runtime_rejects_invalid_configured_python(self) -> None:
        with mock.patch("startup_harness.subprocess.run") as run:
            with self.assertRaisesRegex(RuntimeError, "Python runner is unavailable"):
                validate_enabled_api_runtime({
                    "LLM_INTENT_BACKEND": "api",
                    "LLM_INTENT_ENABLE": "true",
                    "LLM_INTENT_PYTHON": r"Z:\definitely\missing\caol-api-venv",
                })

        run.assert_not_called()

    def test_enabled_api_runtime_turns_runner_oserror_into_configuration_error(self) -> None:
        with (
            mock.patch(
                "startup_harness.resolve_game_runtime_python",
                return_value=(["python"], "python", "configured_option"),
            ),
            mock.patch(
                "startup_harness.subprocess.run",
                side_effect=FileNotFoundError("runner disappeared"),
            ),
            self.assertRaisesRegex(RuntimeError, "could not start"),
        ):
            validate_enabled_api_runtime({
                "LLM_INTENT_BACKEND": "api",
                "LLM_INTENT_ENABLE": "true",
            })

    def test_launch_game_inherits_provisioned_api_key(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            with (
                mock.patch.dict(os.environ, {}, clear=True),
                mock.patch(
                    "startup_harness.load_game_options",
                    return_value={
                        "LLM_INTENT_BACKEND": "api",
                        "LLM_INTENT_ENABLE": "true",
                        "LLM_INTENT_API_KEY_ENV": "CATA_API_KEY",
                    },
                ),
                mock.patch(
                    "startup_harness.read_secure_llm_credential",
                    return_value=("stored", "test_secure_store"),
                ),
                mock.patch("startup_harness.validate_enabled_api_runtime"),
                mock.patch("startup_harness.detect_executable", return_value=Path("game")),
                mock.patch("startup_harness.repo_root", return_value=Path("repo")),
                mock.patch("startup_harness.subprocess.Popen") as popen,
            ):
                launch_game(
                    "test-profile",
                    "Test World",
                    run_dir,
                    scenario="manual.ecology",
                )
                child_environment = popen.call_args.kwargs["env"]
                parent_contains_key = "CATA_API_KEY" in os.environ
                popen.call_args.kwargs["stdout"].close()
                popen.call_args.kwargs["stderr"].close()

            self.assertEqual(child_environment["CATA_API_KEY"], "stored")
            self.assertEqual(child_environment["OPENCLAW_HARNESS_UI_TRACE"], "1")
            self.assertEqual(child_environment["OPENCLAW_HARNESS_RUN_DIR"], str(run_dir.resolve()))
            self.assertEqual(child_environment["OPENCLAW_HARNESS_PROFILE"], "test-profile")
            self.assertEqual(child_environment["OPENCLAW_HARNESS_WORLD"], "Test World")
            self.assertEqual(child_environment["OPENCLAW_HARNESS_SCENARIO"], "manual.ecology")
            self.assertFalse(parent_contains_key)

    def test_launch_game_clears_inherited_scenario_without_explicit_identity(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            child_environment = {"OPENCLAW_HARNESS_SCENARIO": "stale.parent"}
            with (
                mock.patch("startup_harness.detect_executable", return_value=Path("game")),
                mock.patch("startup_harness.repo_root", return_value=Path("repo")),
                mock.patch("startup_harness.subprocess.Popen") as popen,
            ):
                launch_game(
                    "test-profile",
                    "Test World",
                    run_dir,
                    child_environment=child_environment,
                )
                launched_environment = popen.call_args.kwargs["env"]
                popen.call_args.kwargs["stdout"].close()
                popen.call_args.kwargs["stderr"].close()

            self.assertNotIn("OPENCLAW_HARNESS_SCENARIO", launched_environment)
            self.assertEqual(child_environment["OPENCLAW_HARNESS_SCENARIO"], "stale.parent")

    def test_launch_game_refuses_missing_enabled_api_credential(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            with (
                mock.patch.dict(os.environ, {}, clear=True),
                mock.patch(
                    "startup_harness.load_game_options",
                    return_value={
                        "LLM_INTENT_BACKEND": "api",
                        "LLM_INTENT_ENABLE": "true",
                        "LLM_INTENT_API_KEY_ENV": "CATA_API_KEY",
                    },
                ),
                mock.patch(
                    "startup_harness.read_secure_llm_credential",
                    return_value=("", "test_secure_store"),
                ),
                mock.patch("startup_harness.validate_enabled_api_runtime"),
                mock.patch("startup_harness.detect_executable", return_value=Path("game")),
                mock.patch("startup_harness.subprocess.Popen") as popen,
                self.assertRaisesRegex(RuntimeError, "credential is unavailable"),
            ):
                launch_game("test-profile", "Test World", run_dir)

            popen.assert_not_called()
            self.assertFalse((run_dir / "game.stdout.log").exists())
            self.assertFalse((run_dir / "game.stderr.log").exists())

    def test_release_candidate_selects_platform_api_runner(self) -> None:
        scenario = load_scenario("manual.release_candidate_roaming_mcw")

        self.assertEqual(
            resolve_scenario_profile_option_overrides(scenario, "windows"),
            {
                "DEBUG_LLM_INTENT_LOG": "true",
                "DEBUG_LLM_INTENT_UI": "true",
                "LLM_INTENT_API_KEY_ENV": "CATA_API_KEY",
                "LLM_INTENT_API_MODEL": "gpt-5.4-nano",
                "LLM_INTENT_BACKEND": "api",
                "LLM_INTENT_ENABLE": "true",
                "LLM_INTENT_PYTHON": r"C:\Users\josef\openvino_models\openvino_env",
                "TILES": "UltimateCataclysm",
            },
        )
        self.assertEqual(
            resolve_scenario_profile_option_overrides(scenario, "macos")[
                "LLM_INTENT_PYTHON"
            ],
            "/Users/josefhorvath/ollama/api_env311",
        )
        self.assertNotIn(
            "LLM_INTENT_PYTHON",
            resolve_scenario_profile_option_overrides(scenario, "linux"),
        )

    def test_platform_option_overrides_reject_unknown_platform(self) -> None:
        with self.assertRaisesRegex(ValueError, "unsupported platform"):
            resolve_scenario_profile_option_overrides({
                "profile_option_overrides_by_platform": {
                    "plan9": {"LLM_INTENT_PYTHON": "python"}
                }
            })

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
            "profile_option_overrides": {"TILES": "UltimateCataclysm"},
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
        self.assertEqual(
            start_command[start_command.index("--scenario-identity") + 1],
            "test.isolated_profile",
        )
        self.assertEqual(
            start_command[start_command.index("--profile-option") + 1],
            "TILES=UltimateCataclysm",
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
                profile_option_overrides={},
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


class BanditLiveWorldAuditContractTest(unittest.TestCase):
    @staticmethod
    def current_pair_site() -> Dict[str, Any]:
        return {
            "site_id": "camp-current",
            "site_kind": "bandit_camp",
            "hostile_profile": "bandit_camp",
            "members": [
                {"npc_id": 101, "state": "local_contact"},
                {"npc_id": 102, "state": "local_contact"},
            ],
            "active_outing": {
                "schema_version": 8,
                "kind": "structural_sortie",
                "activity_id": "camp-current#structural",
                "camp_id": "camp-current",
                "generation": 3,
                "member_ids": [101, 102],
                "leader_id": 101,
                "shared_route": [[1, 1, 0], [2, 1, 0], [3, 1, 0]],
                "waypoint_index": 1,
                "target_id": "forest",
                "target_omt": [2, 1, 0],
                "job_type": "scavenge",
                "phase": "outbound",
                "simulation_owner": "local",
                "handoff_epoch": 1,
                "started_minutes": 60,
                "local_contact_minutes": 120,
                "last_progress_minutes": 120,
                "last_advanced_minutes": 120,
                "local_handoff": {
                    "schema_version": 3,
                    "activity_id": "camp-current#structural",
                    "activity_generation": 3,
                    "handoff_epoch": 1,
                    "waypoint_index": 1,
                    "phase": "outbound",
                    "cohesion_leader_id": 101,
                    "route_position": [2, 1, 0],
                    "committed_minutes": 120,
                    "members": [
                        {
                            "npc_id": 101,
                            "entry_position": [48, 24, 0],
                            "staging_position": [49, 24, 0],
                            "hp_percent": 100,
                            "dead": False,
                        },
                        {
                            "npc_id": 102,
                            "entry_position": [48, 25, 0],
                            "staging_position": [49, 25, 0],
                            "hp_percent": 85,
                            "dead": False,
                        },
                    ],
                },
            },
        }

    @staticmethod
    def write_world(world_dir: Path, site: Dict[str, Any]) -> None:
        (world_dir / "dimension_data.gsav").write_text(
            "# version 39\n"
            + json.dumps({
                "overmapbuffer": {
                    "bandit_live_world": {"sites": [site]},
                },
            }),
            encoding="utf-8",
        )

    def test_current_pair_summary_exposes_nested_owner_route_handoff_and_cursor(self) -> None:
        summary = summarize_bandit_live_world_site(self.current_pair_site())
        outing = summary["active_outing"]
        handoff = outing["local_handoff"]

        self.assertEqual(summary["active_group_id"], "camp-current#structural")
        self.assertEqual(summary["active_member_ids"], [101, 102])
        self.assertEqual(outing["simulation_cursor"], {
            "activity_id": "camp-current#structural",
            "generation": 3,
            "simulation_owner": "local",
            "handoff_epoch": 1,
            "last_advanced_minutes": 120,
        })
        self.assertTrue(outing["exact_pair_with_leader"])
        self.assertTrue(outing["structural_route_valid"])
        self.assertEqual(outing["waypoint_omt"], [2, 1, 0])
        self.assertTrue(outing["owner_cursor_valid"])
        self.assertEqual(handoff["state"], "local")
        self.assertEqual(handoff["member_ids"], [101, 102])
        self.assertTrue(handoff["distinct_entry_positions"])
        self.assertTrue(handoff["distinct_staging_positions"])
        self.assertTrue(handoff["entry_staging_positions_separate"])
        self.assertTrue(handoff["cursor_consistent"])
        self.assertTrue(handoff["pair_contract_valid"])
        self.assertTrue(outing["pair_contract_valid"])

    def test_lead_summary_and_audit_expose_persisted_origin(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir)
            site = self.current_pair_site()
            site["intelligence_map"] = {
                "leads": [{
                    "lead_id": "signal:smoke:1",
                    "kind": "camp_signal",
                    "origin": "autonomous_signal",
                }],
            }
            self.write_world(world_dir, site)

            matching = audit_saved_bandit_live_world_state(
                world_dir,
                required_lead_origin="autonomous_signal",
            )
            missing = audit_saved_bandit_live_world_state(
                world_dir,
                required_lead_origin="fixture_seed",
            )

        self.assertEqual(
            summarize_bandit_live_world_site(site)["leads"][0]["origin"],
            "autonomous_signal",
        )
        self.assertEqual(matching["status"], "required_state_present")
        self.assertEqual(matching["required_fields"]["required_lead_origin"], "autonomous_signal")
        self.assertEqual(missing["status"], "required_state_missing")

    def test_required_max_leads_accepts_zero_and_rejects_nonzero(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir)
            site = self.current_pair_site()
            site["intelligence_map"] = {"leads": []}
            self.write_world(world_dir, site)
            empty = audit_saved_bandit_live_world_state(world_dir, required_max_leads=0)

            site["intelligence_map"]["leads"] = [{"lead_id": "unexpected"}]
            self.write_world(world_dir, site)
            nonzero = audit_saved_bandit_live_world_state(world_dir, required_max_leads=0)

        self.assertEqual(empty["status"], "required_state_present")
        self.assertEqual(empty["required_fields"]["required_max_leads"], 0)
        self.assertEqual(nonzero["status"], "required_state_missing")

    def test_final_scout_report_and_decision_audit_share_authoritative_identity(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir)
            site = self.current_pair_site()
            site["active_outing"] = {}
            site["active_group_id"] = ""
            site["active_target_id"] = ""
            site["active_member_ids"] = []
            report_identity = {
                "revision": 7,
                "source_generation": 3,
                "source_activity_id": "camp-current#structural",
                "application_key": "camp-current#structural:3:report",
                "target_id": "road",
                "target_omt": [3, 1, 0],
                "target_lead_id": "terrain-opportunity:road",
                "target_lead_revision": 2,
            }
            site["current_scout_report"] = {
                **report_identity,
                "action_policy": "raid",
                "source_job_type": "scout",
                "observations": [{"kind": "occupancy"}],
                "casualty_ids": [],
                "delivered_minutes": 480,
                "provisional": False,
            }
            site["camp_decision"] = {
                "state": "report_awaiting_assessment",
                "report_policy": "raid",
                "source_report_revision": report_identity["revision"],
                "source_report_generation": report_identity["source_generation"],
                "source_report_activity_id": report_identity["source_activity_id"],
                "source_report_application_key": report_identity["application_key"],
                "target_id": report_identity["target_id"],
                "target_omt": report_identity["target_omt"],
                "target_lead_id": report_identity["target_lead_id"],
                "target_lead_revision": report_identity["target_lead_revision"],
            }
            self.write_world(world_dir, site)

            matching = audit_saved_bandit_live_world_state(
                world_dir,
                required_active_group_id_exact="",
                required_scout_report_present=True,
                required_scout_report_provisional=False,
                required_scout_report_min_observations=1,
                required_camp_decision_state="report_awaiting_assessment",
                required_report_decision_identity_match=True,
            )
            site["camp_decision"]["target_lead_revision"] = 3
            self.write_world(world_dir, site)
            mismatched = audit_saved_bandit_live_world_state(
                world_dir,
                required_report_decision_identity_match=True,
            )

        self.assertEqual(matching["status"], "required_state_present")
        matched_site = matching["matching_sites"][0]
        self.assertFalse(matched_site["current_scout_report"]["provisional"])
        self.assertEqual(matched_site["current_scout_report"]["observation_count"], 1)
        self.assertTrue(matched_site["report_decision_identity_matches"])
        self.assertEqual(mismatched["status"], "required_state_missing")

    def test_home_survivor_audit_accepts_wounded_carrier_and_rejects_all_loss(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir)
            site = self.current_pair_site()
            site["active_outing"] = {}
            site["active_group_id"] = ""
            site["active_member_ids"] = []
            site["members"] = [
                {"npc_id": 1, "state": "at_home"},
                {"npc_id": 2, "state": "at_home"},
                {"npc_id": 3, "state": "at_home"},
                {"npc_id": 4, "state": "at_home", "wounded_or_unready": True},
                {"npc_id": 5, "state": "missing"},
            ]
            self.write_world(world_dir, site)
            wounded_carrier = audit_saved_bandit_live_world_state(
                world_dir,
                required_min_home_survivor_count=4,
            )

            site["members"][3]["state"] = "missing"
            self.write_world(world_dir, site)
            all_scouts_lost = audit_saved_bandit_live_world_state(
                world_dir,
                required_min_home_survivor_count=4,
            )

        self.assertEqual(wounded_carrier["status"], "required_state_present")
        self.assertEqual(wounded_carrier["matching_sites"][0]["home_survivor_count"], 4)
        self.assertEqual(all_scouts_lost["status"], "required_state_missing")

    def test_required_all_lead_origin_rejects_one_foreign_writer(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir)
            site = self.current_pair_site()
            site["intelligence_map"] = {
                "leads": [
                    {"lead_id": "terrain-a", "origin": "structural_routine"},
                    {"lead_id": "terrain-b", "origin": "structural_routine"},
                ],
            }
            self.write_world(world_dir, site)
            green = audit_saved_bandit_live_world_state(
                world_dir,
                required_all_lead_origin="structural_routine",
            )

            site["intelligence_map"]["leads"][1]["origin"] = "legacy_radar"
            self.write_world(world_dir, site)
            foreign = audit_saved_bandit_live_world_state(
                world_dir,
                required_all_lead_origin="structural_routine",
            )

        self.assertEqual(green["status"], "required_state_present")
        self.assertEqual(
            green["required_fields"]["required_all_lead_origin"],
            "structural_routine",
        )
        self.assertEqual(foreign["status"], "required_state_missing")

    def test_required_all_lead_origin_rejects_malformed_lead(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir)
            site = self.current_pair_site()
            site["intelligence_map"] = {
                "leads": [
                    {"lead_id": "terrain-a", "origin": "structural_routine"},
                    "malformed-lead",
                ],
            }
            self.write_world(world_dir, site)

            result = audit_saved_bandit_live_world_state(
                world_dir,
                required_all_lead_origin="structural_routine",
            )

        self.assertEqual(result["status"], "required_state_missing")

    def test_current_pair_requirements_reject_malformed_handoff_without_legacy_bypass(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir)
            site = self.current_pair_site()
            self.write_world(world_dir, site)
            green = audit_saved_bandit_live_world_state(
                world_dir,
                required_active_outing_kind="structural_sortie",
                required_active_outing_activity_id_contains="#structural",
                required_active_outing_generation=3,
                required_active_outing_simulation_owner="local",
                required_active_outing_handoff_epoch=1,
                required_active_outing_phase="outbound",
                required_active_outing_waypoint_index=1,
                required_active_outing_last_advanced_minutes=120,
                required_active_outing_exact_pair=True,
                required_active_outing_pair_contract=True,
                required_local_handoff_state="local",
                required_local_handoff_exact_pair=True,
                required_local_handoff_pair_contract=True,
            )
            legacy_empty = audit_saved_bandit_live_world_state(
                world_dir,
                required_active_group_id_exact="",
            )

            site["active_outing"]["local_handoff"]["members"][1]["staging_position"] = [49, 24, 0]
            self.write_world(world_dir, site)
            malformed = audit_saved_bandit_live_world_state(
                world_dir,
                required_local_handoff_pair_contract=True,
            )

        self.assertEqual(green["status"], "required_state_present")
        self.assertEqual(legacy_empty["status"], "required_state_missing")
        self.assertEqual(malformed["status"], "required_state_missing")

    def test_legacy_active_fields_remain_auditable_without_nested_outing(self) -> None:
        legacy_site = {
            "site_id": "camp-legacy",
            "site_kind": "bandit_camp",
            "active_group_id": "legacy-group",
            "active_target_id": "legacy-target",
            "active_member_ids": [7],
        }
        summary = summarize_bandit_live_world_site(legacy_site)

        self.assertEqual(summary["active_group_id"], "legacy-group")
        self.assertEqual(summary["active_target_id"], "legacy-target")
        self.assertEqual(summary["active_member_ids"], [7])
        self.assertFalse(summary["active_outing"]["is_active"])

    def test_windows_uses_exe_zzip_helper(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            helper = repo_root / "zzip.exe"
            helper.touch()

            self.assertEqual(bandit_zzip_binary(repo_root, platform_name="nt"), helper)

    def test_posix_uses_extensionless_executable_zzip_helper(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            helper = repo_root / "zzip"
            helper.touch(mode=0o700)

            self.assertEqual(bandit_zzip_binary(repo_root, platform_name="posix"), helper)


class ProfileOptionOverrideContractTest(unittest.TestCase):
    def test_updates_existing_profile_option_without_changing_other_entries(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            options_path = Path(temp_dir) / "options.json"
            options_path.write_text(
                json.dumps([
                    {"name": "TILES", "value": "ASCIITiles", "info": "tileset"},
                    {"name": "SOUND_ENABLED", "value": "true"},
                ]),
                encoding="utf-8",
            )

            result = apply_option_overrides_to_file(options_path, {"TILES": "UltimateCataclysm"})
            updated = json.loads(options_path.read_text(encoding="utf-8"))

            self.assertEqual(result["applied"], {"TILES": "UltimateCataclysm"})
            self.assertEqual(updated[0], {"name": "TILES", "value": "UltimateCataclysm", "info": "tileset"})
            self.assertEqual(updated[1], {"name": "SOUND_ENABLED", "value": "true"})


class PlayerMutationsTransformContractTest(unittest.TestCase):
    @staticmethod
    def fake_zzip(path: Path) -> None:
        if path.suffix == ".zzip":
            path.with_suffix("").write_bytes(path.read_bytes())
        else:
            path.with_suffix(f"{path.suffix}.zzip").write_bytes(path.read_bytes())

    def test_normalizes_report_and_is_idempotent_without_replacing_mutation_state(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir) / "McWilliams"
            world_dir.mkdir()
            player_save = world_dir / "player.sav.zzip"
            inherited_mutation = {
                "corrupted": 0,
                "key": 99,
                "charge": 7,
                "powered": True,
                "show_sprite": False,
            }
            payload = {
                "player": {
                    "traits": ["DEBUG_CLOAK"],
                    "mutations": {"DEBUG_CLOAK": inherited_mutation},
                    "cached_mutations": {"DEBUG_CLOAK": inherited_mutation},
                },
            }
            player_save.write_text(json.dumps(payload), encoding="utf-8")
            transform = {
                "kind": "player_mutations",
                "player_save": player_save.name,
                "mutations": [" DEBUG_CLAIRVOYANCE ", "DEBUG_CLAIRVOYANCE"],
            }

            with mock.patch("startup_harness.run_zzip", side_effect=self.fake_zzip):
                first_report = apply_player_mutations_transform(world_dir, transform)
                first_payload = json.loads(player_save.read_text(encoding="utf-8"))
                second_report = apply_player_mutations_transform(world_dir, transform)
                second_payload = json.loads(player_save.read_text(encoding="utf-8"))

        self.assertEqual(first_report["requested_mutations"], ["DEBUG_CLAIRVOYANCE"])
        self.assertEqual(first_report["traits_before"], ["DEBUG_CLOAK"])
        self.assertEqual(
            first_report["traits_after"],
            ["DEBUG_CLOAK", "DEBUG_CLAIRVOYANCE"],
        )
        self.assertEqual(first_report["added_traits"], ["DEBUG_CLAIRVOYANCE"])
        self.assertEqual(first_report["already_present"], [])
        self.assertTrue(first_report["newly_added"])
        self.assertEqual(second_report["added_traits"], [])
        self.assertEqual(second_report["already_present"], ["DEBUG_CLAIRVOYANCE"])
        self.assertFalse(second_report["newly_added"])
        self.assertEqual(second_report["traits_before"], second_report["traits_after"])
        self.assertEqual(first_payload, second_payload)
        self.assertEqual(
            second_payload["player"]["mutations"]["DEBUG_CLOAK"],
            inherited_mutation,
        )
        self.assertEqual(
            second_payload["player"]["cached_mutations"]["DEBUG_CLOAK"],
            inherited_mutation,
        )


class BanditCloneSiteTransformContractTest(unittest.TestCase):
    def test_clone_can_set_cannibal_profile_explicitly(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir)
            dimension_path = world_dir / "dimension_data.gsav"
            dimension_path.write_text(
                "# version 39\n" + json.dumps({
                    "overmapbuffer": {
                        "bandit_live_world": {
                            "sites": [{
                                "site_id": "source",
                                "site_kind": "bandit_camp",
                                "hostile_profile": "camp_style",
                            }],
                        },
                    },
                }),
                encoding="utf-8",
            )

            result = apply_bandit_clone_site_transform(world_dir, {
                "source_site_id": "source",
                "new_site_id": "cannibal",
                "new_site_kind": "cannibal_camp",
                "new_hostile_profile": "cannibal_camp",
            })
            payload = json.loads(dimension_path.read_text(encoding="utf-8").split("\n", 1)[1])
            cloned = payload["overmapbuffer"]["bandit_live_world"]["sites"][1]

            self.assertEqual(result["new_hostile_profile"], "cannibal_camp")
            self.assertEqual(cloned["site_kind"], "cannibal_camp")
            self.assertEqual(cloned["hostile_profile"], "cannibal_camp")

    def test_clone_can_reset_inherited_shakedown_history(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir)
            dimension_path = world_dir / "dimension_data.gsav"
            dimension_path.write_text(
                "# version 39\n" + json.dumps({
                    "overmapbuffer": {
                        "bandit_live_world": {
                            "sites": [{
                                "site_id": "source",
                                "last_shakedown_outcome": "fight_defender_loss",
                                "shakedown_anger": 4,
                                "shakedown_defender_losses": 2,
                                "shakedown_reopen_available": True,
                            }],
                        },
                    },
                }),
                encoding="utf-8",
            )

            result = apply_bandit_clone_site_transform(world_dir, {
                "source_site_id": "source",
                "new_site_id": "clean-clone",
                "reset_shakedown_history": True,
            })
            payload = json.loads(dimension_path.read_text(encoding="utf-8").split("\n", 1)[1])
            source, cloned = payload["overmapbuffer"]["bandit_live_world"]["sites"]

        self.assertTrue(result["reset_shakedown_history"])
        self.assertEqual(source["last_shakedown_outcome"], "fight_defender_loss")
        self.assertEqual(cloned["last_shakedown_outcome"], "")
        self.assertEqual(cloned["shakedown_anger"], 0)
        self.assertEqual(cloned["shakedown_defender_losses"], 0)
        self.assertFalse(cloned["shakedown_reopen_available"])


class OvermapTerrainIdAtAbsOmtTransformContractTest(unittest.TestCase):
    @staticmethod
    def _payload_with_surface_target() -> Dict[str, Any]:
        surface = ["field"] * (OMAPX * OMAPY)
        surface[0] = "forest"
        surface[overmap_flat_index((137, 49, 0))] = "road_ns"
        layers = [[["field", OMAPX * OMAPY]] for _ in range(overmap_layer_index(0) + 1)]
        layers[overmap_layer_index(0)] = encode_overmap_layer(surface)
        return {"layers": layers, "_created_plain": True}

    def test_round_trip_changes_only_one_exact_absolute_omt(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir)
            overmaps_dir = world_dir / "overmaps"
            overmaps_dir.mkdir()
            overmap_path = overmaps_dir / "o.0.0.zzip"
            overmap_path.touch()
            plain_path = world_dir / "o.0.0"
            payload = self._payload_with_surface_target()
            before = decode_overmap_layer(
                payload["layers"][overmap_layer_index(0)],
                context="before",
            )
            normalized = normalize_fixture_save_transforms(
                [{
                    "kind": "overmap_terrain_id_at_abs_omt",
                    "player_save": "survivor.sav.zzip",
                    "abs_omt": [137, 49, 0],
                    "terrain_id": "forest",
                }],
                manifest_path=world_dir / "manifest.json",
            )

            with (
                mock.patch(
                    "startup_harness.extract_overmap_payload",
                    return_value=(plain_path, "# version 39", payload),
                ),
                mock.patch("startup_harness.write_overmap_payload") as write_payload,
            ):
                reports = apply_fixture_save_transforms(world_dir, normalized)

            written_payload = write_payload.call_args.args[2]
            after = decode_overmap_layer(
                written_payload["layers"][overmap_layer_index(0)],
                context="after",
            )

        target_index = overmap_flat_index((137, 49, 0))
        self.assertEqual(reports, [{
            "kind": "overmap_terrain_id_at_abs_omt",
            "world": world_dir.name,
            "player_save": "survivor.sav.zzip",
            "abs_omt": [137, 49, 0],
            "local_omt": [137, 49, 0],
            "overmap": "overmaps/o.0.0.zzip",
            "previous_terrain_id": "road_ns",
            "terrain_id": "forest",
        }])
        self.assertEqual(before[target_index], "road_ns")
        self.assertEqual(after[target_index], "forest")
        self.assertEqual(
            [index for index, pair in enumerate(zip(before, after)) if pair[0] != pair[1]],
            [target_index],
        )

    def test_normalization_rejects_malformed_coordinates_and_empty_terrain(self) -> None:
        invalid_coordinates = (
            None,
            [137, 49],
            [137, 49, 0, 1],
            ["137", 49, 0],
            [True, 49, 0],
        )
        for abs_omt in invalid_coordinates:
            with self.subTest(abs_omt=abs_omt), self.assertRaisesRegex(
                    SystemExit, "needs abs_omt=\\[integer x, integer y, integer z\\]"):
                normalize_fixture_save_transforms(
                    [{
                        "kind": "overmap_terrain_id_at_abs_omt",
                        "player_save": "survivor.sav.zzip",
                        "abs_omt": abs_omt,
                        "terrain_id": "forest",
                    }],
                    manifest_path=Path("manifest.json"),
                )
        with self.assertRaisesRegex(SystemExit, "needs terrain_id"):
            normalize_fixture_save_transforms(
                [{
                    "kind": "overmap_terrain_id_at_abs_omt",
                    "player_save": "survivor.sav.zzip",
                    "abs_omt": [137, 49, 0],
                    "terrain_id": "",
                }],
                manifest_path=Path("manifest.json"),
            )

    def test_apply_fails_closed_for_missing_layer_and_unknown_terrain(self) -> None:
        failure_cases = (
            ({}, "has no layers array"),
            ({"layers": [[["field", OMAPX * OMAPY]]], "_created_plain": True}, "layer z=0 is missing"),
            (self._payload_with_surface_target(), "is not present in the authoritative saved overmap vocabulary"),
        )
        for payload, expected_error in failure_cases:
            with self.subTest(expected_error=expected_error), tempfile.TemporaryDirectory() as temp_dir:
                world_dir = Path(temp_dir)
                overmaps_dir = world_dir / "overmaps"
                overmaps_dir.mkdir()
                (overmaps_dir / "o.0.0.zzip").touch()
                original_payload = json.dumps(payload, sort_keys=True)
                with (
                    mock.patch(
                        "startup_harness.extract_overmap_payload",
                        return_value=(world_dir / "o.0.0", "# version 39", payload),
                    ),
                    mock.patch("startup_harness.write_overmap_payload") as write_payload,
                    self.assertRaisesRegex(SystemExit, expected_error),
                ):
                    apply_overmap_terrain_id_at_abs_omt_transform(
                        world_dir,
                        {
                            "player_save": "survivor.sav.zzip",
                            "abs_omt": [137, 49, 0],
                            "terrain_id": "not_a_real_saved_terrain",
                        },
                    )
                write_payload.assert_not_called()
                self.assertEqual(json.dumps(payload, sort_keys=True), original_payload)


class BanditRosterShapeTransformContractTest(unittest.TestCase):
    def test_can_clear_inherited_spawn_heads_without_inflating_roster(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir)
            dimension_path = world_dir / "dimension_data.gsav"
            dimension_path.write_text(
                "# version 39\n" + json.dumps({
                    "overmapbuffer": {
                        "bandit_live_world": {
                            "sites": [{
                                "site_id": "camp-selected",
                                "headcount": 12,
                                "spawn_tiles": [{"headcount": 7}],
                                "members": [
                                    {"npc_id": 101, "state": "at_home"},
                                    {"npc_id": 102, "state": "at_home"},
                                    {"npc_id": 103, "state": "at_home"},
                                ],
                            }],
                        },
                    },
                }),
                encoding="utf-8",
            )
            normalized = normalize_fixture_save_transforms(
                [{
                    "kind": "bandit_site_roster_shape",
                    "player_save": "survivor.sav",
                    "site_id": "camp-selected",
                    "living_member_count": 3,
                    "clear_spawn_tile_headcount": True,
                }],
                manifest_path=world_dir / "manifest.json",
            )

            reports = apply_fixture_save_transforms(world_dir, normalized)
            payload = json.loads(dimension_path.read_text(encoding="utf-8").split("\n", 1)[1])
            updated = payload["overmapbuffer"]["bandit_live_world"]["sites"][0]

        self.assertEqual(updated["headcount"], 3)
        self.assertEqual(len(updated["members"]), 3)
        self.assertEqual(updated["spawn_tiles"][0]["headcount"], 0)

    def test_current_schema_shapes_authority_without_legacy_headcounts(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir)
            dimension_path = world_dir / "dimension_data.gsav"
            canonical_active_outing = {
                "operation_id": "structural-sortie-4",
                "generation": 4,
                "phase": "observing",
                "member_ids": [901, 902],
            }
            canonical_active_outing_bytes = json.dumps(
                canonical_active_outing, sort_keys=True, separators=(",", ":")
            )
            dimension_path.write_text(
                "# version 39\n" + json.dumps({
                    "overmapbuffer": {
                        "bandit_live_world": {
                            "sites": [{
                                "schema_version": 12,
                                "site_id": "camp-selected",
                                "headcount": 6,
                                "living_total": 6,
                                "supply_units": 37,
                                "supply_last_update_minutes": 8280,
                                "supply_accounted_living_total": 6,
                                "supply_member_minute_remainder": 311,
                                "active_member_ids": [101],
                                "active_group_id": "legacy-group",
                                "active_target_id": "legacy-target",
                                "active_target_omt": [140, 39, 0],
                                "active_job_type": "scout",
                                "active_sortie_started_minutes": 8000,
                                "active_sortie_local_contact_minutes": 8100,
                                "active_outing": canonical_active_outing,
                                "spawn_tiles": [
                                    {
                                        "tile": [3360, 1224, 0],
                                        "headcount": 4,
                                        "assigned_living_total": 3,
                                    },
                                    {
                                        "tile": [3361, 1224, 0],
                                        "headcount": 2,
                                        "assigned_living_total": 2,
                                    },
                                ],
                                "members": [
                                    {"npc_id": 101 + index, "state": "at_home"}
                                    for index in range(6)
                                ],
                            }],
                        },
                    },
                }),
                encoding="utf-8",
            )
            normalized = normalize_fixture_save_transforms(
                [{
                    "kind": "bandit_site_roster_shape",
                    "player_save": "survivor.sav",
                    "site_id": "camp-selected",
                    "living_member_count": 5,
                    "headcount_override": 7,
                    "clear_spawn_tile_headcount": True,
                }],
                manifest_path=world_dir / "manifest.json",
            )

            reports = apply_fixture_save_transforms(world_dir, normalized)
            reloaded = json.loads(dimension_path.read_text(encoding="utf-8").split("\n", 1)[1])
            updated = reloaded["overmapbuffer"]["bandit_live_world"]["sites"][0]

        self.assertEqual(reports[0]["site_schema_version"], 12)
        self.assertNotIn("headcount", updated)
        self.assertEqual(updated["living_total"], 7)
        self.assertEqual(updated["supply_accounted_living_total"], 7)
        self.assertEqual(updated["supply_units"], 37)
        self.assertEqual(updated["supply_member_minute_remainder"], 311)
        self.assertEqual(len(updated["members"]), 5)
        legacy_active_fields = {
            "active_member_ids",
            "active_group_id",
            "active_target_id",
            "active_target_omt",
            "active_job_type",
            "active_sortie_started_minutes",
            "active_sortie_local_contact_minutes",
        }
        self.assertTrue(legacy_active_fields.isdisjoint(updated))
        self.assertEqual(
            json.dumps(updated["active_outing"], sort_keys=True, separators=(",", ":")),
            canonical_active_outing_bytes,
        )
        self.assertEqual(
            [spawn_tile["assigned_living_total"] for spawn_tile in updated["spawn_tiles"]],
            [3, 2],
        )
        self.assertTrue(all("headcount" not in spawn_tile for spawn_tile in updated["spawn_tiles"]))

    def test_current_schema_rejects_fabricated_outside_ownership(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir)
            dimension_path = world_dir / "dimension_data.gsav"
            original = "# version 39\n" + json.dumps({
                "overmapbuffer": {
                    "bandit_live_world": {
                        "sites": [{
                            "schema_version": 12,
                            "site_id": "camp-selected",
                            "living_total": 2,
                            "members": [
                                {"npc_id": 101, "state": "at_home"},
                                {"npc_id": 102, "state": "at_home"},
                            ],
                        }],
                    },
                },
            })
            dimension_path.write_text(original, encoding="utf-8")
            normalized = normalize_fixture_save_transforms(
                [{
                    "kind": "bandit_site_roster_shape",
                    "player_save": "survivor.sav",
                    "site_id": "camp-selected",
                    "living_member_count": 2,
                    "active_outside_member_count": 1,
                }],
                manifest_path=world_dir / "manifest.json",
            )

            with self.assertRaisesRegex(SystemExit, "authoritative outing transform"):
                apply_fixture_save_transforms(world_dir, normalized)

            self.assertEqual(dimension_path.read_text(encoding="utf-8"), original)


class BanditClearSiteEvidenceTransformContractTest(unittest.TestCase):
    def test_clears_only_exact_site_evidence_and_preserves_identity_and_roster(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir)
            dimension_path = world_dir / "dimension_data.gsav"
            selected = {
                "site_id": "camp-selected",
                "source_id": "overmap-special-7",
                "source_kind": "overmap_special",
                "site_kind": "bandit_camp",
                "hostile_profile": "bandit_camp",
                "anchor": [140, 51, 0],
                "headcount": 2,
                "members": [
                    {"npc_id": 101, "state": "at_home"},
                    {"npc_id": 102, "state": "outbound"},
                ],
                "remembered_target_or_mark": "smoke:149,51,0",
                "remembered_pressure": "investigate",
                "known_recent_marks": ["smoke:149,51,0"],
                "intelligence_map": {
                    "schema_version": 5,
                    "leads": [{"lead_id": "smoke:149,51,0", "origin": "autonomous_signal"}],
                },
            }
            untouched = {
                "site_id": "camp-untouched",
                "members": [{"npc_id": 201, "state": "at_home"}],
                "remembered_target_or_mark": "sound:10,10,0",
                "remembered_pressure": "watch",
                "known_recent_marks": ["sound:10,10,0"],
                "intelligence_map": {"leads": [{"lead_id": "sound:10,10,0"}]},
            }
            selected_identity_and_roster = {
                key: json.loads(json.dumps(selected[key]))
                for key in (
                    "site_id",
                    "source_id",
                    "source_kind",
                    "site_kind",
                    "hostile_profile",
                    "anchor",
                    "headcount",
                    "members",
                )
            }
            untouched_before = json.loads(json.dumps(untouched))
            dimension_path.write_text(
                "# version 39\n" + json.dumps({
                    "overmapbuffer": {
                        "bandit_live_world": {"sites": [selected, untouched]},
                    },
                }),
                encoding="utf-8",
            )
            normalized = normalize_fixture_save_transforms(
                [{
                    "kind": "bandit_clear_site_evidence",
                    "player_save": "survivor.sav",
                    "site_id": "camp-selected",
                }],
                manifest_path=world_dir / "manifest.json",
            )

            reports = apply_fixture_save_transforms(world_dir, normalized)
            payload = json.loads(dimension_path.read_text(encoding="utf-8").split("\n", 1)[1])
            updated_selected, updated_untouched = payload["overmapbuffer"]["bandit_live_world"]["sites"]

        self.assertEqual(reports[0]["kind"], "bandit_clear_site_evidence")
        self.assertEqual(reports[0]["previous_lead_count"], 1)
        self.assertEqual(reports[0]["lead_count"], 0)
        self.assertEqual(updated_selected["intelligence_map"], {
            "schema_version": 5,
            "last_daily_cleanup_minutes": -1,
            "next_near_tick_minutes": -1,
            "next_mid_tick_minutes": -1,
            "next_far_tick_minutes": -1,
            "next_frontier_tick_minutes": -1,
            "known_radius_omt": 0,
            "terrain_scan_cursor": 0,
            "last_routine_target_lead_id": "",
            "previous_routine_target_lead_id": "",
            "frontier_radius_omt": 0,
            "frontier_sector_cursor": 0,
            "frontier_last_resolved_minutes": [-1] * 8,
            "leads": [],
        })
        self.assertEqual(updated_selected["remembered_target_or_mark"], "")
        self.assertEqual(updated_selected["remembered_pressure"], "")
        self.assertEqual(updated_selected["known_recent_marks"], [])
        for key, expected in selected_identity_and_roster.items():
            self.assertEqual(updated_selected[key], expected)
        self.assertEqual(updated_untouched, untouched_before)

    def test_requires_one_exact_site_match(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir)
            (world_dir / "dimension_data.gsav").write_text(
                "# version 39\n" + json.dumps({
                    "overmapbuffer": {
                        "bandit_live_world": {"sites": [{"site_id": "camp-present"}]},
                    },
                }),
                encoding="utf-8",
            )

            with self.assertRaisesRegex(SystemExit, "no site exactly matching camp-missing"):
                apply_bandit_clear_site_evidence_transform(
                    world_dir,
                    {"site_id": "camp-missing"},
                )

    def test_can_defer_next_near_tick_while_clearing_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir)
            dimension_path = world_dir / "dimension_data.gsav"
            dimension_path.write_text(
                "# version 39\n" + json.dumps({
                    "overmapbuffer": {
                        "bandit_live_world": {
                            "sites": [{
                                "site_id": "camp-selected",
                                "members": [],
                                "intelligence_map": {"schema_version": 5, "leads": []},
                            }],
                        },
                    },
                }),
                encoding="utf-8",
            )
            normalized = normalize_fixture_save_transforms(
                [{
                    "kind": "bandit_clear_site_evidence",
                    "player_save": "survivor.sav",
                    "site_id": "camp-selected",
                    "next_near_tick_minutes": 10861,
                }],
                manifest_path=world_dir / "manifest.json",
            )

            reports = apply_fixture_save_transforms(world_dir, normalized)
            payload = json.loads(dimension_path.read_text(encoding="utf-8").split("\n", 1)[1])
            intelligence = payload["overmapbuffer"]["bandit_live_world"]["sites"][0][
                "intelligence_map"
            ]

        self.assertEqual(intelligence["next_near_tick_minutes"], 10861)
        self.assertEqual(intelligence["last_routine_target_lead_id"], "")
        self.assertEqual(intelligence["previous_routine_target_lead_id"], "")
        self.assertEqual(reports[0]["next_near_tick_minutes"], 10861)

    def test_rejects_invalid_next_near_tick(self) -> None:
        for invalid_value in (-2, 2147483648, True, "10861"):
            with self.subTest(invalid_value=invalid_value), self.assertRaisesRegex(
                    SystemExit, "must be an integer from -1 through 2147483647"):
                normalize_fixture_save_transforms(
                    [{
                        "kind": "bandit_clear_site_evidence",
                        "player_save": "survivor.sav",
                        "site_id": "camp-selected",
                        "next_near_tick_minutes": invalid_value,
                    }],
                    manifest_path=Path("manifest.json"),
                )


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

    def test_selective_overmap_npc_removal_preserves_bystanders(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir) / "World"
            overmaps_dir = world_dir / "overmaps"
            overmaps_dir.mkdir(parents=True)
            overmap_path = overmaps_dir / "o.0.0.zzip"
            overmap_path.write_bytes(b"fixture")
            plain_path = overmaps_dir / "o.0.0"
            plain_path.write_text("fixture", encoding="utf-8")
            payload = {
                "npcs": [
                    {"id": 4, "name": "orphaned scout", "location": [10, 11, 0]},
                    {"id": 5, "name": "camp member", "location": [10, 240, 0]},
                ],
            }

            with (
                mock.patch("startup_harness.load_player_abs_omt", return_value=([0, 0, 0], [0, 0, 0])),
                mock.patch(
                    "startup_harness.extract_overmap_payload",
                    return_value=(plain_path, "# version 1", payload),
                ),
                mock.patch("startup_harness.write_overmap_payload") as write_payload,
                mock.patch("startup_harness.cleanup_extracted_overmap"),
            ):
                report = apply_remove_overmap_npcs_transform(
                    world_dir,
                    {
                        "player_save": "player.sav.zzip",
                        "scan_all_overmaps": True,
                        "npc_ids": [4],
                    },
                )

        self.assertEqual([npc["id"] for npc in payload["npcs"]], [5])
        self.assertEqual(report["requested_npc_ids"], [4])
        self.assertEqual(report["removed_count"], 1)
        self.assertEqual(report["removed_npcs"][0]["name"], "orphaned scout")
        write_payload.assert_called_once()

    def test_release_candidate_fixture_removes_only_orphaned_scout(self) -> None:
        resolved = resolve_fixture_payload(
            "release_candidate_roaming_v0_2026-08-01",
            "live-debug",
        )
        removals = [
            transform
            for transform in resolved["save_transforms"]
            if transform["kind"] == "remove_overmap_npcs"
        ]

        self.assertEqual(len(removals), 1)
        self.assertEqual(removals[0]["npc_ids"], [4])
        self.assertTrue(removals[0]["scan_all_overmaps"])

    def test_basecamp_assignment_repair_preserves_game_owned_schedule(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir) / "World"
            overmaps_dir = world_dir / "overmaps"
            overmaps_dir.mkdir(parents=True)
            overmap_path = overmaps_dir / "o.0.0.zzip"
            overmap_path.write_bytes(b"fixture")
            plain_path = overmaps_dir / "o.0.0"
            plain_path.write_text("fixture", encoding="utf-8")
            katharina_job = {"task_priorities": {"ACT_CAMP_PATROL": 8, "ACT_MOVE_LOOT": 8}}
            robbie_job = {"task_priorities": {"ACT_CAMP_PATROL": 6, "ACT_MOVE_LOOT": 4}}
            payload = {
                "npcs": [
                    {
                        "id": 2,
                        "name": "Katharina Leach",
                        "assigned_camp": [140, 41, 0],
                        "attitude": 3,
                        "mission": 0,
                        "chair_pos": [3, 4, 0],
                        "job": katharina_job,
                        "chatbin": {"first_topic": "TALK_FRIEND"},
                    },
                    {
                        "id": 3,
                        "name": "Robbie Knox",
                        "assigned_camp": [140, 41, 0],
                        "attitude": 0,
                        "mission": 11,
                        "chair_pos": [4, 4, 0],
                        "job": robbie_job,
                        "chatbin": {"first_topic": "TALK_FRIEND_CAMP_RESIDENT"},
                    },
                ],
            }

            with (
                mock.patch(
                    "startup_harness.extract_overmap_payload",
                    return_value=(plain_path, "# version 1", payload),
                ),
                mock.patch("startup_harness.write_overmap_payload") as write_payload,
                mock.patch("startup_harness.cleanup_extracted_overmap"),
            ):
                report = apply_repair_basecamp_npc_assignments_transform(
                    world_dir,
                    {
                        "npc_ids": [2, 3],
                        "assigned_camp_omt": [140, 41, 0],
                    },
                )

        self.assertEqual([npc["mission"] for npc in payload["npcs"]], [11, 11])
        self.assertEqual([npc["attitude"] for npc in payload["npcs"]], [0, 0])
        self.assertEqual([npc["chair_pos"] for npc in payload["npcs"]], [None, None])
        self.assertIs(payload["npcs"][0]["job"], katharina_job)
        self.assertIs(payload["npcs"][1]["job"], robbie_job)
        self.assertFalse(report["schedule_or_job_priorities_modified"])
        write_payload.assert_called_once()

    def test_release_candidate_repairs_both_assignments_without_schedule_fields(self) -> None:
        resolved = resolve_fixture_payload(
            "release_candidate_roaming_v0_2026-08-01",
            "live-debug",
        )
        repairs = [
            transform
            for transform in resolved["save_transforms"]
            if transform["kind"] == "repair_basecamp_npc_assignments"
        ]

        self.assertEqual(len(repairs), 1)
        self.assertEqual(repairs[0]["npc_ids"], [2, 3])
        self.assertEqual(repairs[0]["assigned_camp_omt"], [140, 41, 0])
        self.assertNotIn("job", repairs[0])
        self.assertNotIn("task_priorities", repairs[0])

    def test_release_candidate_includes_stalker_outside_initial_reality_bubble(self) -> None:
        resolved = resolve_fixture_payload(
            "release_candidate_roaming_v0_2026-08-01",
            "live-debug",
        )
        hordes = {
            transform["monster_id"]: transform
            for transform in resolved["save_transforms"]
            if transform["kind"] == "horde_entity_near_player"
        }

        self.assertEqual(
            set(hordes),
            {"mon_zombie_rider", "mon_spawn_raptor", "mon_writhing_stalker"},
        )
        self.assertEqual(hordes["mon_writhing_stalker"]["offset_ms"], [72, 72, 0])
        self.assertGreater(max(abs(value) for value in hordes["mon_writhing_stalker"]["offset_ms"][:2]), 60)

    def test_phase4_observer_handoff_inherits_cloak_and_adds_only_clairvoyance(self) -> None:
        resolved = resolve_fixture_payload(
            "bandit_phase4_ecology_observer_handoff_v0_2026-08-05",
            "live-debug",
        )
        mutation_transforms = [
            transform
            for transform in resolved["save_transforms"]
            if transform["kind"] == "player_mutations"
        ]
        scenario = load_scenario("manual.phase4_ecology_observer_mcw")

        self.assertEqual(
            [transform["mutations"] for transform in mutation_transforms[-2:]],
            [["DEBUG_CLOAK"], ["DEBUG_CLAIRVOYANCE"]],
        )
        self.assertEqual(
            mutation_transforms[-1],
            {
                "kind": "player_mutations",
                "player_save": "#Wm9yYWlkYSBWaWNr.sav.zzip",
                "mutations": ["DEBUG_CLAIRVOYANCE"],
            },
        )
        self.assertEqual(scenario["profile"], "dev-harness")
        self.assertEqual(scenario["world"], "McWilliams")
        self.assertEqual(scenario["fixture"], resolved["source_chain"][0][1])
        self.assertIn("--launch-only", scenario["recommended_test_command"])

    def test_phase4_signal_matrix_observes_natural_dispatch_before_return(self) -> None:
        scenario = load_scenario("bandit.phase4_structural_signal_matrix_live_mcw")
        steps = list(scenario["steps"])
        labels = [step["label"] for step in steps]
        self.assertEqual(
            scenario["fixture"],
            "bandit_phase4_ecology_observer_handoff_v0_2026-08-05",
        )
        self.assertLess(
            labels.index("audit_phase4_sound_fact"),
            labels.index("audit_no_returned_signal_lead_before_physical_return"),
        )
        for removed_label in (
            "open_natural_phase4_ecology_overmap",
            "move_ecology_cursor_to_natural_dispatch",
            "select_and_capture_natural_phase4_dispatch",
            "open_debug_menu_for_natural_ecology_watch",
            "open_ecology_observer_console",
            "arm_default_selected_phase_watch",
            "record_natural_phase4_ecology_incident",
            "audit_player_save_mtime_before_signal_matrix_save",
            "open_save_prompt_after_signal_matrix",
            "confirm_save_after_signal_matrix",
            "audit_player_save_mtime_after_signal_matrix_save",
            "audit_saved_returned_smoke_lead",
            "audit_saved_returned_light_lead",
            "audit_saved_returned_sound_lead",
        ):
            self.assertNotIn(removed_label, labels)
        self.assertNotIn("stage_physical_fire_on_target_omt", labels)
        self.assertNotIn("audit_phase4_three_active_signal_facts", labels)
        self.assertIn("wait_5_minutes_for_abstract_watch_arrival", labels)
        self.assertIn("audit_phase4_sound_fact", labels)
        west_relocation = labels.index("relocate_passive_avatar_three_omt_west_before_signal_approach")
        guarded_waits = [
            index for index, step in enumerate(steps)
            if step["kind"] == "long_wait" and index > west_relocation
        ]
        self.assertTrue(guarded_waits)
        for wait_index in guarded_waits:
            self.assertEqual(
                steps[wait_index - 1]["kind"],
                "debug_run_local_reality_safety_preflight",
            )
        self.assertLess(
            labels.index("relocate_passive_avatar_three_omt_west_before_signal_approach"),
            labels.index("preflight_local_reality_before_signal_approach_wait"),
        )
        self.assertLess(
            labels.index("relocate_passive_avatar_three_omt_east_for_signal_return"),
            labels.index("preflight_local_reality_before_carried_sound_wait"),
        )
        self.assertIn("target remains loaded", scenario["description"])
        self.assertIn("approach/watch remain abstract", scenario["description"])
        harness_source = (HARNESS_DIR / "startup_harness.py").read_text(encoding="utf-8")
        self.assertIn("missing, malformed, or unsafe preflight output blocks", harness_source)
        self.assertIn("audit_local_reality_safety_preflight_text", harness_source)
        self.assertIn("same_z_possible_reach", harness_source)
        self.assertNotIn("ecology_incident", scenario["evidence_contract"]["observer_artifact_requirement"])
        return_audit = steps[labels.index("audit_phase4_structural_signal_physical_return")]
        self.assertIn(["scheduler_hour=172", "members_returned=2"], return_audit["required_line_patterns"])
        self.assertIn(["structural outing returned signal leads=1"], return_audit["required_line_patterns"])
        self.assertEqual(
            sum(
                "origin=returned_report" in group
                for group in return_audit["required_line_patterns"]
            ),
            1,
        )
        self.assertEqual(labels[-2:], [
            "audit_phase4_structural_signal_physical_return",
            "audit_phase4_returned_signal_leads_have_no_player_token",
        ])

    def test_local_reality_safety_preflight_parser_fails_closed(self) -> None:
        summary = (
            "bandit_live_world local_reality_safety_preflight observer=(1,2,0) "
            "resident_monsters={count} outcome={outcome}"
        )
        resident = (
            "bandit_live_world local_reality_safety_resident identity=mon_deer@(3,2,0) "
            "type=mon_deer position=(3,2,0) senses=infrared:no hostile=no visible=no "
            "contact=no same_z_possible_reach=yes attack_target=other_or_none goal=none "
            "verdict={verdict}"
        )
        self.assertEqual(
            audit_local_reality_safety_preflight_text(summary.format(count=0, outcome="safe"))["status"],
            "required_state_present",
        )
        self.assertEqual(
            audit_local_reality_safety_preflight_text(
                summary.format(count=1, outcome="safe") + "\n" + resident.format(verdict="safe")
            )["status"],
            "required_state_present",
        )
        for text in (
            "",
            summary.format(count=1, outcome="safe"),
            summary.format(count=2, outcome="safe") + "\n" + resident.format(verdict="safe"),
            summary.format(count=1, outcome="unsafe") + "\n" + resident.format(verdict="safe"),
            summary.format(count=1, outcome="safe") + "\n" + resident.format(verdict="unsafe"),
            summary.format(count=1, outcome="safe") + "\n" + resident.format(verdict="safe").replace(
                "goal=none ", ""
            ),
        ):
            self.assertEqual(
                audit_local_reality_safety_preflight_text(text)["status"],
                "required_state_missing",
            )

    def test_safe_local_reality_preflight_closes_console_after_parse(self) -> None:
        summary = (
            "bandit_live_world local_reality_safety_preflight observer=(1,2,0) "
            "resident_monsters=0 outcome=safe\n"
        )
        events: List[Any] = []
        with tempfile.TemporaryDirectory(prefix="local_reality_preflight_") as temp_dir:
            run_dir = Path(temp_dir)
            artifact_log = run_dir / "debug.log"
            artifact_log.write_text("", encoding="utf-8")

            def invoke_preflight(*_args: Any, **_kwargs: Any) -> None:
                events.append("analyzer")
                artifact_log.write_text(summary, encoding="utf-8")

            def record_press(*args: Any, **kwargs: Any) -> None:
                events.append(("press", args, kwargs))

            with (
                mock.patch(
                    "startup_harness.debug_run_local_reality_safety_preflight",
                    side_effect=invoke_preflight,
                ),
                mock.patch(
                    "startup_harness.peekaboo_press_sequence",
                    side_effect=record_press,
                ),
                mock.patch("startup_harness.time.sleep"),
            ):
                reports = execute_probe_steps(
                    42,
                    run_dir,
                    [{
                        "kind": "debug_run_local_reality_safety_preflight",
                        "label": "safe_preflight",
                        "delay_ms": 123,
                        "menu_settle_seconds": 0.2,
                    }],
                    profile="dev-harness",
                    world="McWilliams",
                    artifact_log=artifact_log,
                )

        self.assertEqual(events[0], "analyzer")
        self.assertEqual(
            events[1],
            ("press", (42, ["escape"]), {"delay_ms": 123}),
        )
        self.assertEqual(reports[0]["metadata"]["status"], "required_state_present")
        self.assertTrue(reports[0]["debug_console_closed"])

    def test_unsafe_local_reality_preflight_does_not_close_console(self) -> None:
        events: List[Any] = []
        with tempfile.TemporaryDirectory(prefix="local_reality_preflight_unsafe_") as temp_dir:
            run_dir = Path(temp_dir)
            artifact_log = run_dir / "debug.log"
            artifact_log.write_text("", encoding="utf-8")

            def invoke_preflight(*_args: Any, **_kwargs: Any) -> None:
                events.append("analyzer")
                artifact_log.write_text(
                    "bandit_live_world local_reality_safety_preflight observer=(1,2,0) "
                    "resident_monsters=0 outcome=unsafe\n",
                    encoding="utf-8",
                )

            with (
                mock.patch(
                    "startup_harness.debug_run_local_reality_safety_preflight",
                    side_effect=invoke_preflight,
                ),
                mock.patch("startup_harness.peekaboo_press_sequence") as press,
            ):
                reports = execute_probe_steps(
                    42,
                    run_dir,
                    [{
                        "kind": "debug_run_local_reality_safety_preflight",
                        "label": "unsafe_preflight",
                    }],
                    profile="dev-harness",
                    world="McWilliams",
                    artifact_log=artifact_log,
                )

        self.assertEqual(events, ["analyzer"])
        self.assertEqual(reports[-1]["abort"]["guard"], "local_reality_safety_preflight")
        press.assert_not_called()

    def test_phase4_signal_matrix_derives_schema10_watch_footing_and_clocks(self) -> None:
        fixture_name = "bandit_phase4_ecology_observer_handoff_v0_2026-08-05"
        resolved = resolve_fixture_payload(fixture_name, "live-debug")
        scenario = load_scenario("bandit.phase4_structural_signal_matrix_live_mcw")
        transforms = list(resolved["save_transforms"])
        steps = list(scenario["steps"])
        labels = [step["label"] for step in steps]

        last_clear_index = max(
            index
            for index, transform in enumerate(transforms)
            if transform["kind"] == "bandit_clear_site_evidence"
        )
        effective_leads = [
            transform
            for transform in transforms[last_clear_index + 1 :]
            if transform["kind"] == "bandit_camp_map_lead"
        ]
        self.assertEqual(len(effective_leads), 1)
        lead = effective_leads[0]
        self.assertEqual(
            (
                lead["kind_value"],
                lead["lead_id"],
                lead["target_omt"],
                lead["source_key"],
            ),
            (
                "terrain_opportunity",
                "overmap_special:bandit_camp@140,51,0:terrain_opportunity:137,49,0:road",
                [137, 49, 0],
                "fixture_phase4_structural_signal_matrix:terrain_fit:road",
            ),
        )

        def unchecked_camp_score(target_omt: List[int], source_key: str) -> int:
            marker = ":terrain_fit:"
            marker_at = source_key.find(marker)
            terrain = "unknown" if marker_at < 0 else source_key[marker_at + len(marker) :]
            terrain_fit = 1000 if terrain == "road" else 333
            static_risk = 200 if terrain == "road" else 300
            distance = max(abs(target_omt[0] - 140), abs(target_omt[1] - 51))
            route_quality = max(0, min(1000, 1000 - distance * 1000 // 18))
            weighted_score = (
                300 * 333
                + 250 * 1000
                + 100 * 1000
                + 100 * route_quality
                + 100 * terrain_fit
                - 450 * static_risk
            )
            return max(0, min(1000, weighted_score // 1000))

        score_order = (
            unchecked_camp_score(lead["target_omt"], lead["source_key"]),
            unchecked_camp_score([136, 51, 0], "terrain_opportunity:terrain_fit:road"),
            unchecked_camp_score(lead["target_omt"], "fixture_phase4_structural_signal_matrix"),
        )
        self.assertEqual(score_order, (543, 537, 431))

        terrain_points = [(137, 49, 0), (138, 52, 0), (139, 51, 0)]
        overmap_x, overmap_y, _ = overmap_file_coords_from_abs_omt(terrain_points[0])
        overmap_path = (
            resolved["save_src"]
            / "McWilliams"
            / "overmaps"
            / f"o.{overmap_x}.{overmap_y}.zzip"
        )
        plain_path, _version_line, overmap_payload = extract_overmap_payload(overmap_path)
        try:
            terrain_layer = decode_overmap_layer(
                overmap_payload["layers"][overmap_layer_index(0)],
                context=f"{overmap_path} z=0",
            )
            saved_terrain = []
            for point in terrain_points:
                point_overmap_x, point_overmap_y, local_point = overmap_file_coords_from_abs_omt(point)
                self.assertEqual((point_overmap_x, point_overmap_y), (overmap_x, overmap_y))
                saved_terrain.append(terrain_layer[overmap_flat_index(local_point)])
        finally:
            cleanup_extracted_overmap(
                plain_path,
                keep=not bool(overmap_payload.get("_created_plain", False)),
            )
        self.assertEqual(saved_terrain, ["road_ns", "forest", "field"])

        camp = (140, 51, 0)
        target = (137, 49, 0)
        watch = (138, 52, 0)
        approach = (139, 51, 0)
        target_footprint = {target}
        shared_route = [camp, approach, watch, approach, camp]
        self.assertEqual(max(abs(target[0] - watch[0]), abs(target[1] - watch[1])), 3)
        self.assertTrue(target_footprint.isdisjoint(shared_route))
        self.assertEqual(max(abs(approach[0] - watch[0]), abs(approach[1] - watch[1])), 1)

        player_save = "#Wm9yYWlkYSBWaWNr.sav.zzip"
        positioning_transforms = [
            transform
            for transform in transforms
            if transform["kind"] in {
                "player_near_overmap_special",
                "player_location_offset_ms",
            }
        ]
        self.assertEqual(
            [transform["kind"] for transform in positioning_transforms],
            ["player_near_overmap_special", "player_location_offset_ms"],
        )
        self.assertEqual(positioning_transforms[0]["offset_omt"], [0, -12, 0])
        self.assertEqual(positioning_transforms[1]["offset_ms"], [-36, 239, 0])
        with tempfile.TemporaryDirectory() as temp_dir:
            installed_world = Path(temp_dir) / "McWilliams"
            shutil.copytree(resolved["save_src"] / "McWilliams", installed_world)
            self.assertEqual(
                load_player_abs_omt(installed_world, player_save),
                ((140, 41, 0), [3372, 996, 0]),
            )
            apply_fixture_save_transforms(installed_world, [positioning_transforms[0]])
            self.assertEqual(
                load_player_abs_omt(installed_world, player_save),
                ((140, 39, 0), [3372, 948, 0]),
            )
            apply_fixture_save_transforms(installed_world, [positioning_transforms[1]])
            installed_player_omt, player_abs_ms = load_player_abs_omt(
                installed_world, player_save
            )
        self.assertEqual((installed_player_omt, player_abs_ms), ((139, 49, 0), [3336, 1187, 0]))
        west_relocation = steps[labels.index("relocate_passive_avatar_three_omt_west_before_signal_approach")]
        east_relocation = steps[labels.index("relocate_passive_avatar_three_omt_east_for_signal_return")]
        self.assertEqual(west_relocation["keys"], ["left", "left", "left", "enter"])
        self.assertEqual(east_relocation["keys"], ["right", "right", "right", "enter"])
        self.assertLess(
            labels.index("relocate_passive_avatar_three_omt_west_before_signal_approach"),
            labels.index("wait_1_hour_for_schema10_signal_approach"),
        )
        self.assertLess(
            labels.index("audit_phase4_sound_fact"),
            labels.index("relocate_passive_avatar_three_omt_east_for_signal_return"),
        )
        self.assertEqual(
            steps[labels.index("audit_no_local_handoff_through_signal_observation")]["since_label"],
            "relocate_passive_avatar_three_omt_west_before_signal_approach",
        )
        west_player_abs_ms = [player_abs_ms[0] - 72, player_abs_ms[1], player_abs_ms[2]]
        self.assertEqual(west_player_abs_ms, [3264, 1187, 0])
        source_abs_ms = [west_player_abs_ms[0] + 36, west_player_abs_ms[1], west_player_abs_ms[2]]
        self.assertEqual((west_player_abs_ms, source_abs_ms), ([3264, 1187, 0], [3300, 1187, 0]))
        self.assertEqual(tuple(value // 24 for value in source_abs_ms[:2]) + (0,), target)
        west_avatar_omt = (136, 49, 0)
        loaded_handoff_radius_sm = 4
        two_west_avatar_omt = (137, 49, 0)
        three_west_route_delta_sm = (
            2 * abs(approach[0] - west_avatar_omt[0]),
            2 * abs(approach[1] - west_avatar_omt[1]),
        )
        two_west_route_delta_sm = (
            2 * abs(approach[0] - two_west_avatar_omt[0]),
            2 * abs(approach[1] - two_west_avatar_omt[1]),
        )
        self.assertEqual(three_west_route_delta_sm, (6, 4))
        self.assertEqual(two_west_route_delta_sm, (4, 4))
        self.assertEqual(max(two_west_route_delta_sm), loaded_handoff_radius_sm)
        self.assertGreater(max(three_west_route_delta_sm), loaded_handoff_radius_sm)
        self.assertEqual(
            (2 * abs(target[0] - west_avatar_omt[0]), 2 * abs(target[1] - west_avatar_omt[1])),
            (2, 0),
        )

        def distance_to_omt(point_ms: List[int], omt: tuple[int, int, int]) -> int:
            distances = []
            for axis in range(2):
                lower = omt[axis] * 24
                upper = lower + 23
                distances.append(max(lower - point_ms[axis], point_ms[axis] - upper, 0))
            return max(distances)

        self.assertEqual(
            (
                distance_to_omt(west_player_abs_ms, target),
                distance_to_omt(west_player_abs_ms, approach),
                distance_to_omt(west_player_abs_ms, camp),
                distance_to_omt(west_player_abs_ms, watch),
            ),
            (24, 72, 96, 61),
        )
        resolved_profile = resolve_profile_snapshot_payload(
            scenario["profile_snapshot"], scenario["profile_snapshot_profile"]
        )
        profile_options = {
            str(entry["name"]): str(entry["value"])
            for entry in json.loads(
                (resolved_profile["snapshot_dir"] / "config" / "options.json").read_text(
                    encoding="utf-8"
                )
            )
        }
        profile_options.update(resolve_scenario_profile_option_overrides(scenario))
        self.assertEqual(profile_options["TILES"], "ASCIITiles")
        matching_tilesets = []
        for tileset_path in (resolved_profile["snapshot_dir"] / "gfx").glob("*/tileset.txt"):
            metadata = {
                key.strip(): value.strip()
                for line in tileset_path.read_text(encoding="utf-8").splitlines()
                if ":" in line and not line.lstrip().startswith("#")
                for key, value in [line.split(":", 1)]
            }
            if metadata.get("VIEW") == profile_options["TILES"]:
                matching_tilesets.append((tileset_path.parent, metadata))
        self.assertEqual(len(matching_tilesets), 1)
        tileset_dir, tileset_metadata = matching_tilesets[0]
        tileset_config = json.loads(
            (tileset_dir / tileset_metadata["JSON"]).read_text(encoding="utf-8")
        )
        is_tileset_isometric = bool(tileset_config["tile_info"][-1].get("iso", False))
        self.assertFalse(is_tileset_isometric)
        screen_direction_delta = {
            "left": (-1, 0),
            "right": (1, 0),
        }
        rotate_direction_vec = (1, 2, 5, 0, 4, 8, 3, 6, 7)

        def profile_world_delta(keys: List[str]) -> tuple[int, int]:
            world_x = 0
            world_y = 0
            for key in keys:
                screen_x, screen_y = screen_direction_delta[key]
                if is_tileset_isometric:
                    direction_number = (screen_y + 1) * 3 + screen_x + 1
                    rotated = rotate_direction_vec[direction_number]
                    screen_x = rotated % 3 - 1
                    screen_y = rotated // 3 - 1
                world_x += screen_x
                world_y += screen_y
            return world_x, world_y

        for setup_label in ("stage_active_explosive_after_watch_arrival",):
            setup = steps[labels.index(setup_label)]
            self.assertEqual(len(setup["target_keys"]), 36)
            self.assertEqual(set(setup["target_keys"]), {"right"})
            world_delta = profile_world_delta(setup["target_keys"])
            placed_abs_ms = [
                west_player_abs_ms[0] + world_delta[0],
                west_player_abs_ms[1] + world_delta[1],
                west_player_abs_ms[2],
            ]
            placed_omt = tuple(value // 24 for value in placed_abs_ms[:2]) + (0,)
            self.assertEqual(world_delta, (36, 0))
            self.assertEqual(placed_abs_ms, source_abs_ms)
            self.assertEqual(placed_omt, target)
            self.assertIn(
                "36 screen-right non-isometric EDITMAP actions",
                setup["expected_visible_fact"],
            )

        expected_clocks = {
            "wait_1_hour_for_schema10_signal_approach": ("scheduler_hour=167", "now_minutes=10020"),
            "wait_5_minutes_for_abstract_watch_arrival": ("scheduler_hour=168", "now_minutes=10080"),
            "wait_5_minutes_for_post_arrival_sound_fact": ("now_minutes=10085",),
            "wait_1_hour_with_carried_sound_fact": ("scheduler_hour=169", "now_minutes=10140"),
            "wait_second_1_hour_for_sound_pair_report_transition": ("scheduler_hour=170", "now_minutes=10200"),
            "wait_third_1_hour_for_sound_pair_homeward_transition": ("scheduler_hour=171", "now_minutes=10260"),
            "wait_final_1_hour_for_signal_pair_physical_return": ("scheduler_hour=172", "now_minutes=10320"),
        }
        for label, clock_patterns in expected_clocks.items():
            patterns = steps[labels.index(label)]["artifact_state_patterns"]
            self.assertTrue(all(pattern in patterns for pattern in clock_patterns))
        homeward_transition = steps[labels.index(
            "wait_third_1_hour_for_sound_pair_homeward_transition"
        )]
        final_return = steps[labels.index("wait_final_1_hour_for_signal_pair_physical_return")]
        completed_assessment = (
            "structural outing completed its watch assessment "
            "lead=overmap_special:bandit_camp@140,51,0:terrain_opportunity:137,49,0:road"
        )
        secured_return = (
            "structural outing secured its normal watch report and began return "
            "lead=overmap_special:bandit_camp@140,51,0:terrain_opportunity:137,49,0:road"
        )
        self.assertIn(completed_assessment, homeward_transition["artifact_state_patterns"])
        self.assertNotIn(secured_return, homeward_transition["artifact_state_patterns"])
        self.assertIn(secured_return, final_return["artifact_state_patterns"])
        self.assertIn("structural outing returned signal leads=1", final_return["artifact_state_patterns"])
        self.assertIn("members_returned=2", final_return["artifact_state_patterns"])
        self.assertIn("origin=returned_report", final_return["artifact_state_patterns"])
        expected_staging_clocks = {
            "wait_30_minutes_toward_schema10_watch_boundary": 10050,
            "wait_first_5_minutes_toward_schema10_watch_boundary": 10055,
            "wait_second_5_minutes_toward_schema10_watch_boundary": 10060,
            "wait_third_5_minutes_toward_schema10_watch_boundary": 10065,
            "wait_fourth_5_minutes_toward_schema10_watch_boundary": 10070,
            "wait_fifth_5_minutes_to_stage_schema10_watch_boundary": 10075,
        }
        for label, now_minutes in expected_staging_clocks.items():
            self.assertEqual(
                steps[labels.index(label)]["artifact_state_patterns"],
                [
                    "bandit_live_world signal scan: signal_packet=no "
                    f"kind=smoke/fire/light now_minutes={now_minutes}"
                ],
            )
        staging_wait_minutes = sum(
            {"30m": 30, "5m": 5}[steps[labels.index(label)]["expected_duration"]]
            for label in labels
            if label.endswith("toward_schema10_watch_boundary")
            or label == "wait_fifth_5_minutes_to_stage_schema10_watch_boundary"
        )
        self.assertEqual(staging_wait_minutes, 55)
        self.assertLess(labels.index("wait_5_minutes_for_abstract_watch_arrival"), labels.index("stage_active_explosive_after_watch_arrival"))
        self.assertLess(labels.index("stage_active_explosive_after_watch_arrival"), labels.index("wait_5_minutes_for_post_arrival_sound_fact"))
        self.assertLess(
            labels.index("audit_phase4_sound_fact"),
            labels.index("audit_no_local_handoff_through_signal_observation"),
        )
        self.assertLess(
            labels.index("audit_no_local_handoff_through_signal_observation"),
            labels.index("relocate_passive_avatar_three_omt_east_for_signal_return"),
        )
        self.assertLess(labels.index("audit_no_returned_signal_lead_before_physical_return"), labels.index("wait_final_1_hour_for_signal_pair_physical_return"))
        self.assertNotIn("screen-left", json.dumps(scenario, sort_keys=True))
        self.assertNotIn("136,51,0", json.dumps(scenario, sort_keys=True))

    def test_phase4_signal_matrix_log_groups_use_normalized_runner_matchers(self) -> None:
        scenario = load_scenario("bandit.phase4_structural_signal_matrix_live_mcw")
        steps = {step["label"]: step for step in scenario["steps"]}
        immediate_step = steps["wait_5_minutes_for_post_arrival_sound_fact"]
        immediate_patterns = normalize_screen_text_patterns(
            immediate_step["artifact_state_patterns"]
        )
        self.assertEqual(
            immediate_patterns,
            [
                "bandit_live_world sound_observation sites=1 active=1 callbacks=1 recorded=1 facts=1",
                "bandit_live_world signal_adapter request current_omt=(138,52,0)",
                "current_inbounds=no field_signals=0 sound_events=1",
                "sound_adapter event source_omt=(137,49,0) current_omt=(138,52,0) supported=yes permitted=yes",
                "now_minutes=10085",
            ],
        )
        positive_step = steps["audit_phase4_sound_fact"]
        positive_groups = [
            normalize_screen_text_patterns(group)
            for group in positive_step["required_line_patterns"]
        ]
        positive_lines = [
            "bandit_live_world sound_observation sites=1 active=1 callbacks=1 recorded=1 facts=1",
            "bandit_live_world signal_adapter request current_omt=(138,52,0) map_origin_omt=(133,46,0) current_inbounds=no field_signals=0 sound_events=1",
            "bandit_live_world sound_adapter event source_omt=(137,49,0) current_omt=(138,52,0) supported=yes permitted=yes emitted_minutes=10081 now_minutes=10085",
        ]
        arrival_step = steps["audit_phase4_abstract_watch_arrival"]
        arrival_groups = [
            normalize_screen_text_patterns(group)
            for group in arrival_step["required_line_patterns"]
        ]
        forbidden_groups = [
            normalize_screen_text_patterns(group)
            for group in steps["audit_phase4_returned_signal_leads_have_no_player_token"][
                "forbidden_line_patterns"
            ]
        ]

        with tempfile.TemporaryDirectory() as tmp:
            run_dir = Path(tmp)
            artifact_log = run_dir / "debug.log"
            request_row = (
                "bandit_live_world signal_adapter request current_omt=(138,52,0) "
                "map_origin_omt=(133,46,0) current_inbounds=no field_signals=0 sound_events=1"
            )
            completion_lines = [
                request_row,
                "bandit_live_world sound_adapter event source_omt=(137,49,0) current_omt=(138,52,0) supported=yes permitted=yes emitted_minutes=10081 now_minutes=10085",
                "bandit_live_world sound_observation sites=1 active=1 callbacks=1 recorded=1 facts=1",
            ]
            artifact_log.write_text("\n".join(positive_lines + completion_lines) + "\n", encoding="utf-8")
            completion = capture_wait_artifact_delta(
                artifact_log,
                run_dir,
                "completion",
                "completion_poll",
                0,
                immediate_patterns,
                filter_debug_noise=False,
            )
            completion_match = artifact_delta_matches_all_patterns(completion)

            def audit_match_for(label: str, lines: List[str]) -> Dict[str, Any]:
                log_path = run_dir / f"{label}.audit.log"
                log_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
                return audit_log_contains(
                    run_dir,
                    label,
                    artifact_log=log_path,
                    artifact_baseline=0,
                    patterns=[],
                    required_line_patterns=positive_groups,
                )

            wrong_audit_watch = audit_match_for(
                "wrong_audit_watch",
                [positive_lines[0], positive_lines[1].replace("current_omt=(138,52,0)", "current_omt=(139,52,0)"), positive_lines[2]],
            )
            audit_inbounds_yes = audit_match_for(
                "audit_inbounds_yes",
                [positive_lines[0], positive_lines[1].replace("current_inbounds=no", "current_inbounds=yes"), positive_lines[2]],
            )
            audit_field_signals_nonzero = audit_match_for(
                "audit_field_signals_nonzero",
                [positive_lines[0], positive_lines[1].replace("field_signals=0", "field_signals=1"), positive_lines[2]],
            )
            audit_sound_count_wrong = audit_match_for(
                "audit_sound_count_wrong",
                [positive_lines[0], positive_lines[1].replace("sound_events=1", "sound_events=2"), positive_lines[2]],
            )
            audit_wrong_source = audit_match_for(
                "audit_wrong_source",
                [positive_lines[0], positive_lines[1], positive_lines[2].replace("source_omt=(137,49,0)", "source_omt=(136,49,0)")],
            )
            audit_nonpermitted = audit_match_for(
                "audit_nonpermitted",
                [positive_lines[0], positive_lines[1], positive_lines[2].replace("permitted=yes", "permitted=no")],
            )
            audit_wrong_minute = audit_match_for(
                "audit_wrong_minute",
                [positive_lines[0], positive_lines[1], positive_lines[2].replace("now_minutes=10085", "now_minutes=10084")],
            )
            audit_missing_minute = audit_match_for(
                "audit_missing_minute",
                [positive_lines[0], positive_lines[1], positive_lines[2].replace(" now_minutes=10085", "")],
            )
            audit_missing_recorder = audit_match_for("audit_missing_recorder", positive_lines[1:])
            audit_zero_recorder = audit_match_for(
                "audit_zero_recorder",
                [positive_lines[0].replace("facts=1", "facts=0"), positive_lines[1], positive_lines[2]],
            )

            def completion_match_for(label: str, lines: List[str]) -> Dict[str, Any]:
                log_path = run_dir / f"{label}.log"
                log_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
                report = capture_wait_artifact_delta(
                    log_path,
                    run_dir,
                    label,
                    "completion_poll",
                    0,
                    immediate_patterns,
                    filter_debug_noise=False,
                )
                return artifact_delta_matches_all_patterns(report)

            wrong_watch = completion_match_for(
                "wrong_watch",
                [request_row.replace("current_omt=(138,52,0)", "current_omt=(139,52,0)")] + completion_lines[1:],
            )
            inbounds_yes = completion_match_for(
                "inbounds_yes",
                [request_row.replace("current_inbounds=no", "current_inbounds=yes")] + completion_lines[1:],
            )
            field_signals_nonzero = completion_match_for(
                "field_signals_nonzero",
                [request_row.replace("field_signals=0", "field_signals=1")] + completion_lines[1:],
            )
            sound_count_wrong = completion_match_for(
                "sound_count_wrong",
                [request_row.replace("sound_events=1", "sound_events=2")] + completion_lines[1:],
            )
            recorder_absent = completion_match_for("recorder_absent", completion_lines[:2])
            positive = audit_log_contains(
                run_dir,
                "positive",
                artifact_log=artifact_log,
                artifact_baseline=0,
                patterns=[],
                required_line_patterns=positive_groups,
            )
            arrival_log = run_dir / "arrival.log"
            arrival_log.write_text(
                "\n".join([
                    "bandit_live_world structural maintenance scheduler_hour=168 arrivals=1",
                    "bandit_live_world evidence debug: now_minutes=10080 site_cap=8",
                    "- structural outing reached selected watch without resolving remote lead=overmap_special:bandit_camp@140,51,0:terrain_opportunity:137,49,0:road",
                ]) + "\n",
                encoding="utf-8",
            )
            arrival = audit_log_contains(
                run_dir,
                "arrival",
                artifact_log=arrival_log,
                artifact_baseline=0,
                patterns=[],
                required_line_patterns=arrival_groups,
            )
            clean_negative = audit_log_not_contains(
                run_dir,
                "clean_negative",
                artifact_log=artifact_log,
                artifact_baseline=0,
                patterns=[],
                forbidden_line_patterns=forbidden_groups,
            )
            with artifact_log.open("a", encoding="utf-8") as stream:
                stream.write("lead origin=returned_report source_key=player@139,49,0\n")
            forbidden = audit_log_not_contains(
                run_dir,
                "forbidden",
                artifact_log=artifact_log,
                artifact_baseline=0,
                patterns=[],
                forbidden_line_patterns=forbidden_groups,
            )

        self.assertEqual(positive["status"], "required_state_present")
        for rejected_audit in (
            wrong_audit_watch,
            audit_inbounds_yes,
            audit_field_signals_nonzero,
            audit_sound_count_wrong,
            audit_wrong_source,
            audit_nonpermitted,
            audit_wrong_minute,
            audit_missing_minute,
            audit_missing_recorder,
            audit_zero_recorder,
        ):
            self.assertEqual(rejected_audit["status"], "required_state_missing")
        self.assertTrue(completion_match["matched"])
        for rejected in (
            wrong_watch,
            inbounds_yes,
            field_signals_nonzero,
            sound_count_wrong,
            recorder_absent,
        ):
            self.assertFalse(rejected["matched"])
        self.assertEqual(
            wrong_watch["missing_patterns"],
            [
                "bandit_live_world signal_adapter request current_omt=(138,52,0)",
            ],
        )
        self.assertEqual(inbounds_yes["missing_patterns"], ["current_inbounds=no field_signals=0 sound_events=1"])
        self.assertEqual(field_signals_nonzero["missing_patterns"], ["current_inbounds=no field_signals=0 sound_events=1"])
        self.assertEqual(sound_count_wrong["missing_patterns"], ["current_inbounds=no field_signals=0 sound_events=1"])
        self.assertEqual(
            recorder_absent["missing_patterns"],
            ["bandit_live_world sound_observation sites=1 active=1 callbacks=1 recorded=1 facts=1"],
        )
        self.assertEqual(arrival["status"], "required_state_present")
        self.assertEqual(clean_negative["status"], "required_state_present")
        self.assertEqual(forbidden["status"], "required_state_missing")
        self.assertEqual(
            forbidden["observed_forbidden_items"],
            ["origin=returned_report && player@"],
        )

    def test_phase4_quiet_wait_allows_exact_artifact_meridiem_ambiguity(self) -> None:
        scenario = load_scenario("bandit.phase4_quiet_former_radar_live_mcw")
        wait = next(step for step in scenario["steps"] if step["kind"] == "long_wait")

        self.assertTrue(wait["allow_exact_artifact_meridiem_ambiguity"])

    def test_phase4_road_day_serializes_approach_before_exact_watch_visibility(self) -> None:
        fixture_name = "bandit_phase4_visibility_road_day_v0_2026-08-04"
        resolved = resolve_fixture_payload(fixture_name, "live-debug")
        scenario = load_scenario("bandit.phase4_visibility_road_day_live_mcw")
        steps = list(scenario["steps"])
        labels = [step["label"] for step in steps]
        approach_label = "wait_first_1_hour_for_phase4_visibility_road_day_approach"
        visibility_label = "wait_second_1_hour_for_phase4_visibility_road_day_observer"
        audit_label = "audit_phase4_visibility_road_day_artifact"
        approach = steps[labels.index(approach_label)]
        visibility = steps[labels.index(visibility_label)]
        audit = steps[labels.index(audit_label)]

        self.assertEqual(
            resolved["source_chain"][:3],
            [
                ("live-debug", fixture_name),
                (
                    "live-debug",
                    "bandit_structural_bounty_idle_camp_forest_town_v0_2026-04-30",
                ),
                (
                    "live-debug",
                    "bandit_extortion_reopen_local_contact_mcw_v0_2026-04-24",
                ),
            ],
        )
        transforms = list(resolved["save_transforms"])
        last_clear_index = max(
            index
            for index, transform in enumerate(transforms)
            if transform["kind"] == "bandit_clear_site_evidence"
        )
        effective_leads = [
            transform
            for transform in transforms[last_clear_index + 1 :]
            if transform["kind"] == "bandit_camp_map_lead"
        ]
        self.assertEqual(len(effective_leads), 1)
        self.assertEqual(effective_leads[0]["kind_value"], "terrain_opportunity")
        self.assertEqual(
            effective_leads[0]["lead_id"],
            "overmap_special:bandit_camp@140,51,0:terrain_opportunity:137,49,0:road",
        )
        self.assertNotIn(
            "#lead:structural_bounty:road@137,49,0",
            json.dumps(effective_leads, sort_keys=True),
        )
        self.assertEqual(effective_leads[0]["target_omt"], [137, 49, 0])
        self.assertEqual(
            effective_leads[0]["source_key"],
            "fixture_phase4_visibility_road_horde:terrain_fit:road",
        )

        def unchecked_camp_score(target_omt: List[int], source_key: str) -> int:
            marker = ":terrain_fit:"
            marker_at = source_key.find(marker)
            terrain = "unknown" if marker_at < 0 else source_key[marker_at + len(marker) :]
            terrain_fit = 1000 if terrain == "road" else 333
            static_risk = 200 if terrain == "road" else 300
            distance = max(abs(target_omt[0] - 140), abs(target_omt[1] - 51))
            route_quality = max(0, min(1000, 1000 - distance * 1000 // 18))
            weighted_score = (
                300 * 333
                + 250 * 1000
                + 150 * 0
                + 100 * 1000
                + 100 * route_quality
                + 100 * terrain_fit
                - 450 * static_risk
                - 150 * 0
            )
            return max(0, min(1000, weighted_score // 1000))

        corrected_score = unchecked_camp_score(
            effective_leads[0]["target_omt"], effective_leads[0]["source_key"]
        )
        scanned_road_score = unchecked_camp_score(
            [136, 51, 0], "terrain_opportunity:terrain_fit:road"
        )
        stale_control_score = unchecked_camp_score(
            effective_leads[0]["target_omt"], "fixture_phase4_visibility_road_horde"
        )
        self.assertEqual(
            (corrected_score, scanned_road_score, stale_control_score),
            (543, 537, 431),
        )
        self.assertGreater(corrected_score, scanned_road_score)
        self.assertLess(stale_control_score, scanned_road_score)

        player_omt = [140, 39, 0]
        horde_transforms = [
            transform
            for transform in transforms
            if transform["kind"] == "horde_entity_near_player"
        ]
        expected_horde_offsets = [
            [-72, 240, 0],
            [-71, 240, 0],
            [-70, 240, 0],
        ]
        self.assertEqual(
            [transform["offset_ms"] for transform in horde_transforms],
            expected_horde_offsets,
        )
        self.assertEqual(
            [transform["destination_offset_ms"] for transform in horde_transforms],
            expected_horde_offsets,
        )
        self.assertEqual(
            [transform["monster_id"] for transform in horde_transforms],
            ["mon_zombie"] * 3,
        )
        self.assertEqual(
            [
                [
                    (player_omt[axis] * 24 + (12 if axis < 2 else 0) + offset[axis])
                    // (24 if axis < 2 else 1)
                    for axis in range(3)
                ]
                for offset in expected_horde_offsets
            ],
            [[137, 49, 0]] * 3,
        )

        terrain_points = [
            (137, 49, 0),
            (138, 52, 0),
            (138, 51, 0),
            (137, 50, 0),
        ]
        overmap_x, overmap_y, _ = overmap_file_coords_from_abs_omt(terrain_points[0])
        overmap_path = (
            resolved["save_src"]
            / "McWilliams"
            / "overmaps"
            / f"o.{overmap_x}.{overmap_y}.zzip"
        )
        plain_path, _version_line, overmap_payload = extract_overmap_payload(overmap_path)
        try:
            terrain_layer = decode_overmap_layer(
                overmap_payload["layers"][overmap_layer_index(0)],
                context=f"{overmap_path} z=0",
            )
            saved_terrain = []
            for point in terrain_points:
                point_overmap_x, point_overmap_y, local_point = (
                    overmap_file_coords_from_abs_omt(point)
                )
                self.assertEqual((point_overmap_x, point_overmap_y), (overmap_x, overmap_y))
                saved_terrain.append(terrain_layer[overmap_flat_index(local_point)])
        finally:
            cleanup_extracted_overmap(
                plain_path,
                keep=not bool(overmap_payload.get("_created_plain", False)),
            )
        self.assertEqual(saved_terrain, ["road_ns", "forest", "field", "road_wn"])

        self.assertLess(labels.index(approach_label), labels.index(visibility_label))
        self.assertLess(labels.index(visibility_label), labels.index(audit_label))
        self.assertEqual(
            approach["artifact_state_patterns"],
            ["scheduler_hour=156", "now_minutes=9360"],
        )
        self.assertEqual(
            visibility["artifact_state_patterns"],
            [
                "scheduler_hour=157",
                "now_minutes=9420",
                "bandit_live_world structural_visibility:",
            ],
        )
        self.assertIn(
            "first_forward_omt=(137,49,0)",
            audit["required_line_patterns"][1],
        )
        self.assertIn(
            "first_forward_distance=3",
            audit["required_line_patterns"][1],
        )
        horde_preflight = steps[
            labels.index("preflight_phase4_visibility_road_day_horde")
        ]
        self.assertEqual(
            horde_preflight["required_hordes"],
            [
                {"monster_id": "mon_zombie", "offset_ms": offset}
                for offset in expected_horde_offsets
            ],
        )
        site_preflight = steps[
            labels.index("preflight_phase4_visibility_road_day_site")
        ]
        self.assertEqual(site_preflight["required_lead_kind"], "terrain_opportunity")
        serialized_scenario = json.dumps(scenario, sort_keys=True)
        exact_dispatch_lead = (
            "lead=overmap_special:bandit_camp@140,51,0"
            ":terrain_opportunity:137,49,0:road"
        )
        stale_bounty_dispatch_lead = (
            "lead=overmap_special:bandit_camp@140,51,0"
            "#lead:structural_bounty:road@137,49,0"
        )
        self.assertEqual(serialized_scenario.count(exact_dispatch_lead), 3)
        self.assertNotIn(stale_bounty_dispatch_lead, serialized_scenario)
        self.assertIn("threat_omt=(137,49,0)", serialized_scenario)
        self.assertNotIn("136,51,0", serialized_scenario)

    def test_phase4_forest_no_optic_day_is_target_only_negative_control(self) -> None:
        fixture_name = "bandit_phase4_visibility_forest_no_optic_day_v0_2026-08-04"
        road_fixture_name = "bandit_phase4_visibility_road_day_v0_2026-08-04"
        resolved = resolve_fixture_payload(fixture_name, "live-debug")
        scenario = load_scenario("bandit.phase4_visibility_forest_no_optic_day_live_mcw")
        steps = list(scenario["steps"])
        labels = [step["label"] for step in steps]

        self.assertEqual(
            resolved["source_chain"][:3],
            [
                ("live-debug", fixture_name),
                ("live-debug", road_fixture_name),
                (
                    "live-debug",
                    "bandit_structural_bounty_idle_camp_forest_town_v0_2026-04-30",
                ),
            ],
        )
        manifest_path = (
            HARNESS_DIR
            / "fixtures"
            / "saves"
            / "live-debug"
            / fixture_name
            / "manifest.json"
        )
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        expected_terrain_transform = {
            "kind": "overmap_terrain_id_at_abs_omt",
            "player_save": "#Wm9yYWlkYSBWaWNr.sav.zzip",
            "abs_omt": [137, 49, 0],
            "terrain_id": "forest",
        }
        self.assertEqual(manifest["source_fixture"], road_fixture_name)
        self.assertEqual(manifest["save_transforms"], [expected_terrain_transform])

        transforms = list(resolved["save_transforms"])
        terrain_transforms = [
            transform
            for transform in transforms
            if transform["kind"] == "overmap_terrain_id_at_abs_omt"
        ]
        self.assertEqual(terrain_transforms, [expected_terrain_transform])
        self.assertEqual(
            [
                transform
                for transform in transforms
                if transform["kind"] == "basecamp_assigned_npc_items"
            ],
            [],
        )

        last_clear_index = max(
            index
            for index, transform in enumerate(transforms)
            if transform["kind"] == "bandit_clear_site_evidence"
        )
        effective_leads = [
            transform
            for transform in transforms[last_clear_index + 1 :]
            if transform["kind"] == "bandit_camp_map_lead"
        ]
        self.assertEqual(len(effective_leads), 1)
        self.assertEqual(effective_leads[0]["kind_value"], "terrain_opportunity")
        self.assertEqual(effective_leads[0]["target_id"], "road")
        self.assertEqual(effective_leads[0]["target_omt"], [137, 49, 0])
        exact_dispatch_lead = (
            "lead=overmap_special:bandit_camp@140,51,0"
            ":terrain_opportunity:137,49,0:road"
        )
        self.assertEqual(effective_leads[0]["lead_id"], exact_dispatch_lead[5:])

        expected_horde_offsets = [[-72, 240, 0], [-71, 240, 0], [-70, 240, 0]]
        horde_transforms = [
            transform
            for transform in transforms
            if transform["kind"] == "horde_entity_near_player"
        ]
        self.assertEqual(
            [transform["offset_ms"] for transform in horde_transforms],
            expected_horde_offsets,
        )
        horde_preflight = steps[
            labels.index("preflight_phase4_visibility_forest_no_optic_day_horde")
        ]
        self.assertEqual(
            horde_preflight["required_hordes"],
            [
                {"monster_id": "mon_zombie", "offset_ms": offset}
                for offset in expected_horde_offsets
            ],
        )
        site_preflight = steps[
            labels.index("preflight_phase4_visibility_forest_no_optic_day_site")
        ]
        self.assertEqual(site_preflight["required_lead_kind"], "terrain_opportunity")
        self.assertEqual(site_preflight["required_lead_target_id"], "road")

        first_wait_label = (
            "wait_first_6_hours_toward_phase4_visibility_forest_no_optic_day_due"
        )
        dispatch_label = (
            "wait_second_6_hours_for_phase4_visibility_forest_no_optic_day_dispatch"
        )
        approach_label = (
            "wait_first_1_hour_for_phase4_visibility_forest_no_optic_day_approach"
        )
        visibility_label = (
            "wait_second_1_hour_for_phase4_visibility_forest_no_optic_day_observer"
        )
        audit_label = "audit_phase4_visibility_forest_no_optic_day_artifact"
        first_wait = steps[labels.index(first_wait_label)]
        dispatch = steps[labels.index(dispatch_label)]
        approach = steps[labels.index(approach_label)]
        visibility = steps[labels.index(visibility_label)]
        audit = steps[labels.index(audit_label)]
        self.assertEqual(first_wait["artifact_state_patterns"], [
            "scheduler_hour=149",
            "now_minutes=8940",
        ])
        self.assertEqual(dispatch["artifact_state_patterns"], [
            "scheduler_hour=155",
            "now_minutes=9300",
            "structural maintenance dispatched site=overmap_special:bandit_camp@140,51,0",
            exact_dispatch_lead,
        ])
        self.assertEqual(approach["artifact_state_patterns"], [
            "scheduler_hour=156",
            "now_minutes=9360",
        ])
        self.assertEqual(visibility["artifact_state_patterns"], [
            "scheduler_hour=157",
            "now_minutes=9420",
            "bandit_live_world structural_visibility:",
        ])
        self.assertLess(labels.index(dispatch_label), labels.index(approach_label))
        self.assertLess(labels.index(approach_label), labels.index(visibility_label))
        self.assertLess(labels.index(visibility_label), labels.index(audit_label))

        self.assertEqual(
            audit["required_line_patterns"],
            [
                [
                    "structural maintenance dispatched site="
                    "overmap_special:bandit_camp@140,51,0",
                    exact_dispatch_lead,
                ],
                [
                    "bandit_live_world structural_visibility:",
                    "site=overmap_special:bandit_camp@140,51,0",
                    "current_omt=(138,52,0)",
                    "weather=clear",
                    "sight_penalty=1",
                    "optic=no",
                    "sight_points=3",
                    "first_forward_omt=(137,49,0)",
                    "first_forward_distance=3",
                    "first_forward_acquired=no",
                    "outcome=no_visible_threat",
                    "threat_omt=none",
                ],
            ],
        )
        serialized_scenario = json.dumps(scenario, sort_keys=True)
        for stale_assumption in (
            "#lead:structural_bounty:forest@138,52,0",
            "current_omt=(139,51,0)",
            "first_forward_omt=(138,52,0)",
            "first_forward_distance=1",
            "optic=yes",
            "sight_points=6",
            "first_forward_acquired=yes",
            "outcome=observed",
            "threat_omt=(137,49,0)",
        ):
            self.assertNotIn(stale_assumption, serialized_scenario)

    def test_phase4_forest_optic_day_reuses_accepted_route_with_target_only_forest(self) -> None:
        fixture_name = "bandit_phase4_visibility_forest_optic_day_v0_2026-08-04"
        road_fixture_name = "bandit_phase4_visibility_road_day_v0_2026-08-04"
        resolved = resolve_fixture_payload(fixture_name, "live-debug")
        scenario = load_scenario("bandit.phase4_visibility_forest_optic_day_live_mcw")
        steps = list(scenario["steps"])
        labels = [step["label"] for step in steps]

        self.assertEqual(
            resolved["source_chain"][:3],
            [
                ("live-debug", fixture_name),
                ("live-debug", road_fixture_name),
                (
                    "live-debug",
                    "bandit_structural_bounty_idle_camp_forest_town_v0_2026-04-30",
                ),
            ],
        )
        manifest_path = (
            HARNESS_DIR
            / "fixtures"
            / "saves"
            / "live-debug"
            / fixture_name
            / "manifest.json"
        )
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        self.assertEqual(manifest["source_fixture"], road_fixture_name)
        self.assertEqual(
            [transform["kind"] for transform in manifest["save_transforms"]],
            ["overmap_terrain_id_at_abs_omt"] + ["basecamp_assigned_npc_items"] * 5,
        )

        transforms = list(resolved["save_transforms"])
        terrain_transforms = [
            transform
            for transform in transforms
            if transform["kind"] == "overmap_terrain_id_at_abs_omt"
        ]
        self.assertEqual(terrain_transforms, [{
            "kind": "overmap_terrain_id_at_abs_omt",
            "player_save": "#Wm9yYWlkYSBWaWNr.sav.zzip",
            "abs_omt": [137, 49, 0],
            "terrain_id": "forest",
        }])
        forest_definitions = json.loads(
            (
                HARNESS_DIR.parents[1]
                / "data"
                / "json"
                / "overmap"
                / "overmap_terrain"
                / "overmap_terrain_hardcoded.json"
            ).read_text(encoding="utf-8")
        )
        forest_definition = next(
            entry for entry in forest_definitions
            if entry.get("type") == "overmap_terrain" and entry.get("id") == "forest"
        )
        self.assertEqual(forest_definition["see_cost"], "spaced_high")

        inventory_transforms = [
            transform
            for transform in transforms
            if transform["kind"] == "basecamp_assigned_npc_items"
        ]
        self.assertEqual([transform["npc_id"] for transform in inventory_transforms], list(range(4, 9)))
        self.assertEqual(
            [transform["offset_ms"] for transform in inventory_transforms],
            [[offset, 288, 0] for offset in range(5)],
        )
        self.assertTrue(all(
            transform["items"] == [{"typeid": "binoculars", "count": 1}]
            for transform in inventory_transforms
        ))

        last_clear_index = max(
            index
            for index, transform in enumerate(transforms)
            if transform["kind"] == "bandit_clear_site_evidence"
        )
        effective_leads = [
            transform
            for transform in transforms[last_clear_index + 1 :]
            if transform["kind"] == "bandit_camp_map_lead"
        ]
        self.assertEqual(len(effective_leads), 1)
        self.assertEqual(effective_leads[0]["kind_value"], "terrain_opportunity")
        self.assertEqual(effective_leads[0]["target_omt"], [137, 49, 0])
        self.assertEqual(
            effective_leads[0]["lead_id"],
            "overmap_special:bandit_camp@140,51,0:terrain_opportunity:137,49,0:road",
        )
        horde_transforms = [
            transform
            for transform in transforms
            if transform["kind"] == "horde_entity_near_player"
        ]
        expected_horde_offsets = [[-72, 240, 0], [-71, 240, 0], [-70, 240, 0]]
        self.assertEqual(
            [transform["offset_ms"] for transform in horde_transforms],
            expected_horde_offsets,
        )

        dispatch = steps[
            labels.index("wait_second_6_hours_for_phase4_visibility_forest_optic_day_dispatch")
        ]
        approach_label = "wait_first_1_hour_for_phase4_visibility_forest_optic_day_approach"
        visibility_label = "wait_second_1_hour_for_phase4_visibility_forest_optic_day_observer"
        audit_label = "audit_phase4_visibility_forest_optic_day_artifact"
        approach = steps[labels.index(approach_label)]
        visibility = steps[labels.index(visibility_label)]
        audit = steps[labels.index(audit_label)]
        self.assertEqual(dispatch["artifact_state_patterns"][:2], [
            "scheduler_hour=155",
            "now_minutes=9300",
        ])
        self.assertEqual(approach["artifact_state_patterns"], [
            "scheduler_hour=156",
            "now_minutes=9360",
        ])
        self.assertEqual(visibility["artifact_state_patterns"], [
            "scheduler_hour=157",
            "now_minutes=9420",
            "bandit_live_world structural_visibility:",
        ])
        self.assertLess(labels.index(approach_label), labels.index(visibility_label))
        self.assertLess(labels.index(visibility_label), labels.index(audit_label))
        exact_visibility = audit["required_line_patterns"][1]
        for required in (
            "current_omt=(138,52,0)",
            "weather=clear",
            "sight_penalty=1",
            "optic=yes",
            "sight_points=6",
            "first_forward_omt=(137,49,0)",
            "first_forward_distance=3",
            "first_forward_acquired=yes",
            "outcome=observed",
            "threat_omt=(137,49,0)",
        ):
            self.assertIn(required, exact_visibility)
        serialized_scenario = json.dumps(scenario, sort_keys=True)
        self.assertNotIn("#lead:structural_bounty:forest@138,52,0", serialized_scenario)
        self.assertNotIn("current_omt=(139,51,0)", serialized_scenario)
        self.assertNotIn("first_forward_distance=1", serialized_scenario)

    def test_phase4_forest_optic_fog_binds_accepted_exact_result(self) -> None:
        fixture_name = "bandit_phase4_visibility_forest_optic_fog_v0_2026-08-04"
        day_fixture_name = "bandit_phase4_visibility_forest_optic_day_v0_2026-08-04"
        road_fixture_name = "bandit_phase4_visibility_road_day_v0_2026-08-04"
        resolved = resolve_fixture_payload(fixture_name, "live-debug")
        scenario = load_scenario("bandit.phase4_visibility_forest_optic_fog_live_mcw")
        steps = list(scenario["steps"])
        labels = [step["label"] for step in steps]

        self.assertEqual(
            resolved["source_chain"][:4],
            [
                ("live-debug", fixture_name),
                ("live-debug", day_fixture_name),
                ("live-debug", road_fixture_name),
                (
                    "live-debug",
                    "bandit_structural_bounty_idle_camp_forest_town_v0_2026-04-30",
                ),
            ],
        )
        manifest_path = (
            HARNESS_DIR
            / "fixtures"
            / "saves"
            / "live-debug"
            / fixture_name
            / "manifest.json"
        )
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        self.assertEqual(manifest["source_fixture"], day_fixture_name)
        self.assertEqual(manifest["save_transforms"], [{
            "kind": "game_turn",
            "player_save": "#Wm9yYWlkYSBWaWNr.sav.zzip",
            "turn": 5360400,
            "shift_queued_eocs": True,
        }])

        transforms = list(resolved["save_transforms"])
        terrain_transforms = [
            transform
            for transform in transforms
            if transform["kind"] == "overmap_terrain_id_at_abs_omt"
        ]
        self.assertEqual(terrain_transforms, [{
            "kind": "overmap_terrain_id_at_abs_omt",
            "player_save": "#Wm9yYWlkYSBWaWNr.sav.zzip",
            "abs_omt": [137, 49, 0],
            "terrain_id": "forest",
        }])
        inventory_transforms = [
            transform
            for transform in transforms
            if transform["kind"] == "basecamp_assigned_npc_items"
        ]
        self.assertEqual(
            [transform["npc_id"] for transform in inventory_transforms],
            list(range(4, 9)),
        )
        self.assertTrue(all(
            transform["items"] == [{"typeid": "binoculars", "count": 1}]
            for transform in inventory_transforms
        ))

        last_clear_index = max(
            index
            for index, transform in enumerate(transforms)
            if transform["kind"] == "bandit_clear_site_evidence"
        )
        effective_leads = [
            transform
            for transform in transforms[last_clear_index + 1 :]
            if transform["kind"] == "bandit_camp_map_lead"
        ]
        self.assertEqual(len(effective_leads), 1)
        self.assertEqual(effective_leads[0]["kind_value"], "terrain_opportunity")
        self.assertEqual(effective_leads[0]["target_omt"], [137, 49, 0])
        exact_dispatch_lead = (
            "lead=overmap_special:bandit_camp@140,51,0"
            ":terrain_opportunity:137,49,0:road"
        )
        self.assertEqual(effective_leads[0]["lead_id"], exact_dispatch_lead[5:])

        expected_horde_offsets = [[-72, 240, 0], [-71, 240, 0], [-70, 240, 0]]
        horde_transforms = [
            transform
            for transform in transforms
            if transform["kind"] == "horde_entity_near_player"
        ]
        self.assertEqual(
            [transform["offset_ms"] for transform in horde_transforms],
            expected_horde_offsets,
        )
        horde_preflight = steps[
            labels.index("preflight_phase4_visibility_forest_optic_fog_horde")
        ]
        self.assertEqual(
            horde_preflight["required_hordes"],
            [
                {"monster_id": "mon_zombie", "offset_ms": offset}
                for offset in expected_horde_offsets
            ],
        )
        site_preflight = steps[
            labels.index("preflight_phase4_visibility_forest_optic_fog_site")
        ]
        self.assertEqual(site_preflight["required_lead_kind"], "terrain_opportunity")
        self.assertEqual(site_preflight["required_lead_target_id"], "road")

        first_wait = steps[
            labels.index("wait_first_6_hours_toward_phase4_visibility_forest_optic_fog_due")
        ]
        dispatch_label = "wait_second_6_hours_for_phase4_visibility_forest_optic_fog_dispatch"
        approach_label = "wait_first_1_hour_for_phase4_visibility_forest_optic_fog_approach"
        visibility_label = "wait_second_1_hour_for_phase4_visibility_forest_optic_fog_observer"
        audit_label = "audit_phase4_visibility_forest_optic_fog_artifact"
        dispatch = steps[labels.index(dispatch_label)]
        approach = steps[labels.index(approach_label)]
        visibility = steps[labels.index(visibility_label)]
        audit = steps[labels.index(audit_label)]
        self.assertEqual(first_wait["artifact_state_patterns"], [
            "scheduler_hour=174",
            "now_minutes=10440",
        ])
        self.assertEqual(dispatch["artifact_state_patterns"], [
            "scheduler_hour=180",
            "now_minutes=10800",
            "structural maintenance dispatched site=overmap_special:bandit_camp@140,51,0",
            exact_dispatch_lead,
        ])
        self.assertEqual(approach["artifact_state_patterns"], [
            "scheduler_hour=181",
            "now_minutes=10860",
        ])
        self.assertEqual(visibility["artifact_state_patterns"], [
            "scheduler_hour=182",
            "now_minutes=10920",
            "bandit_live_world structural_visibility:",
        ])
        self.assertLess(labels.index(dispatch_label), labels.index(approach_label))
        self.assertLess(labels.index(approach_label), labels.index(visibility_label))
        self.assertLess(labels.index(visibility_label), labels.index(audit_label))
        self.assertEqual(
            audit["required_line_patterns"],
            [
                [
                    "structural maintenance dispatched site="
                    "overmap_special:bandit_camp@140,51,0",
                    exact_dispatch_lead,
                ],
                [
                    "bandit_live_world structural_visibility:",
                    "site=overmap_special:bandit_camp@140,51,0",
                    "current_omt=(138,52,0)",
                    "weather=fog",
                    "sight_penalty=1.7",
                    "optic=yes",
                    "sight_points=3",
                    "first_forward_omt=(137,49,0)",
                    "first_forward_distance=3",
                    "first_forward_acquired=no",
                    "outcome=no_visible_threat",
                    "threat_omt=none",
                ],
            ],
        )
        expected_artifact_patterns = [
            "structural maintenance dispatched site="
            "overmap_special:bandit_camp@140,51,0",
            exact_dispatch_lead,
            "bandit_live_world structural_visibility:",
            "current_omt=(138,52,0)",
            "weather=fog",
            "sight_penalty=1.7",
            "optic=yes",
            "sight_points=3",
            "first_forward_omt=(137,49,0)",
            "first_forward_distance=3",
            "first_forward_acquired=no",
            "outcome=no_visible_threat",
            "threat_omt=none",
        ]
        self.assertEqual(scenario["artifact_patterns"], expected_artifact_patterns)
        exact_visibility = audit["required_line_patterns"][1]
        for generic_prefix in (
            "weather=",
            "sight_penalty=",
            "sight_points=",
            "first_forward_acquired=",
            "outcome=",
        ):
            self.assertNotIn(generic_prefix, exact_visibility)
            self.assertNotIn(generic_prefix, scenario["artifact_patterns"])
        self.assertNotIn("audit_log_not_contains", [step["kind"] for step in steps])

        serialized_scenario = json.dumps(scenario, sort_keys=True)
        for stale_assumption in (
            "#lead:structural_bounty:forest@138,52,0",
            "current_omt=(139,51,0)",
            "first_forward_omt=(138,52,0)",
            "first_forward_distance=1",
        ):
            self.assertNotIn(stale_assumption, serialized_scenario)

    def test_phase4_road_twilight_reuses_exact_watch_for_negative_budget(self) -> None:
        fixture_name = "bandit_phase4_visibility_road_twilight_v0_2026-08-04"
        day_fixture_name = "bandit_phase4_visibility_road_day_v0_2026-08-04"
        resolved = resolve_fixture_payload(fixture_name, "live-debug")
        scenario = load_scenario("bandit.phase4_visibility_road_twilight_live_mcw")
        steps = list(scenario["steps"])
        labels = [step["label"] for step in steps]

        self.assertEqual(
            resolved["source_chain"][:3],
            [
                ("live-debug", fixture_name),
                ("live-debug", day_fixture_name),
                (
                    "live-debug",
                    "bandit_structural_bounty_idle_camp_forest_town_v0_2026-04-30",
                ),
            ],
        )
        fixture_manifest_path = (
            HARNESS_DIR
            / "fixtures"
            / "saves"
            / "live-debug"
            / fixture_name
            / "manifest.json"
        )
        fixture_manifest = json.loads(fixture_manifest_path.read_text(encoding="utf-8"))
        self.assertEqual(fixture_manifest["source_fixture"], day_fixture_name)
        self.assertEqual(
            fixture_manifest["save_transforms"],
            [
                {
                    "kind": "game_turn",
                    "player_save": "#Wm9yYWlkYSBWaWNr.sav.zzip",
                    "turn": 5238000,
                    "shift_queued_eocs": True,
                }
            ],
        )

        transforms = list(resolved["save_transforms"])
        last_clear_index = max(
            index
            for index, transform in enumerate(transforms)
            if transform["kind"] == "bandit_clear_site_evidence"
        )
        effective_leads = [
            transform
            for transform in transforms[last_clear_index + 1 :]
            if transform["kind"] == "bandit_camp_map_lead"
        ]
        self.assertEqual(len(effective_leads), 1)
        self.assertEqual(effective_leads[0]["kind_value"], "terrain_opportunity")
        self.assertEqual(
            effective_leads[0]["lead_id"],
            "overmap_special:bandit_camp@140,51,0:terrain_opportunity:137,49,0:road",
        )
        self.assertEqual(effective_leads[0]["target_omt"], [137, 49, 0])
        self.assertEqual(
            effective_leads[0]["source_key"],
            "fixture_phase4_visibility_road_horde:terrain_fit:road",
        )
        self.assertNotIn(
            "#lead:structural_bounty:road@137,49,0",
            json.dumps(effective_leads, sort_keys=True),
        )

        expected_horde_offsets = [
            [-72, 240, 0],
            [-71, 240, 0],
            [-70, 240, 0],
        ]
        horde_transforms = [
            transform
            for transform in transforms
            if transform["kind"] == "horde_entity_near_player"
        ]
        self.assertEqual(
            [transform["offset_ms"] for transform in horde_transforms],
            expected_horde_offsets,
        )
        self.assertEqual(
            [transform["destination_offset_ms"] for transform in horde_transforms],
            expected_horde_offsets,
        )
        self.assertEqual(
            [transform["monster_id"] for transform in horde_transforms],
            ["mon_zombie"] * 3,
        )
        player_omt = [140, 39, 0]
        self.assertEqual(
            [
                [
                    (player_omt[axis] * 24 + (12 if axis < 2 else 0) + offset[axis])
                    // (24 if axis < 2 else 1)
                    for axis in range(3)
                ]
                for offset in expected_horde_offsets
            ],
            [[137, 49, 0]] * 3,
        )
        horde_preflight = steps[
            labels.index("preflight_phase4_visibility_road_twilight_horde")
        ]
        self.assertEqual(
            horde_preflight["required_hordes"],
            [
                {"monster_id": "mon_zombie", "offset_ms": offset}
                for offset in expected_horde_offsets
            ],
        )

        first_wait_label = "wait_first_6_hours_toward_phase4_visibility_road_twilight_due"
        dispatch_label = "wait_second_6_hours_for_phase4_visibility_road_twilight_dispatch"
        approach_label = "wait_first_1_hour_for_phase4_visibility_road_twilight_approach"
        visibility_label = "wait_second_1_hour_for_phase4_visibility_road_twilight_observer"
        audit_label = "audit_phase4_visibility_road_twilight_artifact"
        self.assertLess(labels.index(first_wait_label), labels.index(dispatch_label))
        self.assertLess(labels.index(dispatch_label), labels.index(approach_label))
        self.assertLess(labels.index(approach_label), labels.index(visibility_label))
        self.assertLess(labels.index(visibility_label), labels.index(audit_label))
        exact_dispatch_lead = (
            "lead=overmap_special:bandit_camp@140,51,0"
            ":terrain_opportunity:137,49,0:road"
        )
        self.assertEqual(
            steps[labels.index(first_wait_label)]["artifact_state_patterns"],
            ["scheduler_hour=140", "now_minutes=8400"],
        )
        self.assertEqual(
            steps[labels.index(dispatch_label)]["artifact_state_patterns"],
            [
                "scheduler_hour=146",
                "now_minutes=8760",
                "structural maintenance dispatched site=overmap_special:bandit_camp@140,51,0",
                exact_dispatch_lead,
            ],
        )
        self.assertEqual(
            steps[labels.index(approach_label)]["artifact_state_patterns"],
            ["scheduler_hour=147", "now_minutes=8820"],
        )
        self.assertEqual(
            steps[labels.index(visibility_label)]["artifact_state_patterns"],
            [
                "scheduler_hour=148",
                "now_minutes=8880",
                "bandit_live_world structural_visibility:",
            ],
        )
        site_preflight = steps[
            labels.index("preflight_phase4_visibility_road_twilight_site")
        ]
        self.assertEqual(site_preflight["required_lead_kind"], "terrain_opportunity")
        audit = steps[labels.index(audit_label)]
        self.assertEqual(
            audit["required_line_patterns"],
            [
                [
                    "structural maintenance dispatched site=overmap_special:bandit_camp@140,51,0",
                    exact_dispatch_lead,
                ],
                [
                    "bandit_live_world structural_visibility:",
                    "site=overmap_special:bandit_camp@140,51,0",
                    "current_omt=(138,52,0)",
                    "weather=clear",
                    "optic=no",
                    "sight_points=2",
                    "forward_candidates=1",
                    "first_forward_omt=(137,49,0)",
                    "first_forward_distance=3",
                    "first_forward_acquired=no",
                    "outcome=no_visible_threat",
                    "threat_omt=none",
                ],
            ],
        )
        serialized_scenario = json.dumps(scenario, sort_keys=True)
        self.assertEqual(serialized_scenario.count(exact_dispatch_lead), 3)
        self.assertNotIn("#lead:structural_bounty:road@137,49,0", serialized_scenario)
        self.assertNotIn("136,51,0", serialized_scenario)
        self.assertNotIn("first_forward_acquired=yes", serialized_scenario)
        self.assertNotIn("outcome=observed", serialized_scenario)

    def test_phase4_road_night_binds_measured_light_drizzle_exact_watch(self) -> None:
        fixture_name = "bandit_phase4_visibility_road_night_v0_2026-08-04"
        day_fixture_name = "bandit_phase4_visibility_road_day_v0_2026-08-04"
        resolved = resolve_fixture_payload(fixture_name, "live-debug")
        scenario = load_scenario("bandit.phase4_visibility_road_night_live_mcw")
        steps = list(scenario["steps"])
        labels = [step["label"] for step in steps]

        self.assertEqual(
            resolved["source_chain"][:3],
            [
                ("live-debug", fixture_name),
                ("live-debug", day_fixture_name),
                (
                    "live-debug",
                    "bandit_structural_bounty_idle_camp_forest_town_v0_2026-04-30",
                ),
            ],
        )
        fixture_manifest_path = (
            HARNESS_DIR
            / "fixtures"
            / "saves"
            / "live-debug"
            / fixture_name
            / "manifest.json"
        )
        fixture_manifest = json.loads(fixture_manifest_path.read_text(encoding="utf-8"))
        self.assertEqual(fixture_manifest["source_fixture"], day_fixture_name)
        self.assertEqual(
            fixture_manifest["save_transforms"],
            [
                {
                    "kind": "game_turn",
                    "player_save": "#Wm9yYWlkYSBWaWNr.sav.zzip",
                    "turn": 5310000,
                    "shift_queued_eocs": True,
                }
            ],
        )

        transforms = list(resolved["save_transforms"])
        last_clear_index = max(
            index
            for index, transform in enumerate(transforms)
            if transform["kind"] == "bandit_clear_site_evidence"
        )
        effective_leads = [
            transform
            for transform in transforms[last_clear_index + 1 :]
            if transform["kind"] == "bandit_camp_map_lead"
        ]
        self.assertEqual(len(effective_leads), 1)
        self.assertEqual(effective_leads[0]["kind_value"], "terrain_opportunity")
        self.assertEqual(
            effective_leads[0]["lead_id"],
            "overmap_special:bandit_camp@140,51,0:terrain_opportunity:137,49,0:road",
        )
        self.assertEqual(effective_leads[0]["target_omt"], [137, 49, 0])
        self.assertNotIn(
            "#lead:structural_bounty:road@137,49,0",
            json.dumps(effective_leads, sort_keys=True),
        )

        expected_horde_offsets = [
            [-72, 240, 0],
            [-71, 240, 0],
            [-70, 240, 0],
        ]
        horde_transforms = [
            transform
            for transform in transforms
            if transform["kind"] == "horde_entity_near_player"
        ]
        self.assertEqual(
            [transform["offset_ms"] for transform in horde_transforms],
            expected_horde_offsets,
        )
        self.assertEqual(
            [transform["destination_offset_ms"] for transform in horde_transforms],
            expected_horde_offsets,
        )
        self.assertEqual(
            [transform["monster_id"] for transform in horde_transforms],
            ["mon_zombie"] * 3,
        )
        player_omt = [140, 39, 0]
        self.assertEqual(
            [
                [
                    (player_omt[axis] * 24 + (12 if axis < 2 else 0) + offset[axis])
                    // (24 if axis < 2 else 1)
                    for axis in range(3)
                ]
                for offset in expected_horde_offsets
            ],
            [[137, 49, 0]] * 3,
        )
        horde_preflight = steps[
            labels.index("preflight_phase4_visibility_road_night_horde")
        ]
        self.assertEqual(
            horde_preflight["required_hordes"],
            [
                {"monster_id": "mon_zombie", "offset_ms": offset}
                for offset in expected_horde_offsets
            ],
        )

        first_wait_label = "wait_first_6_hours_toward_phase4_visibility_road_night_due"
        dispatch_label = "wait_second_6_hours_for_phase4_visibility_road_night_dispatch"
        approach_label = "wait_first_1_hour_for_phase4_visibility_road_night_approach"
        visibility_label = "wait_second_1_hour_for_phase4_visibility_road_night_observer"
        audit_label = "audit_phase4_visibility_road_night_artifact"
        self.assertLess(labels.index(first_wait_label), labels.index(dispatch_label))
        self.assertLess(labels.index(dispatch_label), labels.index(approach_label))
        self.assertLess(labels.index(approach_label), labels.index(visibility_label))
        self.assertLess(labels.index(visibility_label), labels.index(audit_label))
        exact_dispatch_lead = (
            "lead=overmap_special:bandit_camp@140,51,0"
            ":terrain_opportunity:137,49,0:road"
        )
        self.assertEqual(
            steps[labels.index(first_wait_label)]["artifact_state_patterns"],
            ["scheduler_hour=160", "now_minutes=9600"],
        )
        self.assertEqual(
            steps[labels.index(dispatch_label)]["artifact_state_patterns"],
            [
                "scheduler_hour=166",
                "now_minutes=9960",
                "structural maintenance dispatched site=overmap_special:bandit_camp@140,51,0",
                exact_dispatch_lead,
            ],
        )
        self.assertEqual(
            steps[labels.index(approach_label)]["artifact_state_patterns"],
            ["scheduler_hour=167", "now_minutes=10020"],
        )
        self.assertEqual(
            steps[labels.index(visibility_label)]["artifact_state_patterns"],
            [
                "scheduler_hour=168",
                "now_minutes=10080",
                "bandit_live_world structural_visibility:",
            ],
        )
        site_preflight = steps[
            labels.index("preflight_phase4_visibility_road_night_site")
        ]
        self.assertEqual(site_preflight["required_lead_kind"], "terrain_opportunity")
        audit = steps[labels.index(audit_label)]
        self.assertEqual(
            audit["required_line_patterns"],
            [
                [
                    "structural maintenance dispatched site=overmap_special:bandit_camp@140,51,0",
                    exact_dispatch_lead,
                ],
                [
                    "bandit_live_world structural_visibility:",
                    "site=overmap_special:bandit_camp@140,51,0",
                    "current_omt=(138,52,0)",
                    "weather=light_drizzle",
                    "sight_penalty=1.01",
                    "optic=no",
                    "sight_points=0",
                    "forward_candidates=1",
                    "first_forward_omt=(137,49,0)",
                    "first_forward_distance=3",
                    "first_forward_acquired=no",
                    "outcome=no_visible_threat",
                    "threat_omt=none",
                ],
            ],
        )
        visibility_patterns = audit["required_line_patterns"][1]
        self.assertIn("sight_penalty=1.01", visibility_patterns)
        self.assertNotIn("sight_penalty=1", visibility_patterns)
        self.assertIn("sight_points=0", visibility_patterns)
        self.assertNotIn("sight_points=1", visibility_patterns)
        serialized_scenario = json.dumps(scenario, sort_keys=True)
        self.assertEqual(serialized_scenario.count(exact_dispatch_lead), 3)
        self.assertIn("weather=light_drizzle", serialized_scenario)
        self.assertIn("sight_penalty=1.01", serialized_scenario)
        self.assertIn("sight_points=0", serialized_scenario)
        self.assertNotIn("weather=cloudy", serialized_scenario)
        self.assertNotIn("136,51,0", serialized_scenario)
        self.assertNotIn("first_forward_acquired=yes", serialized_scenario)
        self.assertNotIn("outcome=observed", serialized_scenario)

    def test_scout_to_decision_observer_fixture_stops_before_natural_lead(self) -> None:
        fixture_name = "bandit_scout_to_decision_observer_southwest_v0_2026-08-07"
        resolved = resolve_fixture_payload(
            fixture_name,
            "live-debug",
        )

        self.assertEqual(
            resolved["source_chain"][:3],
            [
                ("live-debug", fixture_name),
                ("live-debug", "bandit_scout_to_decision_observer_v0_2026-08-06"),
                ("live-debug", "bandit_extortion_reopen_local_contact_mcw_v0_2026-04-24"),
            ],
        )
        manifest_path = (
            HARNESS_DIR
            / "fixtures"
            / "saves"
            / "live-debug"
            / fixture_name
            / "manifest.json"
        )
        derived_manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        self.assertEqual(
            derived_manifest["source_fixture"],
            "bandit_scout_to_decision_observer_v0_2026-08-06",
        )
        self.assertEqual(derived_manifest["source_profile"], "live-debug")
        self.assertEqual(
            derived_manifest["save_transforms"],
            [
                {
                    "kind": "player_location_offset_ms",
                    "player_save": "#Wm9yYWlkYSBWaWNr.sav.zzip",
                    "offset_ms": [0, 24, 0],
                }
            ],
        )
        child_transforms = resolved["save_transforms"][:-1]
        self.assertEqual(
            [transform["kind"] for transform in child_transforms],
            [
                "game_turn",
                "player_near_overmap_special",
                "seed_overmap_special_near_player",
                "bandit_clone_site",
                "bandit_site_roster_shape",
                "bandit_site_roster_shape",
                "bandit_clear_site_evidence",
                "bandit_clear_site_evidence",
                "player_mutations",
            ],
        )
        self.assertNotIn(
            "bandit_camp_map_lead",
            [transform["kind"] for transform in child_transforms],
        )
        source_camp_anchor = [140, 51, 0]
        player_offset = child_transforms[1]["offset_omt"]
        player_omt = [
            source_camp_anchor[index] + player_offset[index]
            for index in range(3)
        ]
        self.assertEqual(player_offset, [22, -16, 0])
        self.assertEqual(player_omt, [162, 35, 0])
        derived_player_omt = [
            player_omt[0] + derived_manifest["save_transforms"][0]["offset_ms"][0] // 24,
            player_omt[1] + derived_manifest["save_transforms"][0]["offset_ms"][1] // 24,
            player_omt[2] + derived_manifest["save_transforms"][0]["offset_ms"][2] // 24,
        ]
        self.assertEqual(derived_player_omt, [162, 36, 0])
        self.assertEqual(
            [
                derived_player_omt[0] + 2,
                derived_player_omt[1] - 1,
                derived_player_omt[2],
            ],
            [164, 35, 0],
        )
        self.assertEqual(child_transforms[2]["offset_omt"], [2, 4, 0])
        self.assertEqual(child_transforms[3]["new_anchor"], [164, 39, 0])
        self.assertEqual(
            child_transforms[3]["new_footprint"],
            [[164, 39, 0], [165, 39, 0], [164, 40, 0], [165, 40, 0]],
        )
        sector_zero_outer_target = [
            child_transforms[3]["new_anchor"][0],
            child_transforms[3]["new_anchor"][1] - 9,
            child_transforms[3]["new_anchor"][2],
        ]
        self.assertEqual(sector_zero_outer_target, [164, 30, 0])
        self.assertEqual(child_transforms[3]["new_anchor"][0], sector_zero_outer_target[0])
        self.assertEqual(player_omt[0] + 2, child_transforms[3]["new_anchor"][0])
        self.assertLess(sector_zero_outer_target[1], player_omt[1])
        self.assertLess(player_omt[1], child_transforms[3]["new_anchor"][1])
        self.assertTrue(child_transforms[3]["reset_shakedown_history"])
        child_transform = child_transforms[-1]
        self.assertEqual(
            child_transform,
            {
                "kind": "player_mutations",
                "player_save": "#Wm9yYWlkYSBWaWNr.sav.zzip",
                "mutations": ["DEBUG_CLAIRVOYANCE"],
            },
        )
        self.assertEqual(child_transform["mutations"], ["DEBUG_CLAIRVOYANCE"])
        self.assertEqual(
            resolved["save_transforms"][-1],
            derived_manifest["save_transforms"][0],
        )

    def test_scout_to_decision_observer_scenario_preserves_causal_boundary(self) -> None:
        scenario = load_scenario("bandit.scout_to_decision_observer_live_mcw")
        steps = list(scenario["steps"])
        labels = [step["label"] for step in steps]
        exact_target = "lead=frontier_probe:0"

        self.assertEqual(
            scenario["fixture"],
            "bandit_scout_to_decision_observer_southwest_v0_2026-08-07",
        )
        self.assertIn("(162,36,0)", scenario["description"])
        self.assertIn("two OMTs west and one OMT south", scenario["description"])
        self.assertIn("(164,35,0)", scenario["description"])
        self.assertNotIn("(166,34,0)", scenario["description"])
        self.assertNotIn("(166,36,0)", scenario["description"])
        self.assertNotIn("(166,35,0)", scenario["description"])
        self.assertNotIn("two OMTs east", scenario["description"])
        self.assertIn("(164,30,0)", scenario["description"])
        self.assertIn(
            "structural maintenance dispatched site=overmap_special:bandit_camp@164,39,0",
            scenario["artifact_patterns"],
        )
        self.assertIn(exact_target, scenario["artifact_patterns"])
        self.assertNotIn(
            "structural outing returned home lead=", scenario["artifact_patterns"]
        )
        self.assertIn(
            "road-connected sector-0 outer target (164,30,0)",
            scenario["evidence_contract"]["preconditions_and_interventions"],
        )
        self.assertEqual(
            scenario["evidence_contract"]["fixture_feasibility_gate"],
            "Before this row can receive causal credit, the production selector must emit "
            "bandit_live_world watch_geography_preflight for the installed fixture; its "
            "bounded-candidate predicate counts, rather than the continuous-road assertion, "
            "decide whether a natural watch footing exists.",
        )
        production_owner = (HARNESS_DIR.parent.parent / "src" / "do_turn.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("bandit_live_world watch_geography_preflight", production_owner)
        self.assertIn("visible=", production_owner)
        self.assertIn("route_reachable=", production_owner)
        self.assertIn("selected_omt=", production_owner)
        self.assertIn("outcome=", production_owner)
        self.assertIn(
            "moves only the player observer from (162,35,0) to (162,36,0)",
            scenario["evidence_contract"]["preconditions_and_interventions"],
        )
        self.assertIn(
            "two OMTs west and one OMT south of the same route waypoint at (164,35,0)",
            scenario["evidence_contract"]["preconditions_and_interventions"],
        )
        preflight = steps[labels.index("preflight_idle_zero_lead_camp")]
        self.assertEqual(
            preflight["required_site_id_contains"],
            "overmap_special:bandit_camp@164,39,0",
        )
        self.assertEqual(preflight["required_max_leads"], 0)
        self.assertEqual(preflight["required_active_group_id_exact"], "")
        self.assertEqual(preflight["required_active_target_id_exact"], "")
        waits = [step for step in steps if step["kind"] == "long_wait"]
        self.assertEqual(
            [(step["choice_key"], step["expected_duration"]) for step in waits],
            [
                ("8", "6h"),
                ("8", "6h"),
                ("5", "1h"),
                ("7", "3h"),
                ("3", "5m"),
                ("8", "6h"),
            ],
        )
        dispatch_wait = steps[labels.index("wait_1_hour_for_real_frontier_dispatch")]
        self.assertEqual(
            dispatch_wait["artifact_state_patterns"],
            ["bandit_live_world watch_geography_preflight"],
        )
        watch_preflight = steps[labels.index("audit_watch_geography_fixture_preflight")]
        self.assertEqual(
            watch_preflight["required_line_patterns"],
            [[
                "bandit_live_world watch_geography_preflight",
                "site=overmap_special:bandit_camp@164,39,0",
                "target=(164,30,0)",
                "footprint=4",
                "candidates=",
                "visible=",
                "route_reachable=",
                "selected_omt=",
                "selected_route_cost=",
            ]],
        )
        self.assertEqual(
            watch_preflight["required_any_line_patterns"],
            [["outcome=selected"], ["outcome=no_bounded_safe_watch_geography"]],
        )
        self.assertTrue(watch_preflight["abort_on_metadata_failure"])
        self.assertLess(
            labels.index("wait_1_hour_for_real_frontier_dispatch"),
            labels.index("audit_watch_geography_fixture_preflight"),
        )
        false_selected = steps[
            labels.index("audit_watch_geography_preflight_rejects_false_selected_route")
        ]
        self.assertEqual(
            false_selected["forbidden_line_patterns"],
            [[
                "bandit_live_world watch_geography_preflight",
                "outcome=selected",
                "selected_omt=none",
            ]],
        )
        self.assertTrue(false_selected["abort_on_metadata_failure"])
        self.assertLess(
            labels.index("audit_watch_geography_fixture_preflight"),
            labels.index("audit_watch_geography_preflight_rejects_false_selected_route"),
        )
        self.assertLess(
            labels.index("wait_3_hours_for_real_pair_handoff"),
            labels.index("wait_5_minutes_through_real_pair_handoff_cadence"),
        )
        self.assertLess(
            labels.index("wait_5_minutes_through_real_pair_handoff_cadence"),
            labels.index("audit_real_pair_handoff_and_cohesion"),
        )
        selection_index = labels.index("select_authoritative_dispatch")
        self.assertLess(
            labels.index("audit_real_pair_handoff_and_cohesion"),
            selection_index,
        )
        self.assertEqual(
            steps[selection_index]["keys"],
            ["right", "up", "up", "up", "["],
        )
        reselection_index = labels.index("settled_reselect_authoritative_dispatch")
        self.assertEqual(reselection_index, selection_index + 1)
        reselection = steps[reselection_index]
        self.assertEqual(reselection["keys"], ["["])
        self.assertTrue(reselection["capture_after"])
        self.assertTrue(reselection["extract_text_after_capture"])
        self.assertEqual(
            reselection["expected_screen_text_after_contains"],
            ["BD-DF9E73"],
        )
        self.assertTrue(reselection["abort_on_screen_text_expectation_failure"])
        self.assertEqual(
            reselection["abort_verdict"],
            "blocked_scout_to_decision_authoritative_dispatch_selection_missing",
        )
        for later_credit_label in (
            "open_console_for_natural_watch",
            "arm_default_capture_and_continue_watch",
            "advance_initial_6_hour_post_observation_window",
            "record_post_window_ecology_incident",
            "audit_fresh_post_window_ecology_incident_pair",
            "audit_same_run_survivor_physical_return",
        ):
            self.assertLess(reselection_index, labels.index(later_credit_label))
        handoff_wait = steps[labels.index("wait_3_hours_for_real_pair_handoff")]
        self.assertEqual(handoff_wait["artifact_state_patterns"], ["bandit_live_world perf:"])
        boundary_wait = steps[labels.index("wait_5_minutes_through_real_pair_handoff_cadence")]
        self.assertEqual(boundary_wait["choice_key"], "3")
        self.assertEqual(boundary_wait["expected_duration"], "5m")
        self.assertEqual(
            boundary_wait["proof_deferred_to_label"],
            "audit_real_pair_handoff_and_cohesion",
        )
        self.assertEqual(boundary_wait["artifact_state_patterns"], ["bandit_live_world perf:"])
        self.assertIn("whole-run log audit", boundary_wait["failure_rule"])
        handoff_audit = steps[labels.index("audit_real_pair_handoff_and_cohesion")]
        self.assertEqual(
            handoff_audit["required_line_patterns"],
            [
                ["bandit_live_world local_handoff committed", "members=2"],
                ["bandit_live_world local_cohesion", "assembled=yes", "abort=no"],
            ],
        )
        self.assertLess(
            labels.index("arm_default_capture_and_continue_watch"),
            labels.index("advance_initial_6_hour_post_observation_window"),
        )
        arm_index = labels.index("arm_default_capture_and_continue_watch")
        arm_step = steps[arm_index]
        self.assertEqual(arm_step["keys"], ["a"])
        self.assertTrue(arm_step["capture_after"])
        self.assertTrue(arm_step["extract_text_after_capture"])
        self.assertEqual(
            arm_step["expected_screen_text_after_contains"],
            ["Armed BD-DF9E73"],
        )
        self.assertTrue(arm_step["abort_on_screen_text_expectation_failure"])
        self.assertEqual(
            arm_step["abort_verdict"],
            "blocked_scout_to_decision_authoritative_watch_arm_missing",
        )
        for later_credit_label in (
            "close_console_before_natural_window",
            "advance_initial_6_hour_post_observation_window",
            "record_post_window_ecology_incident",
            "audit_fresh_post_window_ecology_incident_pair",
            "audit_same_run_survivor_physical_return",
        ):
            self.assertLess(arm_index, labels.index(later_credit_label))
        self.assertLess(
            labels.index("advance_initial_6_hour_post_observation_window"),
            labels.index("record_post_window_ecology_incident"),
        )
        reopen_index = labels.index("reopen_console_after_natural_window")
        self.assertNotIn("rearm_selected_watch_after_natural_window", labels)
        incident_index = labels.index("record_post_window_ecology_incident")
        incident_audit_index = labels.index("audit_fresh_post_window_ecology_incident_pair")
        self.assertEqual(incident_index, reopen_index + 1)
        self.assertEqual(incident_audit_index, incident_index + 1)
        incident_step = steps[incident_index]
        incident_audit = steps[incident_audit_index]
        self.assertEqual(incident_step["keys"], ["r"])
        self.assertTrue(incident_step["record_ecology_incident_baseline"])
        self.assertIn(
            "retained already-armed exact terminal identity",
            incident_step["expected_visible_fact"],
        )
        self.assertEqual(
            incident_step["proof_deferred_to_label"],
            incident_audit["label"],
        )
        self.assertEqual(incident_audit["kind"], "audit_fresh_ecology_incident_pair")
        self.assertEqual(
            incident_audit["publication_step_label"],
            incident_step["label"],
        )
        self.assertTrue(incident_audit["abort_on_metadata_failure"])
        self.assertLess(
            incident_audit_index,
            labels.index("audit_same_run_survivor_physical_return"),
        )
        return_audit_index = labels.index("audit_same_run_survivor_physical_return")
        return_audit = steps[return_audit_index]
        self.assertEqual(
            return_audit["required_line_patterns"],
            [
                ["structural outing returned home lead=", "frontier_probe:0"],
            ],
        )
        self.assertEqual(
            return_audit["required_any_line_patterns"],
            [
                ["bandit_live_world structural maintenance:", "members_returned=1"],
                ["bandit_live_world structural maintenance:", "members_returned=2"],
            ],
        )
        self.assertTrue(return_audit["abort_on_metadata_failure"])
        self.assertEqual(
            return_audit["abort_verdict"],
            "blocked_scout_to_decision_physical_return_not_reached_in_initial_window",
        )
        self.assertLess(
            return_audit_index,
            labels.index("audit_player_save_mtime_before_scout_to_decision_save"),
        )
        self.assertLess(
            return_audit_index,
            labels.index("audit_saved_survivors_home_and_outing_closed"),
        )
        final_wait = steps[
            labels.index("advance_initial_6_hour_post_observation_window")
        ]
        self.assertEqual(
            final_wait["artifact_state_patterns"],
            ["bandit_live_world perf:"],
        )
        self.assertEqual(final_wait["minimum_artifact_elapsed_minutes"], 360)
        press_keys = [
            key
            for step in steps if step["kind"] == "press"
            for key in step["keys"]
        ]
        self.assertNotIn("I", press_keys)
        self.assertNotIn("P", press_keys)
        self.assertNotIn("A", press_keys)
        self.assertNotIn("R", press_keys)
        self.assertEqual(press_keys.count("a"), 1)
        self.assertEqual(press_keys.count("r"), 1)
        self.assertIn(
            "debug_intervention=false",
            scenario["evidence_contract"]["observer_artifact_requirement"],
        )
        final_audit = steps[labels.index("audit_saved_survivors_home_and_outing_closed")]
        self.assertNotIn("required_member_count", final_audit)
        self.assertEqual(final_audit["required_min_home_survivor_count"], 4)
        self.assertNotIn("required_wounded_or_unready_count", final_audit)
        self.assertEqual(final_audit["required_active_outside_count"], 0)
        self.assertEqual(final_audit["required_active_group_id_exact"], "")
        self.assertTrue(final_audit["required_scout_report_present"])
        self.assertFalse(final_audit["required_scout_report_provisional"])
        self.assertNotIn("required_scout_report_min_observations", final_audit)
        self.assertEqual(
            final_audit["required_camp_decision_state"],
            "report_awaiting_assessment",
        )
        self.assertTrue(final_audit["required_report_decision_identity_match"])
        self.assertIn("final non-provisional report", scenario["evidence_contract"]["pass_fail_rule"])
        self.assertIn(
            "not by the bounded wait-menu completion token",
            scenario["evidence_contract"]["pass_fail_rule"],
        )

    def test_scout_to_decision_watch_geography_preflight_matcher_rejects_false_green(self) -> None:
        scenario = load_scenario("bandit.scout_to_decision_observer_live_mcw")
        steps = {step["label"]: step for step in scenario["steps"]}
        preflight = steps["audit_watch_geography_fixture_preflight"]
        false_selected = steps[
            "audit_watch_geography_preflight_rejects_false_selected_route"
        ]
        required_groups = [
            normalize_screen_text_patterns(group)
            for group in preflight["required_line_patterns"]
        ]
        required_any_groups = [
            normalize_screen_text_patterns(group)
            for group in preflight["required_any_line_patterns"]
        ]
        forbidden_groups = [
            normalize_screen_text_patterns(group)
            for group in false_selected["forbidden_line_patterns"]
        ]
        selected_line = (
            "bandit_live_world watch_geography_preflight "
            "site=overmap_special:bandit_camp@164,39,0 target=(164,30,0) "
            "footprint=4 candidates=256 concealed=8 clear=8 visible=1 qualified=1 "
            "nonadjacent=1 route_reads=1 route_reachable=1 selected_omt=(161,27,0) "
            "selected_route_cost=8 outcome=selected"
        )
        rejected_line = (
            "bandit_live_world watch_geography_preflight "
            "site=overmap_special:bandit_camp@164,39,0 target=(164,30,0) "
            "footprint=4 candidates=256 concealed=8 clear=8 visible=0 qualified=0 "
            "nonadjacent=0 route_reads=0 route_reachable=0 selected_omt=none "
            "selected_route_cost=-1 outcome=no_bounded_safe_watch_geography"
        )
        false_green_line = selected_line.replace(
            "selected_omt=(161,27,0)", "selected_omt=none"
        )

        with tempfile.TemporaryDirectory() as tmp:
            run_dir = Path(tmp)
            artifact_log = run_dir / "debug.log"
            artifact_log.write_text(selected_line + "\n", encoding="utf-8")
            selected = audit_log_contains(
                run_dir, "selected", artifact_log=artifact_log, artifact_baseline=0,
                patterns=[], required_line_patterns=required_groups,
                required_any_line_patterns=required_any_groups,
            )
            selected_clean = audit_log_not_contains(
                run_dir, "selected_clean", artifact_log=artifact_log, artifact_baseline=0,
                patterns=[], forbidden_line_patterns=forbidden_groups,
            )
            artifact_log.write_text(rejected_line + "\n", encoding="utf-8")
            rejected = audit_log_contains(
                run_dir, "rejected", artifact_log=artifact_log, artifact_baseline=0,
                patterns=[], required_line_patterns=required_groups,
                required_any_line_patterns=required_any_groups,
            )
            artifact_log.write_text(false_green_line + "\n", encoding="utf-8")
            false_green = audit_log_not_contains(
                run_dir, "false_green", artifact_log=artifact_log, artifact_baseline=0,
                patterns=[], forbidden_line_patterns=forbidden_groups,
            )
            artifact_log.write_text(
                selected_line.replace("outcome=selected", "outcome=unknown") + "\n",
                encoding="utf-8",
            )
            missing_outcome = audit_log_contains(
                run_dir, "missing_outcome", artifact_log=artifact_log, artifact_baseline=0,
                patterns=[], required_line_patterns=required_groups,
                required_any_line_patterns=required_any_groups,
            )

        self.assertEqual(selected["status"], "required_state_present")
        self.assertEqual(selected_clean["status"], "required_state_present")
        self.assertEqual(rejected["status"], "required_state_present")
        self.assertEqual(false_green["status"], "required_state_missing")
        self.assertEqual(missing_outcome["status"], "required_state_missing")

    def test_phase4_target_relocation_observes_same_authoritative_dispatch(self) -> None:
        scenario = load_scenario("bandit.phase4_target_relocation_observer_live_mcw")
        steps = list(scenario["steps"])
        labels = [step["label"] for step in steps]
        exact_target = (
            "overmap_special:bandit_camp@140,51,0:"
            "terrain_opportunity:136,51,0:road"
        )

        self.assertEqual(
            scenario["fixture"],
            "bandit_phase4_ecology_dispatch_observer_v0_2026-08-05",
        )
        preflight = steps[labels.index("preflight_authoritative_relocation_dispatch")]
        postflight = steps[labels.index("audit_authoritative_target_after_player_relocation")]
        self.assertEqual(preflight["required_active_target_id_exact"], exact_target)
        self.assertEqual(postflight["required_active_target_id_exact"], exact_target)
        self.assertEqual(preflight["required_active_outing_generation"], 1)
        self.assertEqual(postflight["required_active_outing_generation"], 1)
        self.assertEqual(preflight["required_active_outing_simulation_owner"], "local")
        self.assertNotIn("required_active_outing_simulation_owner", postflight)

        presses = [step for step in steps if step["kind"] == "press"]
        all_keys = [key for step in presses for key in step["keys"]]
        self.assertNotIn("I", all_keys)
        self.assertNotIn("P", all_keys)
        self.assertEqual(all_keys.count("A"), 2)
        self.assertEqual(all_keys.count("R"), 2)
        self.assertEqual(all_keys.count("."), 1)
        self.assertEqual(
            steps[labels.index("open_debug_long_teleport")]["keys"],
            ["}", "t", "l"],
        )
        teleport_keys = steps[labels.index("teleport_player_twelve_omt_south")]["keys"]
        self.assertEqual(teleport_keys, ["down"] * 12 + ["enter"])
        reselect_keys = steps[labels.index("reselect_post_relocation_dispatch")]["keys"]
        self.assertEqual(reselect_keys, ["up"] * 12 + ["right", "right", "["])
        self.assertLess(
            labels.index("record_pre_relocation_incident"),
            labels.index("teleport_player_twelve_omt_south"),
        )
        self.assertLess(
            labels.index("arm_pre_relocation_watch"),
            labels.index("record_pre_relocation_incident"),
        )
        self.assertLess(
            labels.index("teleport_player_twelve_omt_south"),
            labels.index("record_post_relocation_incident"),
        )
        self.assertLess(
            labels.index("arm_post_relocation_watch"),
            labels.index("record_post_relocation_incident"),
        )
        self.assertIn(
            "empty intervention ledger",
            scenario["evidence_contract"]["observer_artifact_requirement"],
        )

    def test_phase4_decoy_fixture_preserves_zero_value_returned_signal_fields(self) -> None:
        resolved = resolve_fixture_payload(
            "bandit_phase4_decoy_empty_signal_v0_2026-08-05",
            "live-debug",
        )
        signal = next(
            transform for transform in resolved["save_transforms"]
            if transform["kind"] == "bandit_camp_map_lead"
        )

        self.assertEqual(signal["revision"], 1)
        self.assertEqual(signal["kind_value"], "smoke_signal")
        self.assertEqual(signal["origin"], "returned_report")
        self.assertEqual(signal["radius_omt"], 2)
        self.assertEqual(signal["bounty"], 0)
        self.assertEqual(signal["threat"], 0)
        self.assertEqual(signal["routine_activated_minutes"], 0)
        self.assertEqual(signal["next_routine_dispatch_eligible_minutes"], 8280)
        self.assertEqual(signal["last_routine_resolved_minutes"], 3960)
        self.assertEqual(signal["last_checked_minutes"], -1)
        self.assertEqual(signal["target_omt"], [137, 49, 0])
        self.assertEqual(
            signal["source_key"],
            "structural-signal:structural-smoke@(137,49,0)",
        )
        self.assertEqual(resolved["save_transforms"][-1]["kind"], "player_mutations")
        self.assertEqual(
            resolved["save_transforms"][-1]["mutations"],
            ["DEBUG_CLAIRVOYANCE"],
        )
        self.assertEqual(
            resolved["source_chain"][-2:],
            [
                ("live-debug", "bandit_phase4_decoy_empty_signal_v0_2026-08-05"),
                ("live-debug", "bandit_phase4_quiet_current_schema_v0_2026-08-05"),
            ],
        )
        clear = next(
            transform for transform in resolved["save_transforms"]
            if transform["kind"] == "bandit_clear_site_evidence"
        )
        self.assertEqual(clear["site_id"], "overmap_special:bandit_camp@140,51,0")
        self.assertEqual(clear["next_near_tick_minutes"], 8640)

    def test_phase4_decoy_transform_clears_current_schema_outing_and_legacy_pressure(self) -> None:
        resolved = resolve_fixture_payload(
            "bandit_phase4_decoy_empty_signal_v0_2026-08-05",
            "live-debug",
        )
        signal = next(
            transform for transform in resolved["save_transforms"]
            if transform["kind"] == "bandit_camp_map_lead"
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            world_dir = Path(temp_dir) / "McWilliams"
            shutil.copytree(resolved["save_src"] / "McWilliams", world_dir)
            dimension_path = world_dir / "dimension_data.gsav"
            version_line, _, payload_text = dimension_path.read_text(encoding="utf-8").partition("\n")
            payload = json.loads(payload_text)
            site = payload["overmapbuffer"]["bandit_live_world"]["sites"][0]
            self.assertEqual(site["schema_version"], 12)
            site["active_outing"].update({
                "kind": "structural_sortie",
                "activity_id": "camp#scout:1",
                "camp_id": site["site_id"],
                "generation": 1,
                "member_ids": [4, 5],
                "leader_id": 4,
                "target_id": "old-target",
                "target_omt": [135, 51, 0],
                "job_type": "scout",
                "started_minutes": 100,
                "local_contact_minutes": 120,
                "last_progress_minutes": 120,
                "last_advanced_minutes": 120,
            })
            site["members"][0]["state"] = "outbound"
            site["members"][1]["state"] = "local_contact"
            for key, value in {
                "active_group_id": "legacy-group",
                "active_target_id": "legacy-target",
                "active_target_omt": [135, 51, 0],
                "active_job_type": "scout",
                "active_member_ids": [4, 5],
                "active_sortie_started_minutes": 100,
                "active_sortie_local_contact_minutes": 120,
            }.items():
                site[key] = value
            dimension_path.write_text(
                version_line + "\n" + json.dumps(payload, ensure_ascii=False, separators=(",", ":")),
                encoding="utf-8",
            )

            apply_bandit_camp_map_lead_transform(world_dir, signal)

            _, _, transformed_text = dimension_path.read_text(encoding="utf-8").partition("\n")
            transformed = json.loads(transformed_text)
            transformed_site = transformed["overmapbuffer"]["bandit_live_world"]["sites"][0]
            active_outing = transformed_site["active_outing"]
            self.assertEqual(active_outing["schema_version"], 5)
            self.assertEqual(active_outing["kind"], "none")
            self.assertEqual(active_outing["activity_id"], "")
            self.assertEqual(active_outing["generation"], 0)
            self.assertEqual(active_outing["member_ids"], [])
            self.assertEqual(active_outing["target_id"], "")
            self.assertEqual(active_outing["started_minutes"], -1)
            self.assertEqual(active_outing["local_contact_minutes"], -1)
            for legacy_key in (
                "active_group_id",
                "active_target_id",
                "active_target_omt",
                "active_job_type",
                "active_member_ids",
                "active_sortie_started_minutes",
                "active_sortie_local_contact_minutes",
            ):
                self.assertNotIn(legacy_key, transformed_site)
            self.assertEqual(
                [member["state"] for member in transformed_site["members"][:2]],
                ["at_home", "at_home"],
            )
            lead = next(
                lead for lead in transformed_site["intelligence_map"]["leads"]
                if lead["lead_id"] == signal["lead_id"]
            )
            self.assertEqual(lead["kind"], "smoke_signal")
            self.assertEqual(lead["target_id"], "structural-smoke@(137,49,0)")
            self.assertEqual(lead["omt"], [137, 49, 0])
            self.assertEqual(lead["bounty"], 0)

    def test_phase5_visible_burn_producer_fixture_is_uncloaked_target_footing(self) -> None:
        resolved = resolve_fixture_payload(
            "bandit_phase5_player_camp_burn_producer_v0_2026-08-06",
            "live-debug",
        )
        transforms = resolved["save_transforms"]
        kinds = [transform["kind"] for transform in transforms]
        lead = next(
            transform for transform in transforms
            if transform["kind"] == "bandit_camp_map_lead"
        )
        mutations = [
            mutation
            for transform in transforms if transform["kind"] == "player_mutations"
            for mutation in transform["mutations"]
        ]

        self.assertEqual(
            resolved["source_chain"][-2:],
            [
                ("live-debug", "bandit_phase5_player_camp_burn_producer_v0_2026-08-06"),
                ("live-debug", "bandit_phase4_quiet_current_schema_v0_2026-08-05"),
            ],
        )
        self.assertLess(kinds.index("bandit_clear_site_evidence"), kinds.index("bandit_camp_map_lead"))
        self.assertLess(kinds.index("bandit_camp_map_lead"), kinds.index("game_turn"))
        self.assertEqual(kinds[-1], "player_mutations")
        self.assertEqual(mutations, ["DEBUG_CLAIRVOYANCE"])
        self.assertNotIn("DEBUG_CLOAK", mutations)
        self.assertEqual(lead["kind_value"], "terrain_opportunity")
        self.assertEqual(lead["origin"], "structural_routine")
        self.assertEqual(lead["status"], "scout_confirmed")
        self.assertEqual(lead["target_id"], "player@140,39,0")
        self.assertEqual(lead["target_omt"], [140, 39, 0])
        self.assertEqual(lead["bounty"], 8)
        self.assertEqual(lead["threat"], 0)
        self.assertEqual(lead["next_routine_dispatch_eligible_minutes"], 8280)
        self.assertIn("no discovery, route, watch, dispatch, burn, or outcome", lead["source_summary"])
        clock = next(transform for transform in transforms if transform["kind"] == "game_turn")
        self.assertEqual(clock["turn"], 5255993)
        self.assertTrue(clock["shift_queued_eocs"])

    def test_phase5_visible_burn_producer_is_explicit_non_credit_observing_capture(self) -> None:
        scenario = load_scenario("bandit.phase5_visible_burn_producer_mcw")
        steps = list(scenario["steps"])
        labels = [step["label"] for step in steps]
        exact_lead = (
            "overmap_special:bandit_camp@140,51,0:"
            "terrain_opportunity:140,39,0:player_camp"
        )

        self.assertEqual(
            scenario["fixture"],
            "bandit_phase5_player_camp_burn_producer_v0_2026-08-06",
        )
        self.assertEqual(scenario["profile"], "dev-harness")
        self.assertEqual(scenario["world"], "McWilliams")
        self.assertTrue(scenario["replace_existing_worlds"])
        self.assertLess(
            labels.index("preflight_phase5_player_camp_producer"),
            labels.index("wait_three_hours_for_phase5_schema10_observer_pair"),
        )
        self.assertLess(
            labels.index("audit_exact_phase5_player_camp_dispatch"),
            labels.index("open_phase5_burn_producer_save_prompt"),
        )
        self.assertLess(
            labels.index("audit_phase5_burn_producer_mtime_after_save"),
            labels.index("audit_saved_phase5_schema10_observing_pair"),
        )
        waits = [step for step in steps if step["kind"] == "long_wait"]
        self.assertEqual([(step["choice_key"], step["expected_duration"]) for step in waits], [("7", "3h")])
        preflight = steps[labels.index("preflight_phase5_player_camp_producer")]
        postflight = steps[labels.index("audit_saved_phase5_schema10_observing_pair")]
        dispatch = steps[labels.index("audit_exact_phase5_player_camp_dispatch")]
        self.assertEqual(preflight["required_lead_kind"], "terrain_opportunity")
        self.assertEqual(preflight["required_lead_target_id"], "player@140,39,0")
        self.assertEqual(postflight["required_active_target_id_exact"], exact_lead)
        self.assertEqual(postflight["required_active_outing_kind"], "structural_sortie")
        self.assertEqual(postflight["required_active_outing_generation"], 1)
        self.assertEqual(postflight["required_active_outing_phase"], "observing")
        self.assertEqual(postflight["required_active_job_type"], "scout")
        self.assertTrue(
            any(
                exact_lead in pattern
                for group in dispatch["required_line_patterns"]
                for pattern in group
            )
        )
        self.assertIn("non-credit producer/calibration only", scenario["evidence_contract"]["claim"])
        self.assertIn("no Phase-5 burn or observer credit", scenario["evidence_contract"]["load_only_verdict"])
        self.assertIn("--compact-stdout", scenario["recommended_test_command"])

    def test_phase4_decoy_scenario_uses_real_empty_arrival_and_owner_audit(self) -> None:
        scenario = load_scenario("bandit.phase4_decoy_empty_signal_live_mcw")
        steps = list(scenario["steps"])
        labels = [step["label"] for step in steps]
        exact_target = "structural-smoke@(137,49,0)"
        exact_source = "structural-signal:structural-smoke@(137,49,0)"

        self.assertEqual(
            scenario["fixture"],
            "bandit_phase4_decoy_empty_signal_v0_2026-08-05",
        )
        preflight = steps[labels.index("preflight_fresh_returned_smoke_decoy")]
        postflight = steps[labels.index("audit_decoy_empty_evidence_debug")]
        self.assertEqual(preflight["required_lead_target_id"], exact_target)
        self.assertEqual(preflight["required_lead_source_contains"], exact_source)
        self.assertEqual(preflight["required_lead_status"], "suspected")
        self.assertEqual(postflight["kind"], "audit_log_contains")
        self.assertEqual(
            postflight["required_line_patterns"],
            [
                [
                    "site id=overmap_special:bandit_camp@140,51,0",
                    "anchor=(140,51,0)",
                    "leads_total=1",
                ],
                [
                    "lead id=overmap_special:bandit_camp@140,51,0#lead:smoke_signal:structural-smoke@(137,49,0)@137,49,0",
                    "last_known_omt=(137,49,0)",
                    "origin=returned_report",
                    "source_key=structural-signal:structural-smoke@(137,49,0)",
                    "status=stale",
                    "confidence=0",
                    "times_checked_empty=1",
                    "last_outcome=signal_investigation_empty",
                ],
            ],
        )

        empty_note = "returned signal investigation was empty"
        empty_wait = steps[labels.index("wait_1_hour_for_decoy_empty_arrival")]
        self.assertEqual(
            empty_wait["artifact_state_patterns"],
            ["scheduler_hour=141", "arrivals=1", empty_note],
        )
        empty_audit = steps[labels.index("audit_decoy_empty_transition")]
        self.assertEqual(empty_audit["required_line_patterns"], [[empty_note]])
        player_negative = steps[labels.index("audit_decoy_lifecycle_has_no_player_token")]
        self.assertEqual(
            player_negative["forbidden_line_patterns"][1],
            [empty_note, "player@"],
        )
        self.assertNotIn("signal investigation empty", json.dumps(scenario))

        watch_preflight = steps[labels.index("audit_decoy_watch_geography_selected")]
        self.assertEqual(
            watch_preflight["required_line_patterns"],
            [[
                "bandit_live_world watch_geography_preflight",
                "site=overmap_special:bandit_camp@140,51,0",
                "target=(137,49,0)",
                "qualified=",
                "route_reachable=",
                "selected_omt=",
                "outcome=selected",
            ]],
        )
        self.assertTrue(watch_preflight["abort_on_metadata_failure"])
        self.assertLess(
            labels.index("wait_1_hour_for_decoy_dispatch"),
            labels.index("audit_decoy_watch_geography_selected"),
        )
        self.assertLess(
            labels.index("audit_decoy_watch_geography_selected"),
            labels.index("audit_exact_decoy_dispatch"),
        )

        waits = [step for step in steps if step["kind"] == "long_wait"]
        self.assertEqual(len(waits), 3)
        self.assertTrue(all(step["expected_duration"] == "1h" for step in waits))
        self.assertTrue(all(step["auto_acknowledge_interruptions"] is False for step in waits))
        self.assertLess(
            labels.index("audit_exact_decoy_dispatch"),
            labels.index("audit_decoy_empty_transition"),
        )
        self.assertLess(
            labels.index("audit_decoy_empty_transition"),
            labels.index("audit_decoy_empty_evidence_debug"),
        )
        press_keys = [
            key
            for step in steps if step["kind"] == "press"
            for key in step["keys"]
        ]
        self.assertNotIn("I", press_keys)
        self.assertNotIn("P", press_keys)
        self.assertEqual(press_keys, [])
        self.assertIn(
            "bounded read-only evidence debug lead row",
            scenario["evidence_contract"]["observer_artifact_requirement"],
        )

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

    def test_game_turn_time_warp_shifts_player_and_global_queued_eocs(self) -> None:
        payload = {
            "turn": 5241593,
            "queued_global_effect_on_conditions": [
                {
                    "time": 6736754,
                    "eoc": "EOC_PORTAL_STORM_WARN_OR_CAUSE_RECURRING",
                    "context": {"alpha": "preserved"},
                },
                {
                    "time": 5241593,
                    "eoc": "EOC_IMMEDIATELY_DUE",
                    "context": {"beta": "preserved"},
                },
            ],
            "player": {
                "queued_effect_on_conditions": [
                    {
                        "time": 5241600,
                        "eoc": "EOC_PLAYER_EVENT",
                        "context": {"gamma": "preserved"},
                    },
                ],
            },
        }

        report = apply_game_turn_to_payload(
            payload,
            new_turn=68256000,
            shift_queued_eocs=True,
        )

        self.assertEqual(report["turn_delta"], 63014407)
        self.assertEqual(payload["turn"], 68256000)
        global_queue = payload["queued_global_effect_on_conditions"]
        player_queue = payload["player"]["queued_effect_on_conditions"]
        self.assertEqual([entry["time"] for entry in global_queue], [69751161, 68256000])
        self.assertEqual(player_queue[0]["time"], 68256007)
        self.assertEqual(global_queue[0]["context"], {"alpha": "preserved"})
        self.assertEqual(global_queue[1]["context"], {"beta": "preserved"})
        self.assertEqual(player_queue[0]["context"], {"gamma": "preserved"})
        self.assertEqual(report["queue_reports"]["global"]["count"], 2)
        self.assertEqual(report["queue_reports"]["player"]["count"], 1)

    def test_game_turn_without_queue_shift_preserves_existing_transform_behavior(self) -> None:
        payload = {
            "turn": 100,
            "queued_global_effect_on_conditions": [{"time": 150, "eoc": "EOC_GLOBAL"}],
            "player": {"queued_effect_on_conditions": [{"time": 175, "eoc": "EOC_PLAYER"}]},
        }

        report = apply_game_turn_to_payload(payload, new_turn=200, shift_queued_eocs=False)

        self.assertEqual(payload["turn"], 200)
        self.assertEqual(payload["queued_global_effect_on_conditions"][0]["time"], 150)
        self.assertEqual(payload["player"]["queued_effect_on_conditions"][0]["time"], 175)
        self.assertEqual(report["queue_reports"], {})

    def test_game_turn_queue_shift_fails_closed_for_malformed_queues(self) -> None:
        invalid_payloads = [
            ({
                "turn": 100,
                "queued_global_effect_on_conditions": {},
                "player": {"queued_effect_on_conditions": []},
            }, "is not a list"),
            ({
                "turn": 100,
                "queued_global_effect_on_conditions": ["bad entry"],
                "player": {"queued_effect_on_conditions": []},
            }, "is not an object"),
            ({
                "turn": 100,
                "queued_global_effect_on_conditions": [{"time": "later", "eoc": "EOC_GLOBAL"}],
                "player": {"queued_effect_on_conditions": []},
            }, "non-integer time"),
        ]
        for payload, expected_error in invalid_payloads:
            with self.subTest(payload=payload):
                with self.assertRaisesRegex(SystemExit, expected_error):
                    apply_game_turn_to_payload(payload, new_turn=200, shift_queued_eocs=True)

        invalid_player_queue = {
            "turn": 100,
            "queued_global_effect_on_conditions": [{"time": 150, "eoc": "EOC_GLOBAL"}],
            "player": {"queued_effect_on_conditions": ["bad entry"]},
        }
        with self.assertRaisesRegex(SystemExit, "is not an object"):
            apply_game_turn_to_payload(invalid_player_queue, new_turn=200, shift_queued_eocs=True)
        self.assertEqual(invalid_player_queue["queued_global_effect_on_conditions"][0]["time"], 150)

    def test_rider_release_fixtures_shift_queued_eocs_with_time_jump(self) -> None:
        manifest_path = Path("fixture") / "manifest.json"
        normalized = normalize_fixture_save_transforms(
            [{
                "kind": "game_turn",
                "player_save": "player.sav.zzip",
                "turn": 68256000,
                "shift_queued_eocs": True,
            }],
            manifest_path=manifest_path,
        )
        self.assertTrue(normalized[0]["shift_queued_eocs"])
        with self.assertRaisesRegex(SystemExit, "must be boolean"):
            normalize_fixture_save_transforms(
                [{
                    "kind": "game_turn",
                    "player_save": "player.sav.zzip",
                    "turn": 68256000,
                    "shift_queued_eocs": "true",
                }],
                manifest_path=manifest_path,
            )

        fixture_names = [
            "mcwilliams_live_debug_zombie_rider_camp_light_2026-05-01",
            "mcwilliams_live_debug_zombie_rider_no_camp_light_2026-05-01",
        ]
        for fixture_name in fixture_names:
            with self.subTest(fixture=fixture_name):
                transforms = resolve_fixture_payload(fixture_name, "live-debug")["save_transforms"]
                game_turn = next(transform for transform in transforms if transform["kind"] == "game_turn")
                self.assertEqual(game_turn["turn"], 68256000)
                self.assertTrue(game_turn["shift_queued_eocs"])

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

    def test_no_camp_light_control_requires_same_run_rider_liveness(self) -> None:
        scenario = load_scenario("zombie_rider.live_no_camp_light_control_mcw")
        steps = {
            str(step.get("label", "")).strip(): step
            for step in scenario.get("steps", [])
        }
        liveness = steps["audit_zombie_rider_no_camp_light_rider_liveness"]

        self.assertEqual(liveness["kind"], "audit_log_contains")
        self.assertEqual(
            liveness["since_label"],
            "advance_rider_no_camp_light_control_window",
        )
        self.assertTrue(liveness["abort_on_metadata_failure"])
        self.assertEqual(
            liveness["required_any_line_patterns"],
            [
                ["zombie_rider target_probe:", "eval_us="],
                ["zombie_rider live_plan:", "camp_posture=none", "eval_us="],
            ],
        )

    def test_no_camp_light_control_keeps_separate_deferred_negative_audit(self) -> None:
        scenario = load_scenario("zombie_rider.live_no_camp_light_control_mcw")
        steps = list(scenario.get("steps", []))
        labels = [str(step.get("label", "")).strip() for step in steps]
        liveness_label = "audit_zombie_rider_no_camp_light_rider_liveness"
        negative_label = "audit_zombie_rider_no_camp_light_no_band_trace"
        negative = steps[labels.index(negative_label)]

        self.assertLess(labels.index(liveness_label), labels.index(negative_label))
        self.assertEqual(negative["kind"], "audit_log_not_contains")
        self.assertEqual(
            negative["since_label"],
            "advance_rider_no_camp_light_control_window",
        )
        self.assertTrue(negative["abort_on_metadata_failure"])
        self.assertTrue(
            all(
                step.get("proof_deferred_to_label") == negative_label
                for step in steps
                if step.get("capture_after")
            )
        )

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
