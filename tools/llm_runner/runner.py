#!/usr/bin/env python3
import argparse
import json
import sys
from typing import Any, Dict


def strip_think_tags(text: str) -> str:
    if "<think>" in text and "</think>" in text:
        head, rest = text.split("<think>", 1)
        _think, tail = rest.split("</think>", 1)
        return f"{head}{tail}"
    return text


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="OpenVINO GenAI LLM runner (stdin/stdout JSON).",
    )
    parser.add_argument("--model-dir", required=True, help="Path to model directory.")
    parser.add_argument("--device", default="NPU", help="Target device (default: NPU).")
    parser.add_argument(
        "--max-tokens",
        type=int,
        default=256,
        help="Default max tokens per request.",
    )
    parser.add_argument(
        "--force-npu",
        action="store_true",
        help="Fail if NPU is not available.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Validate arguments and exit without loading OpenVINO.",
    )
    return parser.parse_args()


def ensure_npu_only() -> None:
    import openvino as ov

    core = ov.Core()
    devices = core.available_devices
    if "NPU" not in devices:
        raise RuntimeError(f"NPU not available. Devices: {devices}")


def load_pipeline(model_dir: str, device: str):
    import openvino_genai as ov_genai

    return ov_genai.LLMPipeline(
        model_dir,
        device,
        {},
        ENABLE_CPU_FALLBACK="NO",
    )


def read_request(line: str) -> Dict[str, Any]:
    return json.loads(line)


def write_response(payload: Dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(payload, ensure_ascii=True) + "\n")
    sys.stdout.flush()


def handle_request(pipe, request: Dict[str, Any], default_max_tokens: int) -> Dict[str, Any]:
    request_id = request.get("request_id", "unknown")
    if request.get("command") == "shutdown":
        return {"request_id": request_id, "ok": True, "shutdown": True}

    prompt = request.get("prompt")
    if not isinstance(prompt, str) or not prompt.strip():
        return {"request_id": request_id, "ok": False, "error": "Missing prompt"}

    max_tokens = request.get("max_tokens", default_max_tokens)
    if not isinstance(max_tokens, int) or max_tokens <= 0:
        max_tokens = default_max_tokens

    try:
        result = pipe.generate(
            prompt,
            max_length=max_tokens,
            do_sample=False,
        )
        text = "" if result is None else strip_think_tags(str(result))
        return {"request_id": request_id, "ok": True, "text": text}
    except Exception as exc:
        return {"request_id": request_id, "ok": False, "error": str(exc)}


def main() -> int:
    args = parse_args()
    if args.dry_run:
        print("dry-run ok")
        return 0

    if args.force_npu:
        try:
            ensure_npu_only()
        except Exception as exc:
            print(f"NPU check failed: {exc}", file=sys.stderr)
            return 1

    try:
        pipe = load_pipeline(args.model_dir, args.device)
    except Exception as exc:
        print(f"Failed to load pipeline: {exc}", file=sys.stderr)
        return 1

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            request = read_request(line)
        except Exception as exc:
            write_response({"request_id": "unknown", "ok": False, "error": str(exc)})
            continue

        response = handle_request(pipe, request, args.max_tokens)
        write_response(response)
        if response.get("shutdown"):
            return 0

    return 0


if __name__ == "__main__":
    sys.exit(main())
