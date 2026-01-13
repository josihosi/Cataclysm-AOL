#!/usr/bin/env python3
import argparse
import json
import os
import queue
import subprocess
import sys
import threading
import time
from typing import Dict, Optional


ALLOWED_ACTIONS = ["wait_here", "follow_player", "equip_gun", "equip_melee", "use_bow", "attack=<target>" ,"idle"]

DEFAULT_SYSTEM_PROMPT = ("""
<System>You are a game NPC response engine, supposed to respond to player_utterance. Return ONLY a single CSV line and nothing else.This CSV line has one to four fields separated by '|':
<Field 1>The first field is an answer to the player utterance.Respond as the NPC.Stick to your role, assume your emotions and opinions.Use a nitti gritty tone fit for a zombie apocalypse.</Field 1>
<Fields 2-4>Write 1-3 of the following allowed actions:wait_here, follow_player, equip_gun, equip_melee, use_bow, idle
Optional target hint token: attack=<target> (single token, no spaces).
wait_here to stay put, keep watch, wait, stand.
follow_player to walk behind, follow, run.
equip_gun to equip gun, rifle, thrower.
equip_melee to equip melee, bash, cut, kick.
use_bow to use bow, crossbow, stealth.
attack=<target> to target and attack a creature from your map.
idle if none of the above.</Fields 2-4>
Print nothing else other than Fields 1-4, separated by |If you break that format, you have failed.Output must be a single line with no markdown or extra text.Absolutely no notes, explanations, examples, or parenthetical text.<Example Output>What?|idle</Example Output>
<Example Output>Lets put them in the ground.|equip_melee</Example Output>
<Example Output>
Providing cover!|wait_here|equip_gun</Example Output>
</System>
"""
)

DEFAULT_SNAPSHOT = ("""
Situation:
SITUATION
id: req_0
player_name: TonZa
player_utterance: shoot the axe guy, leoma!

your_name: Leoma Heck
your_state: morale=0 hunger=0 thirst=0 pain=31 stamina=10000/10000 sleepiness=109 hp_percent=95 effects=[bleed:7 socialized_recently:1 bandaged:1 bandaged:1 social_satisfied:1 asked_to_socialize:1]
your_emotions: danger_assessment=0 panic=0 confidence=1 emergency=false
your_personality: aggression=-1 bravery=10 collector=2 altruism=-2
your_opinion_of_player: trust=5 fear=6 value=1 anger=4

threats: axe murderer@1 ts=4.55357
friendlies: player@2

ruleset: attitude=NPCATT_FOLLOW rules=[allow_pulp use_silent avoid_friendly_fire allow_complain follow_close ignore_noise]

inventory: wielded="lug wrench" ammo=0/0 reload_needed=false weight_percent=48 volume_percent=76
inventory_usable: [lug wrench, ++ fedora, cellphone]
inventory_combat: [lug wrench, ++ brass knuckles (pair), screwdriver (in use)]
bandage_possible: true

legend:
- ... open area
0 ... obstructive area (movement speed > 100)
6 ... obstructed area
[a - z] ... creature
[A - Z] ... obstructed creature
| ... You (NPC)
map_legend:
a ... player
b ... axe murderer
map:
-----------------------------------------
-----------------------------------------
-----------------------------------------
--------------------|--------------------
------------------a--b-------------------
-----------------------------------------
-----------------------------------------
-----------------------------------------
-----------------------------------------
-----------------------------------------
""")


def read_text(path: str) -> str:
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        return handle.read()


def repo_root() -> str:
    return os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def load_options(path: str) -> Dict[str, str]:
    if not path or not os.path.exists(path):
        return {}
    try:
        data = json.loads(read_text(path))
    except Exception:
        return {}
    if not isinstance(data, list):
        return {}
    options: Dict[str, str] = {}
    for entry in data:
        if not isinstance(entry, dict):
            continue
        name = entry.get("name")
        value = entry.get("value")
        if isinstance(name, str) and isinstance(value, str):
            options[name] = value
    return options


def parse_bool(value: Optional[str], default: bool = False) -> bool:
    if value is None:
        return default
    return value.strip().lower() in {"1", "true", "yes", "on"}


def parse_int(value: Optional[str], default: int) -> int:
    if value is None:
        return default
    try:
        return int(value)
    except Exception:
        return default


def resolve_default_paths(options: Dict[str, str], root: str) -> Dict[str, str]:
    runner_path = options.get("LLM_INTENT_RUNNER", "tools/llm_runner/runner.py")
    if runner_path and not os.path.isabs(runner_path):
        runner_path = os.path.abspath(os.path.join(root, runner_path))
    return {
        "python_path": options.get("LLM_INTENT_PYTHON", ""),
        "runner_path": runner_path,
        "model_dir": options.get("LLM_INTENT_MODEL_DIR", ""),
        "device": options.get("LLM_INTENT_DEVICE", "NPU"),
        "max_prompt_len": str(parse_int(options.get("LLM_INTENT_MAX_PROMPT_LEN"), 4096)),
        "force_npu": str(parse_bool(options.get("LLM_INTENT_FORCE_NPU"), False)),
    }


def build_prompt(snapshot: str, system_prompt: str) -> str:
    return f"Situation:\n{snapshot}\n{system_prompt}"


def run_runner(
    python_path: str,
    runner_path: str,
    model_dir: str,
    device: str,
    max_tokens: int,
    max_prompt_len: int,
    force_npu: bool,
    cache_dir: str,
    runner_log: str,
    prompt: str,
    snapshot: str,
    timeout: float,
) -> Dict[str, object]:
    if not python_path:
        python_path = sys.executable
    cmd = [
        python_path,
        runner_path,
        "--model-dir",
        model_dir,
        "--device",
        device,
        "--max-tokens",
        str(max_tokens),
        "--max-prompt-len",
        str(max_prompt_len),
    ]
    if force_npu:
        cmd.append("--force-npu")
    if cache_dir:
        cmd.extend(["--cache-dir", cache_dir])
    if runner_log:
        cmd.extend(["--log-file", runner_log])

    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )

    request_id = "playground_1"
    request = {
        "request_id": request_id,
        "prompt": prompt,
        "snapshot": snapshot,
        "max_tokens": max_tokens,
    }
    assert proc.stdin is not None
    proc.stdin.write(json.dumps(request, ensure_ascii=True) + "\n")
    proc.stdin.flush()

    start = time.time()
    response: Optional[Dict[str, object]] = None
    timed_out = False
    line_queue: "queue.Queue[str]" = queue.Queue()

    def read_stdout() -> None:
        assert proc.stdout is not None
        for line in proc.stdout:
            line_queue.put(line)

    reader = threading.Thread(target=read_stdout, daemon=True)
    reader.start()

    while True:
        if timeout > 0 and (time.time() - start) > timeout:
            timed_out = True
            break
        if proc.poll() is not None and line_queue.empty():
            break
        try:
            line = line_queue.get(timeout=0.1)
        except queue.Empty:
            continue
        try:
            payload = json.loads(line)
        except Exception:
            continue
        if isinstance(payload, dict) and payload.get("request_id") == request_id:
            response = payload
            break

    try:
        shutdown = {"request_id": request_id, "command": "shutdown"}
        proc.stdin.write(json.dumps(shutdown, ensure_ascii=True) + "\n")
        proc.stdin.flush()
    except Exception:
        pass
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

    if response is None:
        stderr = ""
        if proc.stderr is not None:
            stderr = proc.stderr.read()
        if timed_out:
            raise RuntimeError(f"Timed out waiting for runner response. stderr:\n{stderr}")
        raise RuntimeError(f"No response from runner. stderr:\n{stderr}")
    return response


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Prompt playground for LLM intent tuning.",
    )
    parser.add_argument(
        "--snapshot",
        help="Optional path to snapshot text (overrides embedded snapshot).",
    )
    prompt_group = parser.add_mutually_exclusive_group()
    prompt_group.add_argument("--prompt", help="Inline prompt template.")
    prompt_group.add_argument("--prompt-path", help="Path to prompt template file.")
    parser.add_argument(
        "--options-path",
        default=os.path.join(repo_root(), "config", "options.json"),
        help="Path to game options.json for default runner settings.",
    )
    parser.add_argument("--python-path", default="", help="Override python.exe path.")
    parser.add_argument("--runner-path", default="", help="Override runner.py path.")
    parser.add_argument("--model-dir", default="", help="Override model directory.")
    parser.add_argument("--device", default="", help="Override device (e.g. NPU).")
    parser.add_argument("--max-tokens", type=int, default=20000, help="Max tokens.")
    parser.add_argument(
        "--max-prompt-len",
        type=int,
        default=0,
        help="Max prompt length for pipeline (0 uses options.json/default).",
    )
    parser.add_argument("--force-npu", action="store_true", default=None)
    parser.add_argument("--no-force-npu", dest="force_npu", action="store_false")
    parser.add_argument("--cache-dir", default="", help="Optional OpenVINO cache dir.")
    parser.add_argument("--runner-log", default="", help="Optional runner log file.")
    parser.add_argument(
        "--timeout",
        type=float,
        default=120.0,
        help="Timeout in seconds for response (0 disables).",
    )
    parser.add_argument(
        "--print-prompt",
        action="store_true",
        help="Print the final prompt before sending to the runner.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    snapshot = DEFAULT_SNAPSHOT
    if args.snapshot:
        snapshot = read_text(args.snapshot)

    if args.prompt:
        system_prompt = args.prompt
    elif args.prompt_path:
        system_prompt = read_text(args.prompt_path)
    else:
        system_prompt = DEFAULT_SYSTEM_PROMPT

    root = repo_root()
    options = load_options(args.options_path)
    defaults = resolve_default_paths(options, root)

    python_path = args.python_path or defaults["python_path"]
    runner_path = args.runner_path or defaults["runner_path"]
    model_dir = args.model_dir or defaults["model_dir"]
    device = args.device or defaults["device"]

    max_prompt_len = args.max_prompt_len
    if max_prompt_len <= 0:
        max_prompt_len = parse_int(defaults["max_prompt_len"], 4096)

    if args.force_npu is None:
        force_npu = parse_bool(defaults["force_npu"], False)
    else:
        force_npu = args.force_npu

    if not model_dir:
        raise RuntimeError("Model directory is required. Use --model-dir or set LLM_INTENT_MODEL_DIR.")
    if not runner_path:
        raise RuntimeError("Runner path is required. Use --runner-path or set LLM_INTENT_RUNNER.")

    prompt = build_prompt(snapshot, system_prompt)
    if args.print_prompt:
        print(prompt)

    response = run_runner(
        python_path=python_path,
        runner_path=runner_path,
        model_dir=model_dir,
        device=device,
        max_tokens=args.max_tokens,
        max_prompt_len=max_prompt_len,
        force_npu=force_npu,
        cache_dir=args.cache_dir,
        runner_log=args.runner_log,
        prompt=prompt,
        snapshot=snapshot,
        timeout=args.timeout,
    )

    ok = response.get("ok", False)
    text = response.get("text", "")
    metrics = response.get("metrics", {})
    error = response.get("error", "")

    if ok:
        print(text)
        if metrics:
            print("\nMETRICS")
            print(json.dumps(metrics, indent=2, ensure_ascii=True))
    else:
        print(f"Runner error: {error}")
        print(json.dumps(response, indent=2, ensure_ascii=True))
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
