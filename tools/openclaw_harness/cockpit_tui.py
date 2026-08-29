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


def _commands(observation: Mapping[str, Any]) -> list[Dict[str, Any]]:
    advertised = sorted({str(action) for action in observation.get("advertised_actions", [])
                         if isinstance(action, str) and action})
    commands = [{"id": f"command.{action}", "kind": "primitive", "action": "game.act",
                 "action_id": action, "legal": True, "key": str(index + 1)}
                for index, action in enumerate(advertised)]
    for label, action in sorted(_ALIASES.items()):
        commands.append({"id": f"alias.{label.lower().replace(' ', '_')}", "kind": "alias",
                         "label": label, "equivalent": action, "legal": False,
                         "reason": "requires_structured_arguments_or_advertised_recipe"})
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
        # No overmap is currently supplied by the authoritative public frame.
        # An explicit unavailable field is more honest than reconstructing one.
        "overmap": {"id": "field.overmap", "state": "unavailable", "provenance": "none",
                    "recency": "unavailable", "cells": []},
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
            self.last_result = self.service.call({"action": "game.act",
                                                  "observation_id": self.observation["observation_id"],
                                                  "action_id": command["action_id"]})
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
