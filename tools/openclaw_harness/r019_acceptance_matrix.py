#!/usr/bin/env python3
"""Fail-closed cross-report acceptance relation for R-019 Keep watch.

The relation deliberately consumes immutable report-local packets.  It does
not infer a primitive reference from a guarded run, and it never changes the
evidence class carried by either report.
"""

from __future__ import annotations

from typing import Any, Mapping, Sequence


_REQUIRED_STOPS = frozenset((
    "safe_flavour", "safe_prompt", "target_crossed", "meaningful_or_unknown_event",
    "unsafe_condition", "unknown_safety_frame", "stale_observation", "binding_drift",
    "no_progress", "derived_bound_exhausted",
))
_REQUIRED_OFF_SWITCHES = frozenset(("master_enabled", "enabled"))


def _valid_count(receipt: Mapping[str, Any], name: str) -> bool:
    value = receipt.get(name)
    return isinstance(value, Mapping) and isinstance(value.get("count"), int) and \
           not isinstance(value.get("count"), bool) and value["count"] >= 0 and \
           isinstance(value.get("measurement"), str) and bool(value["measurement"].strip())


def validate_r019_report_packet(packet: Mapping[str, Any]) -> list[str]:
    """Reject a mutable R-019 packet before it can become immutable history."""
    role = str(packet.get("role", "")).strip()
    if role not in {"guarded", "primitive"}:
        return ["invalid_r019_role"]
    errors: list[str] = []
    role_receipt = packet.get("role_receipt")
    if not isinstance(role_receipt, Mapping) or role_receipt.get("schema") != \
            "caol-r019-role-receipt-v1" or role_receipt.get("role") != role or \
            not str(role_receipt.get("run_id", "")).strip() or \
            not str(role_receipt.get("binding_id", "")).strip():
        errors.append("invalid_r019_role_receipt")
    receipts = packet.get("round_trip_receipt")
    if not isinstance(receipts, Mapping) or receipts.get("schema") != \
            "caol-r019-round-trip-receipt-v1" or not _valid_count(receipts, "model") or \
            not _valid_count(receipts, "tool"):
        errors.append("invalid_r019_round_trip_receipt")
    return errors


def validate_r019_acceptance_matrix(reports: Sequence[Mapping[str, Any]]) -> dict[str, Any]:
    """Validate the finite R-019 relation over distinct immutable reports."""
    packets: dict[str, Mapping[str, Any]] = {}
    errors: list[str] = []
    for report in reports:
        report_id = str(report.get("report_id", "")).strip()
        packet = report.get("r019_acceptance_matrix")
        if not report_id or not isinstance(packet, Mapping):
            continue
        role = str(packet.get("role", "")).strip()
        if role in packets:
            errors.append(f"duplicate_role:{role}")
        else:
            packets[role] = {**packet, "report_id": report_id,
                              "evidence_class": report.get("evidence_class")}

    guarded = packets.get("guarded")
    primitive = packets.get("primitive")
    if guarded is None:
        errors.append("missing_guarded_report")
    if primitive is None:
        errors.append("missing_primitive_report")
    if guarded is None or primitive is None:
        return {"status": "red", "errors": errors, "inputs": packets}
    if guarded["report_id"] == primitive["report_id"]:
        errors.append("comparison_requires_distinct_immutable_reports")
    if not guarded.get("registry_authority_id") or not primitive.get("registry_authority_id") or \
            guarded.get("registry_authority_id") == primitive.get("registry_authority_id"):
        errors.append("comparison_requires_distinct_authority_ids")
    if not guarded.get("registry_executable_binding") or \
            guarded.get("registry_executable_binding") != primitive.get("registry_executable_binding"):
        errors.append("mismatched_registry_executable_binding")
    for key in ("clean_start_identity", "source_identity"):
        if not guarded.get(key) or guarded.get(key) != primitive.get(key):
            errors.append(f"mismatched_{key}")
    for key in ("native_transitions", "terminal_state"):
        if guarded.get(key) != primitive.get(key):
            errors.append(f"mismatched_{key}")
    for role, packet in (("guarded", guarded), ("primitive", primitive)):
        packet_errors = validate_r019_report_packet(packet)
        if "invalid_r019_role_receipt" in packet_errors:
            errors.append(f"invalid_role_receipt:{role}")
        if "invalid_r019_round_trip_receipt" in packet_errors:
            errors.append(f"missing_measured_round_trips:{role}")
    # Stop and off-switch reports are independently authorized follow-on
    # controls.  They must be valid if supplied, but their future absence may
    # not prevent the zero-credit paired aggregation terminal from preserving
    # its two exact report IDs.
    for stop in _REQUIRED_STOPS:
        packet = packets.get("stop:" + stop)
        if packet is None:
            continue
        if packet.get("report_id") in {guarded["report_id"], primitive["report_id"]} or \
                packet.get("clean_start_identity") != guarded.get("clean_start_identity") or \
                packet.get("source_identity") != guarded.get("source_identity"):
            errors.append("unbound_stop_report:" + stop)
        receipt = packet.get("stop_receipt")
        if not isinstance(receipt, Mapping) or receipt.get("stop_reason") != stop or \
                receipt.get("native_dispatch_after_stop") is not False:
            errors.append("invalid_stop_receipt:" + stop)
    for switch in _REQUIRED_OFF_SWITCHES:
        packet = packets.get("off:" + switch)
        if packet is None:
            continue
        if packet.get("report_id") in {guarded["report_id"], primitive["report_id"]} or \
                packet.get("clean_start_identity") != guarded.get("clean_start_identity") or \
                packet.get("source_identity") != guarded.get("source_identity"):
            errors.append("unbound_off_switch_report:" + switch)
        receipt = packet.get("off_switch_receipt")
        if not isinstance(receipt, Mapping) or receipt.get("schema") != \
                "caol-r019-off-switch-receipt-v1" or receipt.get("switch") != switch or \
                receipt.get("native_dispatch_count") != 0 or \
                receipt.get("guarded_recipe_dispatch_count") != 0 or \
                receipt.get("guarded_handling_count") != 0 or \
                receipt.get("hidden_batching") is not False or \
                not isinstance(receipt.get("primitive_native_dispatch_count"), int) or \
                receipt["primitive_native_dispatch_count"] <= 0 or \
                not isinstance(receipt.get("native_receipt_actions"), list) or \
                not receipt["native_receipt_actions"]:
            errors.append("invalid_off_switch_receipt:" + switch)
    return {
        "status": "green" if not errors else "red",
        "errors": errors,
        "inputs": packets,
        "preserved_evidence_classes": {
            role: packet.get("evidence_class") for role, packet in packets.items()
        },
    }


__all__ = ["validate_r019_acceptance_matrix", "validate_r019_report_packet"]
