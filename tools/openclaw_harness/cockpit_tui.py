#!/usr/bin/env python3
"""Deterministic, non-ANSI projection and controller for the cockpit service.

This module deliberately owns no game, registry, binding, or receipt state.  It
turns public ``CockpitService`` responses into a compact stable view, and sends
one of the service's advertised primitive actions back unchanged.  A terminal
UI, a JSON client, and a test can therefore use the same command schema.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, Mapping, Protocol


class CockpitCaller(Protocol):
    def call(self, request: Mapping[str, Any]) -> Dict[str, Any]:
        """Make one public cockpit request."""


_ALIASES = {
    "KEEP WATCH": "game.keep_watch",
    "MAKE CAMP": "scenario.prepare",
    "STOCK UP": "scenario.prepare",
    "ZAP": "scenario.prepare",
    "MOVE OUT": "game.guarded_move_relative",
    "EYES UP": "game.observe",
    "BIG MAP": "game.observe",
}


_CONTRACTS = {
    "game.act": {
        "id": "contract.game.act.v1",
        "required": ["action", "observation_id", "action_id"],
        "properties": {
            "action": {"const": "game.act"},
            "observation_id": {"type": "string", "source": "field.frame_id"},
            "action_id": {"type": "string", "source": "advertised_actions"},
        },
    },
    "game.keep_watch": {
        "id": "contract.game.keep_watch.v1",
        "guarded": True,
        "required": ["action", "keep_watch"],
        "properties": {
            "action": {"const": "game.keep_watch"},
            "keep_watch": {"type": "object", "required": ["enabled", "target_game_minutes", "bound", "recipe"],
                           "properties": {
                               "master_enabled": {"type": "boolean", "default": True},
                               "enabled": {"const": True},
                               "target_game_minutes": {"type": "number"},
                               "bound": {"type": "object", "required": ["maximum"]},
                               "recipe": {"type": "array", "items": {"type": "string"}, "minItems": 1},
                           }},
        },
    },
    "game.guarded_move_relative": {
        "id": "contract.game.guarded_move_relative.v1",
        "guarded": True,
        "required": ["action", "guarded_move_relative"],
        "properties": {
            "action": {"const": "game.guarded_move_relative"},
            "guarded_move_relative": {"type": "object", "required": ["enabled", "offset_ms", "bound"],
                                        "properties": {
                                            "master_enabled": {"type": "boolean", "default": True},
                                            "enabled": {"const": True},
                                            "offset_ms": {"type": "array", "items": {"type": "integer"},
                                                          "length": 2, "nonzero": True},
                                            "bound": {"type": "object", "required": ["maximum", "basis", "source", "unit"],
                                                      "properties": {"unit": {"const": "steps"}}},
                                        }},
        },
    },
    "scenario.prepare": {
        "id": "contract.scenario.prepare.v1",
        "required": ["action", "id", "required_typeid", "candidate_offsets"],
        "properties": {
            "action": {"const": "scenario.prepare"},
            "id": {"type": "string", "nonempty": True},
            "required_typeid": {"type": "string", "nonempty": True},
            "candidate_offsets": {"type": "array"},
            "world": {"type": "string", "optional": True},
            "player_save": {"type": "string", "optional": True},
        },
    },
    "game.observe": {
        "id": "contract.game.observe.v1",
        "required": ["action"],
        "properties": {"action": {"const": "game.observe"}},
    },
}


def _contract(action: str) -> Dict[str, Any]:
    """Return a detached public contract, never a guessed executable request."""
    contract = _CONTRACTS.get(action)
    return dict(contract) if contract is not None else {
        "id": "contract.unavailable.v1", "state": "unavailable", "action": action,
    }


def _request_view(identifier: str, action: str, request: Mapping[str, Any] | None = None) -> Dict[str, Any]:
    """Expose an exact request when known, otherwise name the missing arguments."""
    contract = _contract(action)
    if request is not None:
        return {"id": identifier, "state": "ready", "request": dict(request), "contract": contract}
    return {
        "id": identifier,
        "state": "arguments_required",
        "request": {"action": action},
        "missing": [key for key in contract.get("required", []) if key != "action"],
        "contract": contract,
    }


def _int(value: Any) -> int | None:
    return value if isinstance(value, int) and not isinstance(value, bool) else None


def _cell(cell: Mapping[str, Any]) -> Dict[str, Any] | None:
    dx, dy = _int(cell.get("dx")), _int(cell.get("dy"))
    if dx is None or dy is None:
        return None
    visibility = str(cell.get("visibility", "unknown"))
    terrain = str(cell.get("terrain", "unknown")) if visibility == "clear" else "unknown"
    return {"id": f"local.cell.{dx}.{dy}", "dx": dx, "dy": dy,
            "visibility": visibility, "terrain": terrain}


def _local_map(observation: Mapping[str, Any]) -> Dict[str, Any]:
    minimap = observation.get("minimap")
    source = minimap if isinstance(minimap, Mapping) else {"cells": observation.get("visible_local", [])}
    declared_radius = _int(source.get("radius"))
    cells = []
    for raw in source.get("cells", []):
        if not isinstance(raw, Mapping):
            continue
        normalized = _cell(raw)
        if normalized is None:
            continue
        # A producer-declared radius is the measured visible-world boundary.
        # Do not render out-of-contract cells supplied beside that frame.
        if declared_radius is not None and (abs(normalized["dx"]) > declared_radius or
                                            abs(normalized["dy"]) > declared_radius):
            continue
        cells.append(normalized)
    return {
        "id": "field.local_map",
        "coordinate_system": "avatar_relative_tiles",
        "bound": {"radius": declared_radius, "source": "native_minimap.radius"} if declared_radius is not None else
                 {"radius": 1, "source": "native_visible_local"},
        "cells": sorted(cells, key=lambda cell: (cell["dy"], cell["dx"])),
    }


def _overmap_cell(cell: Mapping[str, Any], radius: int | None) -> Dict[str, Any] | None:
    dx, dy = _int(cell.get("dx")), _int(cell.get("dy"))
    if dx is None or dy is None:
        return None
    if radius is not None and (abs(dx) > radius or abs(dy) > radius):
        return None
    state = str(cell.get("state", "unknown"))
    if state not in {"clear", "unknown", "stale", "unavailable"}:
        state = "unavailable"
    terrain = str(cell.get("terrain", "unknown")) if state == "clear" else "unknown"
    provenance = str(cell.get("provenance", "none"))
    recency = cell.get("recency") if isinstance(cell.get("recency"), Mapping) else {"state": "unavailable"}
    return {
        "id": f"overmap.cell.{dx}.{dy}", "dx": dx, "dy": dy, "state": state,
        "vision": str(cell.get("vision", "unknown")), "terrain": terrain,
        "provenance": provenance, "recency": dict(recency),
    }


def _overmap(observation: Mapping[str, Any]) -> Dict[str, Any]:
    source = observation.get("overmap")
    if not isinstance(source, Mapping):
        return {"id": "field.overmap", "state": "unavailable", "provenance": "none",
                "recency": {"state": "unavailable"}, "cells": []}
    radius = _int(source.get("radius"))
    cells = []
    for raw in source.get("cells", []):
        if isinstance(raw, Mapping):
            normalized = _overmap_cell(raw, radius)
            if normalized is not None:
                cells.append(normalized)
    return {
        "id": "field.overmap", "state": str(source.get("state", "available")),
        "coordinate_system": str(source.get("coordinate_system", "avatar_relative_omt")),
        "bound": {"radius": radius, "source": str(source.get("bound_source", "native_overmap.radius"))},
        "center_absolute_omt": list(source.get("center_absolute_omt", [])),
        "provenance": str(source.get("provenance", "native_avatar_overmap")),
        "recency": dict(source.get("recency", {"state": "current_frame"})) if isinstance(
            source.get("recency", {"state": "current_frame"}), Mapping) else {"state": "unavailable"},
        "cells": sorted(cells, key=lambda cell: (cell["dy"], cell["dx"])),
    }


def _commands(observation: Mapping[str, Any]) -> list[Dict[str, Any]]:
    advertised = sorted({str(action) for action in observation.get("advertised_actions", [])
                         if isinstance(action, str) and action})
    observation_id = str(observation.get("observation_id", ""))
    commands = [{"id": f"command.{action}", "kind": "primitive", "action": "game.act",
                 "action_id": action, "legal": True, "key": str(index + 1),
                 "request_view": _request_view(
                     f"request.command.{action}", "game.act",
                     {"action": "game.act", "observation_id": observation_id, "action_id": action},
                 )}
                for index, action in enumerate(advertised)]
    for label, action in sorted(_ALIASES.items()):
        commands.append({"id": f"alias.{label.lower().replace(' ', '_')}", "kind": "alias",
                         "label": label, "equivalent": action, "legal": False,
                         "reason": "requires_structured_arguments_or_advertised_recipe",
                         "request_view": _request_view(
                             f"request.alias.{label.lower().replace(' ', '_')}", action,
                             {"action": action} if action == "game.observe" else None,
                         )})
    return commands


def render_state(observation: Mapping[str, Any], status: Mapping[str, Any], last_result: Mapping[str, Any] | None = None) -> Dict[str, Any]:
    """Project only public cockpit facts into stable field and command IDs."""
    compact_log = observation.get("compact_log") if isinstance(observation.get("compact_log"), Mapping) else {}
    safety = "unsafe" if compact_log.get("unsafe") is True else "clear"
    final = status.get("final") if isinstance(status.get("final"), Mapping) else {}
    stop_reason = str(final.get("stop_reason", ""))
    terminal = str(status.get("state", "active")) != "active" or bool(stop_reason)
    receipt = compact_log.get("latest_receipt")
    return {
        "schema": "caol-cockpit-tui-v1",
        "fields": [
            {"id": "field.binding_id", "value": str(status.get("binding_id", ""))},
            {"id": "field.frame_id", "value": str(observation.get("observation_id", ""))},
            {"id": "field.run_id", "value": str(observation.get("run_id", status.get("run_id", "")) )},
            {"id": "field.safety", "value": safety},
            {"id": "field.terminal", "value": terminal},
            {"id": "field.stop_reason", "value": stop_reason},
            {"id": "field.progress", "value": observation.get("delta", {"kind": "full"})},
            {"id": "field.receipt", "value": receipt},
            {"id": "field.mission", "value": observation.get("continuation", {})},
            {"id": "field.target", "value": observation.get("expected_postcondition", "")},
        ],
        "local_map": _local_map(observation),
        "overmap": _overmap(observation),
        "commands": _commands(observation),
        "last_result": dict(last_result or {}),
    }


@dataclass
class CockpitTui:
    """Thin controller: refreshes state and invokes public primitive actions."""

    service: CockpitCaller
    state: Dict[str, Any] | None = None
    observation: Dict[str, Any] | None = None
    last_result: Dict[str, Any] | None = None

    def refresh(self) -> Dict[str, Any]:
        observed = self.service.call({"action": "game.observe"})
        status = self.service.call({"action": "run.status"})
        if observed.get("ok") is not True:
            self.last_result = dict(observed)
            self.state = {"schema": "caol-cockpit-tui-v1", "error": str(observed.get("error", "unknown")),
                          "fields": [], "local_map": {"id": "field.local_map", "cells": []},
                          "overmap": {"id": "field.overmap", "state": "unavailable", "cells": []},
                          "commands": []}
            return self.state
        self.observation = dict(observed["result"])
        status_result = status.get("result", {}) if status.get("ok") is True else {}
        self.state = render_state(self.observation, status_result, self.last_result)
        return self.state

    def dispatch(self, command_id: str) -> Dict[str, Any]:
        if self.state is None or self.observation is None:
            self.refresh()
        for command in self.state.get("commands", []) if self.state else []:
            if command.get("id") != command_id:
                continue
            if command.get("legal") is not True:
                self.last_result = {"ok": False, "error": "command_is_alias_only", "command_id": command_id,
                                    "equivalent": command.get("equivalent")}
                return self.last_result
            request_view = command.get("request_view")
            request = request_view.get("request") if isinstance(request_view, Mapping) else None
            if not isinstance(request, Mapping):
                self.last_result = {"ok": False, "error": "command_request_unavailable", "command_id": command_id}
                return self.last_result
            self.last_result = self.service.call(dict(request))
            next_observation = self.last_result.get("observation")
            if isinstance(next_observation, Mapping):
                self.observation = dict(next_observation)
                status = self.service.call({"action": "run.status"})
                self.state = render_state(self.observation, status.get("result", {}), self.last_result)
            return self.last_result
        self.last_result = {"ok": False, "error": "unknown_command", "command_id": command_id}
        return self.last_result

    def dispatch_key(self, key: str) -> Dict[str, Any]:
        for command in self.state.get("commands", []) if self.state else []:
            if command.get("key") == key:
                return self.dispatch(str(command["id"]))
        return {"ok": False, "error": "unknown_key", "key": key}
