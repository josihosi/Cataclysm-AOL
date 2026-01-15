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


ALLOWED_ACTIONS = ["wait_here", "follow_player", "equip_gun", "equip_melee", "equip_bow", "attack=<target>", "idle"]
DEFAULT_MODEL_DIR = r"C:\Users\josef\openvino_models\Phi-3.5-mini-instruct-int4-cw-ov"
# Other local models (leave commented to keep a single active model).
# r"C:\Users\josef\openvino_models\Mistral-7B-Instruct-v0.3-int4-cw-ov",
# r"C:\Users\josef\openvino_models\DeepSeek-R1-Distill-Qwen-1.5B-int4-cw-ov",
# r"C:\Users\josef\openvino_models\Mistral-7B-Instruct-v0.2-int4-cw-ov",
# r"C:\Users\josef\openvino_models\qwen3-8b-int4-cw-ov",

DEFAULT_SYSTEM_PROMPT = (
               "Situation:\n%s\n"
               "<System>"
               "You are controlling a human survivor NPC in a cataclysmic world, exhausted, armed, and trying not to die."
               "Return a single line only, with correct syntax, to be parsed by the game."
               "This line has two to four fields separated by ‘|’ :\n"
               "<Field 1>"
               "The first field is an answer to player_utterance."
               "You have decided to team up with the player for now, and must answer as the NPC."
               "Stick to your role, with your emotions and opinions."
               "Use a dry tone, with swear words, fit for a zombie apocalypse."
               "</Field 1>\n"
               "<Fields 2-4>"
               "Write 1-3 of the following allowed actions:"
               "%s\n"
               "<Allowed actions>"
               "wait_here to stay put, keep watch, wait, stand.\n"
               "follow_player to walk behind, follow, run.\n"
               "equip_gun to equip gun, rifle, thrower, get ready to shoot.\n"
               "equip_melee to equip melee, get ready to bash, cut, kick, stab.\n"
               "equip_bow to use bow, crossbow, stealth.\n"
               "attack=<target> to attack a target from your map.\n"
               "idle if none of the above.\n"
               "</Allowed actions>\n"
               "</Fields 2-4>\n"
               "Print only Fields 1-4, separated by | ."
               "If you break this format, you have failed."
               "Output a single line with an answer and actions from the allowed list, in fields separated by ‘|’ and no additional text.\n"
               "<Example Output 1>"
               "Blow me.|idle"
               "</Example Output 1>\n"
               "<Example Output 2>"
               "Lets put those fucks in the ground.|equip_melee|attack=zombie"
               "</Example Output 2>\n"
               "<Example Output 3>"
               "Providing cover!|wait_here|equip_gun"
               "</Example Output 3>\n"
               "<Example Output 4>"
               "Lets get some dinner!|equip_gun|attack=chicken"
               "</Example Output 4>\n"
               "<Example Output 5>"
               "Don't worry, I'm ready to kick some teeth in.|equip_melee"
               "</Example Output 5>\n"
               "<Example Output 6>"
               "Locked and loaded.|equip_gun"
               "</Example Output 6>\n"
               "</System>\n"
,)

TEMPERATURE = 0.6
TOP_P = 0.9
REPETITION_PENALTY = 1.1

DEFAULT_SNAPSHOT = ("""
SITUATION
id: req_1
player_name: Norine 'Red' Marroquin
player_utterance: Get your bow out and shoot the axe murderer! I'll attack the Zombie!

your_name: Jewell Cheek
your_state: morale=5 hunger=0 thirst=0 pain=10 stamina=10000/10000 sleepiness=0 hp_percent=98 effects=[]
your_emotions: danger_assessment=10 panic=2 confidence=1
your_personality: aggression=3 bravery=7 collector=2 altruism=1
your_opinion_of_player: trust=3 intimidation=-3 respect=0 anger=0

threats: True
friendlies: player@2

inventory: wielded="F-S fighting knife"
inventory_usable: [|\ fancy hairpin, smartphone (UPS) (unbrowsed), cash card]
inventory_combat: [Cops' Glock 22 (15/15), hammer, F-S fighting knife]
bandage_possible: false

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
c ... zombie
map:
-----------------------------------------
-----------------------------------------
-----------------------------------------
------------------------------------0----
-----------------------------------------
-------------------------------0---------
----------------------------------------0
-----------------------------------------
----------------------------------------0
----------------------------60-----------
-----------------------------------------
-----------------------------------------
-----------------------------------------
-------------------------------0-0-------
---------------------------------0-------
---------------------------------------0-
------------------------------0----------
---------c-------------------------------
------------------------b----6-------06--
-------------------------------0-0------6
--------------------|-----------------0--
------------------a----------------60----
-------------------------0----0----------
--------------------------------0--------
-----------------------------0--0----0---
-----------------------------------0-----
---------------------------------0-------
-----------------------------------------
--------------------------------0--0-----
-----------------------------------------
-------------------------------0---------
-----------------------------0--------0--
---------------------------0-------------
----------------------------------------0
---------------------------------6-----0-
-----------------------------------------
---------------------------0-----------0-
--------------------------------0--------
-----------------------------------------
-----------------------------------------
-----------------------------------0-----

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
    runner_path = os.path.abspath(os.path.join(root, "tools", "llm_runner", "runner.py"))
    return {
        "python_path": options.get("LLM_INTENT_PYTHON", ""),
        "runner_path": runner_path,
        "model_dir": options.get("LLM_INTENT_MODEL_DIR", ""),
        "device": options.get("LLM_INTENT_DEVICE", "NPU"),
        "max_prompt_len": str(parse_int(options.get("LLM_INTENT_MAX_PROMPT_LEN"), 4096)),
        "force_npu": str(parse_bool(options.get("LLM_INTENT_FORCE_NPU"), False)),
        "use_api": options.get("LLM_INTENT_USE_API", ""),
        "api_key_env": options.get("LLM_INTENT_API_KEY_ENV", ""),
        "api_provider": options.get("LLM_INTENT_API_PROVIDER", ""),
        "api_model": options.get("LLM_INTENT_API_MODEL", ""),
    }


def build_prompt(snapshot: str, system_prompt: str) -> str:
    return f"Situation:\n{snapshot}\n{system_prompt}"


def extract_csv_line(text: str) -> str:
    if not text:
        return ""
    trimmed = text.strip()
    if "</think>" in trimmed:
        trimmed = trimmed.split("</think>")[-1].strip()
    lines = [line.strip() for line in trimmed.splitlines() if line.strip()]
    for line in reversed(lines):
        if "|" in line:
            return line
    return trimmed


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
    use_api: bool,
    api_key_env: str,
    api_provider: str,
    api_model: str,
) -> Dict[str, object]:
    if not python_path:
        python_path = sys.executable
    cmd = [python_path, runner_path]
    if use_api:
        cmd.append("--use-api")
        cmd.extend(["--api-provider", api_provider])
        cmd.extend(["--api-model", api_model])
        if api_key_env:
            cmd.extend(["--api-key-env", api_key_env])
        cmd.extend(["--max-tokens", str(max_tokens)])
    else:
        cmd.extend(
            [
                "--model-dir",
                model_dir,
                "--device",
                device,
                "--max-tokens",
                str(max_tokens),
                "--max-prompt-len",
                str(max_prompt_len),
            ]
        )
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
        "temperature": TEMPERATURE,
        "top_p": TOP_P,
        "repetition_penalty": REPETITION_PENALTY,
    }
    assert proc.stdin is not None
    try:
        proc.stdin.write(json.dumps(request, ensure_ascii=True) + "\n")
        proc.stdin.flush()
    except BrokenPipeError:
        stderr = ""
        if proc.stderr is not None:
            stderr = proc.stderr.read()
        raise RuntimeError(f"Runner exited before request was sent. stderr:\n{stderr}")

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
    parser.add_argument("--use-api", action="store_true", help="Use API mode.")
    parser.add_argument("--api-key-env", default="", help="API key env var name.")
    parser.add_argument("--api-provider", default="", help="API provider name.")
    parser.add_argument("--api-model", default="", help="API model name.")
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
    max_attempts = 2

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
    model_dir = args.model_dir or defaults["model_dir"] or DEFAULT_MODEL_DIR
    device = args.device or defaults["device"]
    use_api = args.use_api or parse_bool(defaults["use_api"], False)
    api_key_env = args.api_key_env or defaults["api_key_env"]
    api_provider = args.api_provider or defaults["api_provider"]
    api_model = args.api_model or defaults["api_model"]

    max_prompt_len = args.max_prompt_len
    if max_prompt_len <= 0:
        max_prompt_len = parse_int(defaults["max_prompt_len"], 4096)

    if args.force_npu is None:
        force_npu = parse_bool(defaults["force_npu"], False)
    else:
        force_npu = args.force_npu

    if not runner_path:
        raise RuntimeError("Runner path is required. Use --runner-path if the repo path is non-standard.")
    if not use_api and not model_dir:
        raise RuntimeError("Model directory is required for local mode.")
    if use_api and (not api_provider or not api_model):
        raise RuntimeError("API mode requires provider and model.")

    prompt = build_prompt(snapshot, system_prompt)
    if args.print_prompt:
        print(prompt)

    exit_code = 0
    if not use_api:
        print(f"\nMODEL {model_dir}")
    attempt = 0
    response = None
    timeout = args.timeout
    while attempt < max_attempts:
        try:
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
                timeout=timeout,
                use_api=use_api,
                api_key_env=api_key_env,
                api_provider=api_provider,
                api_model=api_model,
            )
            break
        except RuntimeError as exc:
            attempt += 1
            if "Timed out waiting for runner response" in str(exc) and timeout > 0 and attempt < max_attempts:
                timeout = timeout * 2
                continue
            print(str(exc))
            exit_code = 1
            break

    if response is None:
        return exit_code

    ok = response.get("ok", False)
    text = response.get("text", "")
    metrics = response.get("metrics", {})
    error = response.get("error", "")

    if ok:
        parsed_text = extract_csv_line(text)
        print(parsed_text)
        if parsed_text != text.strip():
            print(f"RAW {text.strip()}")
        if isinstance(metrics, dict):
            gen_time = metrics.get("gen_time_ms")
            if isinstance(gen_time, (int, float)):
                print(f"gen_time_ms: {float(gen_time):.2f}")
    else:
        print(f"Runner error: {error}")
        print(json.dumps(response, indent=2, ensure_ascii=True))
        exit_code = 1

    return exit_code


if __name__ == "__main__":
    sys.exit(main())
