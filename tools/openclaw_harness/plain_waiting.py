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
from waiting_interruptions import continuation


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
        result = self.client.act(action, target, {}, 1)
        self.capture(result)
        return result

    def capture(self, result):
        if not hasattr(self, "events"):
            return
        if any(result is previous for previous in self.captured):
            return
        self.captured.append(result)
        self.events.extend(performance_text(result.get("turn_assessment", {})))
        if result.get("ok") and result.get("state") != "pending":
            self.events.extend(self.new_messages())
        if result.get("state") != "pending" and self.state.get("automatic_reply"):
            cause = self.state.pop("automatic_reply")
            self.events.append(cause + (" (" + self.state["mode"] + " mode)."
                                       if result.get("ok") else " — continuation failed."))
            self.client.save()

    def new_messages(self):
        facts = self.current().get("surface", {}).get("facts", {})
        messages = decode(facts.get("messages", []))
        lines = []
        if "messages" in facts and isinstance(messages, list):
            previous = self.state.get("messages")
            if previous is not None:
                overlap = min(len(previous), len(messages))
                while overlap and previous[-overlap:] != messages[:overlap]:
                    overlap -= 1
                for message in messages[overlap:]:
                    if isinstance(message, dict) and message.get("text"):
                        lines.append((str(message["time"]) + ": " if message.get("time") else "") + message["text"])
            self.state["messages"] = messages
        return lines

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

    def run(self, command, duration=None, mode="ignore"):
        if mode not in {"ignore", "safe", "stop"}:
            raise ValueError("Waiting mode must be ignore, safe, or stop.")
        self.events, self.captured = [], []
        result = self.run_once(command, duration, mode)
        self.capture(result)
        while (result.get("ok") and result.get("state") != "pending" and
               self.state.get("waiting") and not self.state.get("manual_stop") and
               not self.client.state.get("finished") and command != "quit"):
            surface = self.current().get("surface", {})
            action = continuation(surface, self.state.get("mode", "stop"))
            if action is None:
                break
            owner = self.client.state.get("observation_id")
            self.state["automatic_reply"] = (str(surface.get("facts", {}).get("text") or "Interruption") +
                                              " → " + str(action.get("label") or action["id"]))
            self.client.save()
            result = self.act(action["id"], action.get("stable_id") or None)
            if result.get("state") != "pending" and result.get("ok") and self.client.state.get("observation_id") == owner:
                result = {"ok": False, "error": "The interruption did not advance after its response. Run play look."}
                break
        return {**result, "plain_events": self.events, "turn_assessment": {}}

    def run_once(self, command, duration=None, mode="ignore"):
        if self.state.get("finishing"):
            status = json.loads((self.client.session / "status.json").read_text(encoding="utf-8"))
            cleanup = status.get("cleanup", {})
            if status.get("binding_id") != self.client.binding:
                return {"ok": False, "error": "Session binding changed during cleanup."}
            if status.get("state") == "safe_to_cleanup" or cleanup.get("game", {}).get("status") in {
                    "terminated", "already_exited", "killed", "terminated_during_kill_escalation"}:
                self.state.pop("finishing", None)
                self.client.state.pop("pending", None)
                self.client.state["finished"] = True
                self.client.save()
                return {"ok": True}
            if status.get("state") in {"cleaned", "bridge_failed", "process_dead", "terminalization_failed"}:
                return {"ok": False, "error": "Cleanup did not confirm that the game stopped."}
            return {"ok": True, "state": "pending"}
        if command == "quit":
            status = json.loads((self.client.session / "status.json").read_text(encoding="utf-8"))
            if status.get("state") in {"process_dead", "bridge_failed", "terminalization_failed", "reentry_failed"}:
                from cockpit_file_bridge import FileBackedCockpitBridge
                result = FileBackedCockpitBridge.cleanup(self.client.session, self.client.binding)
                if result.get("ok") and result.get("cleanup") == "requested":
                    self.state["finishing"] = True
                    self.client.save()
                    return {"ok": True, "state": "pending"}
                if result.get("ok"):
                    self.client.state.pop("pending", None)
                    self.client.state["finished"] = True
                    self.client.save()
                return result
        if self.client.state.get("pending"):
            if command != "look":
                raise ValueError("A command is still pending. Run play look to collect its result.")
            result = self.client.collect(1)
            self.capture(result)
            return self.advance_wait(result)
        if command == "look":
            return self.client.submit({"action": "game.observe"}, 1)
        if command in {"yes", "no", "ignore"}:
            result = self.answer(command)
            if result.get("ok"):
                self.state["manual_stop"] = False
            return result
        if command == "choose":
            choices = self.actions()
            if not duration or not duration.isdigit() or not 1 <= int(duration) <= len(choices):
                raise ValueError("Choose a displayed option number. Run play look.")
            action = choices[int(duration) - 1]
            return self.act(action["id"], action.get("stable_id") or None)
        if command == "cancel":
            return self.act("prompt.cancel")
        if command == "continue":
            offered = [a for a in self.actions() if a.get("id") in {
                "prompt.acknowledge", "modal.acknowledge", "activity.continue"}]
            if len(offered) != 1:
                raise ValueError("The game does not offer a single continuation. Run play look.")
            return self.act(offered[0]["id"], offered[0].get("stable_id") or None)
        if command == "stop":
            self.state["manual_stop"] = True
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
                          selecting=True, answer=None, mode=mode, manual_stop=False)
        self.client.save()
        return self.advance_wait({"ok": True} if in_duration_menu else self.act("world.wait"))

    def render(self, result):
        lines = []
        if result.get("state") == "pending":
            lines = ["Command pending. Check result → play look"]
        elif not result.get("ok"):
            status = result.get("status", {})
            state_reason = ("Session is " + str(status["state"]).replace("_", " ")
                            if isinstance(status, dict) and status.get("state") else
                            "The game did not confirm the command.")
            reason = result.get("error", result.get("response", {}).get("error",
                                result.get("reason", state_reason)))
            lines = ["Command failed: " + str(reason).replace("_", " "), "Check the game → play look"]
        elif self.client.state.get("finished"):
            lines = ["Playtest ended."]
        else:
            current = self.current()
            surface = current.get("surface", {})
            kind = surface.get("kind")
            facts = surface.get("facts", {})
            lines.extend(self.new_messages())
            text = facts.get("text", "")
            if isinstance(text, str) and text:
                # Native wording preserves interruption causes; remove keyboard-only advice.
                lines.append(text.removeprefix("Confirm: ").replace(" (Case Sensitive)", ""))
            if kind in {"prompt", "activity_distraction", "menu"}:
                choices = 0
                for index, choice in enumerate(self.actions(), 1):
                    label = str(choice.get("label", "")).strip()
                    if choice.get("id") == "prompt.choose" and label.casefold() in {"yes", "no", "ignore"}:
                        lines.append(f"{label.upper()} → play {label.lower()}")
                        choices += 1
                    elif choice.get("id") not in {"prompt.acknowledge", "modal.acknowledge", "prompt.cancel"}:
                        lines.append(f"{label or choice['id']} → play choose {index}")
                        choices += 1
                if any(a.get("id") in {"prompt.acknowledge", "modal.acknowledge"} for a in self.actions()):
                    lines.append("Continue → play continue")
                    choices += 1
                if any(a.get("id") == "prompt.cancel" for a in self.actions()):
                    lines.append("Cancel → play cancel")
                    choices += 1
                if not choices:
                    lines.append("This prompt needs a choice that the waiting interface does not yet support.")
            elif kind in {"activity_wait", "wait_activity"}:
                delta = self.elapsed_minutes(current)
                if delta is not None:
                    lines.append(f"Waiting: {elapsed(delta)} elapsed of {elapsed(self.state['requested_minutes'])}.")
                else:
                    lines.append("Activity in progress.")
                if any(a.get("id") == "activity.pause" for a in self.actions()):
                    lines.append("Stop waiting → play stop")
                else:
                    lines.append("Check progress → play look")
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
            elif any(a.get("id") in {"modal.acknowledge", "activity.continue"} for a in self.actions()):
                lines.append("Continue → play continue")
            else:
                lines.append("The game is not ready for waiting. Check again → play look")
        lines.extend(result.get("plain_events", []))
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
    parser.add_argument("command", choices=["look", "wait", "stop", "yes", "no", "ignore", "choose", "continue", "cancel", "quit"])
    parser.add_argument("duration", nargs="?")
    parser.add_argument("mode", nargs="?", choices=["ignore", "safe", "stop"])
    args = parser.parse_args(argv)
    if args.duration and args.command not in {"wait", "choose"}:
        parser.error("Only play wait and play choose take an argument.")
    if args.mode and args.command != "wait":
        parser.error("Only play wait takes a mode.")
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
            command += " " + args.mode if args.mode else ""
            try:
                result = player.run(args.command, args.duration, args.mode or "ignore")
                text = player.render(result)
            except (OSError, ValueError, KeyError, TypeError) as error:
                text = "\n".join(getattr(player, "events", []) + [str(error)])
                result = {"ok": False}
            player.record(command, text)
            print(text)
            return 0 if result.get("ok") else 1
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("Cannot access this playtest: " + str(error))
        return 1


if __name__ == "__main__":
    sys.exit(main())
