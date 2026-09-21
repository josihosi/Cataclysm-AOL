"""Session-bound waiting commands and a plain-text playtest conversation."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import sys

from play_cli import PlayerClient, session_lock
from cockpit_evidence import decode


def elapsed(minutes):
    seconds = round(minutes * 60)
    hours, seconds = divmod(seconds, 3600)
    minutes, seconds = divmod(seconds, 60)
    return " ".join(f"{value} {unit}{'' if value == 1 else 's'}"
                    for value, unit in ((hours, "hour"), (minutes, "minute"), (seconds, "second"))
                    if value) or "0 seconds"


def performance_text(assessment):
    lines = []
    for alarm in assessment.get("alarms", []):
        if alarm.get("kind") == "waiting_slow":
            lines.extend([
                "Performance alarm: waiting is too slow.",
                f"Average turn: {alarm['mean_seconds'] * 1000:g} ms over {alarm['sample_count']} turns. "
                f"Limit: {alarm['limit_seconds'] * 1000:g} ms.",
                "Tell the coordinator: waiting performance needs attention.",
            ])
        else:
            lines.append("Performance alarm: " + str(alarm.get("kind", "unknown")).replace("_", " ") + ".")
    for recovery in assessment.get("recoveries", []):
        lines.append("Waiting performance recovered." if recovery.get("kind") == "waiting_recovery"
                     else "Performance alarm cleared: " + str(recovery.get("recovered_alarm", "unknown")) + ".")
    if assessment.get("status") == "unavailable":
        lines.append("Performance monitoring unavailable: " + str(assessment.get("reason", "no measurement")) + ".")
    return lines


class WaitingPlayer:
    def __init__(self, client):
        self.client = client
        self.state = client.state.setdefault("plain_waiting", {})

    def current(self):
        return self.client.state.get("plain_current", {})

    def actions(self):
        return [a for a in self.current().get("surface", {}).get("actions", []) if a.get("enabled", True)]

    def elapsed_minutes(self, current):
        start, now = self.state.get("start_turn"), current.get("game_turn")
        if isinstance(start, (int, float)) and isinstance(now, (int, float)):
            return (now - start) / 60
        start, now = self.state.get("start_minutes"), current.get("game_minutes")
        return now - start if isinstance(start, (int, float)) and isinstance(now, (int, float)) else None

    def act(self, action, target=None):
        offered = [a for a in self.actions() if a.get("id") == action and
                   (target is None or a.get("stable_id") == target)]
        if len(offered) != 1:
            raise ValueError("That action is not currently available. Run play look.")
        return self.client.act(action, target, {}, 1)

    def answer(self, answer):
        matches = [a for a in self.actions() if a.get("id") == "prompt.choose" and
                   str(a.get("label", "")).strip().casefold() == answer]
        if len(matches) != 1:
            raise ValueError(f"The current prompt does not offer {answer.upper()}. Run play look.")
        self.state["answer"] = answer
        return self.act("prompt.choose", matches[0].get("stable_id"))

    def advance_wait(self, result):
        """Walk the native duration menu; a pending request is never resubmitted."""
        while self.state.get("selecting") and result.get("ok") and result.get("state") != "pending":
            action_ids = {a["id"] for a in self.actions()}
            duration = "wait." + self.state["duration"]
            if duration in action_ids:
                self.state["selecting"] = False
                self.state["waiting"] = True
                self.client.save()
                return self.act(duration)
            if "wait.duration_menu" in action_ids:
                result = self.act("wait.duration_menu")
            elif any(a.get("id") == "menu.choose" and
                     a.get("stable_id") == "wait-mode:wait-a-while" for a in self.actions()):
                result = self.act("menu.choose", "wait-mode:wait-a-while")
            else:
                self.state["selecting"] = False
                offered = sorted(a.removeprefix("wait.") for a in action_ids
                                 if re.fullmatch(r"wait\.[1-9][0-9]*[mh]", a))
                choices = "\n".join("Choose " + value + " → play wait " + value for value in offered)
                raise ValueError("The game does not offer that waiting duration." +
                                 ("\n" + choices if choices else " Run play look."))
        if not result.get("ok"):
            self.state["selecting"] = False
        return result

    def run(self, command, duration=None):
        if self.client.state.get("pending"):
            if command != "look":
                raise ValueError("A command is still pending. Run play look to collect its result.")
            return self.advance_wait(self.client.collect(1))
        if command == "look":
            return self.client.submit({"action": "game.observe"}, 1)
        if command in {"yes", "no"}:
            return self.answer(command)
        if command == "stop":
            return self.act("activity.pause")
        if command == "quit":
            return self.client.submit({"action": "run.quit", "stop_reason": "player requested quit"}, 1)
        if not duration or not re.fullmatch(r"[1-9][0-9]*[mh]", duration):
            raise ValueError("Choose a duration, for example: play wait 5m or play wait 1h.")
        current = self.current()
        in_duration_menu = (current.get("surface", {}).get("kind") == "menu" and
                            any(a["id"].startswith("wait.") or
                                a.get("stable_id") == "wait-mode:wait-a-while" for a in self.actions()))
        if current.get("surface", {}).get("kind") != "world" and not in_duration_menu:
            raise ValueError("Waiting can start from ordinary gameplay. Run play look.")
        minutes = int(duration[:-1]) * (60 if duration[-1] == "h" else 1)
        self.state.update(duration=duration, requested_minutes=minutes,
                          start_minutes=current.get("game_minutes"), start_turn=current.get("game_turn"),
                          selecting=True, answer=None)
        self.client.save()
        return self.advance_wait({"ok": True} if in_duration_menu else self.act("world.wait"))

    def render(self, result):
        lines = []
        if result.get("state") == "pending":
            lines = ["Command pending. Check result → play look"]
        elif not result.get("ok"):
            reason = result.get("error", result.get("response", {}).get("error",
                                result.get("reason", "The game did not confirm the command.")))
            lines = ["Command failed: " + str(reason).replace("_", " "), "Check the game → play look"]
        elif self.client.state.get("finished"):
            lines = ["Playtest ended."]
        else:
            current = self.current()
            surface = current.get("surface", {})
            kind = surface.get("kind")
            facts = surface.get("facts", {})
            messages = decode(facts.get("messages", []))
            if isinstance(messages, list):
                previous = self.state.get("messages")
                if previous is not None:
                    overlap = min(len(previous), len(messages))
                    while overlap and previous[-overlap:] != messages[:overlap]:
                        overlap -= 1
                    for message in messages[overlap:]:
                        if isinstance(message, dict) and message.get("text"):
                            lines.append((str(message["time"]) + ": " if message.get("time") else "") + message["text"])
                self.state["messages"] = messages
            text = facts.get("text", "")
            if isinstance(text, str) and text:
                # Native wording preserves interruption causes; remove keyboard-only advice.
                lines.append(text.removeprefix("Confirm: ").replace(" (Case Sensitive)", ""))
            if kind == "prompt":
                for choice in self.actions():
                    label = str(choice.get("label", "")).strip()
                    if choice.get("id") == "prompt.choose" and label.casefold() in {"yes", "no"}:
                        lines.append(f"{label.upper()} → play {label.lower()}")
                if len(lines) < 2:
                    lines.append("This prompt needs a choice that the waiting interface does not yet support.")
            elif kind == "activity_wait":
                delta = self.elapsed_minutes(current)
                if delta is not None:
                    lines.append(f"Waiting: {elapsed(delta)} elapsed of {elapsed(self.state['requested_minutes'])}.")
                else:
                    lines.append("Activity in progress.")
                if any(a.get("id") == "activity.pause" for a in self.actions()):
                    lines.append("Stop waiting → play stop")
            elif kind == "world":
                delta = self.elapsed_minutes(current)
                if self.state.get("waiting") and delta is not None:
                    if delta >= self.state["requested_minutes"]:
                        lines.append(f"Waited {elapsed(delta)}. Ready.")
                    else:
                        lines.append(f"Waiting ended after {elapsed(delta)}. Ready.")
                    self.state["waiting"] = False
                else:
                    lines.append("Ready. Start waiting → play wait 5m")
            else:
                lines.append("The game is not ready for waiting. Check again → play look")
        lines.extend(performance_text(result.get("turn_assessment", {})))
        return "\n".join(lines)

    def record(self, command, text):
        # Repeated unchanged observations are useful on screen, not as saved history.
        if command != "look" or text != self.state.get("last_text"):
            with (self.client.session / "playtest.txt").open("a", encoding="utf-8") as log:
                log.write(f"> play {command}\n{text}\n\n")
        self.state["last_text"] = text
        self.client.save()


def main(argv=None):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(prog="play", description="Operate your waiting playtest.")
    parser.add_argument("command", choices=["look", "wait", "stop", "yes", "no", "quit"])
    parser.add_argument("duration", nargs="?")
    args = parser.parse_args(argv)
    if args.duration and args.command != "wait":
        parser.error("Only play wait takes a duration.")
    session = os.environ.get("CAOL_PLAY_SESSION")
    if not session:
        print("No playtest session is bound. Launch a playtest before using play.")
        return 1
    try:
        directory = Path(session).resolve(strict=True)
        manifest = json.loads((directory / "bridge.manifest.json").read_text(encoding="utf-8"))
        if manifest.get("plain_waiting") is not True:
            print("Launch a fresh waiting session with CAOL_PLAIN_WAITING=1 before using play.")
            return 1
        with session_lock(directory / "play-client.lock"):
            player = WaitingPlayer(PlayerClient(directory, plain_waiting=True))
            command = args.command + (" " + args.duration if args.duration else "")
            try:
                result = player.run(args.command, args.duration)
                text = player.render(result)
            except (OSError, ValueError, KeyError, TypeError) as error:
                text = str(error)
                result = {"ok": False}
            player.record(command, text)
            print(text)
            return 0 if result.get("ok") else 1
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("Cannot access this playtest: " + str(error))
        return 1


if __name__ == "__main__":
    sys.exit(main())
