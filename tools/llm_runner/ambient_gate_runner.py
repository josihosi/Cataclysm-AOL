#!/usr/bin/env python3
import argparse
import json
import math
import os
import sys
import time
import traceback
from typing import Any, Dict, Optional, TextIO


TRIGGER_HYPOTHESIS = (
    "An NPC nearby would find this player log line eyebrow-raising and worth reacting to."
)
IGNORE_HYPOTHESIS = (
    "An NPC nearby would consider this player log line ordinary and not worth reacting to."
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Ambient log gate runner (stdin/stdout JSON).",
    )
    parser.add_argument("--model-dir", required=True, help="Path to OpenVINO gate model directory.")
    parser.add_argument("--device", default="NPU", help="OpenVINO target device.")
    parser.add_argument("--log-file", default="", help="Optional runner log path.")
    parser.add_argument("--max-length", type=int, default=64, help="Tokenizer max length.")
    parser.add_argument("--backend", default="openvino")
    parser.add_argument("--max-tokens", type=int, default=1)
    parser.add_argument("--max-prompt-len", type=int, default=256)
    parser.add_argument("--cache-dir", default="")
    parser.add_argument("--force-npu", action="store_true")
    return parser.parse_args()


def setup_logger(log_path: str) -> Optional[TextIO]:
    if not log_path:
        return None
    try:
        os.makedirs(os.path.dirname(log_path), exist_ok=True)
        return open(log_path, "a", encoding="utf-8")
    except Exception:
        return None


def log_line(log_fp: Optional[TextIO], message: str) -> None:
    if log_fp is None:
        return
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    log_fp.write(f"[{timestamp}] {message}\n")
    log_fp.flush()


def write_response(payload: Dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(payload, ensure_ascii=True) + "\n")
    sys.stdout.flush()


def softmax(values: Any) -> list[float]:
    numbers = [float(v) for v in values]
    top = max(numbers)
    exps = [math.exp(v - top) for v in numbers]
    total = sum(exps)
    return [v / total for v in exps]


def infer_entailment_index(id2label: Dict[Any, Any]) -> int:
    for key, value in id2label.items():
        if "entail" in str(value).lower():
            return int(key)
    return 0


def score_hypothesis(compiled: Any, tokenizer: Any, entailment_index: int,
                     premise: str, hypothesis: str, max_length: int) -> float:
    encoded = tokenizer(
        premise,
        hypothesis,
        truncation=True,
        padding="max_length",
        max_length=max_length,
        return_tensors="np",
    )
    result = compiled({
        "input_ids": encoded["input_ids"],
        "attention_mask": encoded["attention_mask"],
    })
    logits = next(iter(result.values()))[0]
    probabilities = softmax(logits.tolist())
    return float(probabilities[entailment_index])


def main() -> int:
    args = parse_args()
    log_fp = setup_logger(args.log_file)

    try:
        import openvino as ov
        from transformers import AutoConfig, AutoTokenizer
    except Exception as exc:
        write_response({
            "request_id": "startup",
            "ok": False,
            "error": f"Ambient gate imports failed: {exc}",
        })
        return 1

    try:
        core = ov.Core()
        model_path = os.path.join(args.model_dir, "openvino_model.xml")
        compiled = core.compile_model(model_path, args.device)
        tokenizer = AutoTokenizer.from_pretrained(args.model_dir)
        config = AutoConfig.from_pretrained(args.model_dir)
        entailment_index = infer_entailment_index(getattr(config, "id2label", {}))
        execution_devices = compiled.get_property("EXECUTION_DEVICES")
        log_line(log_fp, f"ambient gate ready device={args.device} execution={execution_devices}")
    except Exception as exc:
        write_response({
            "request_id": "startup",
            "ok": False,
            "error": f"Ambient gate startup failed: {exc}",
        })
        return 1

    for line in sys.stdin:
        request_id = "unknown"
        try:
            request = json.loads(line)
            request_id = str(request.get("request_id", "unknown"))
            if request.get("command") == "shutdown":
                write_response({"request_id": request_id, "ok": True, "shutdown": True})
                break
            premise = str(request.get("prompt", "")).strip()
            if not premise:
                write_response({
                    "request_id": request_id,
                    "ok": False,
                    "error": "Missing prompt",
                })
                continue
            start = time.perf_counter()
            trigger_score = score_hypothesis(
                compiled, tokenizer, entailment_index, premise, TRIGGER_HYPOTHESIS, args.max_length
            )
            ignore_score = score_hypothesis(
                compiled, tokenizer, entailment_index, premise, IGNORE_HYPOTHESIS, args.max_length
            )
            elapsed_ms = (time.perf_counter() - start) * 1000.0
            decision = "trigger" if trigger_score > ignore_score else "ignore"
            write_response({
                "request_id": request_id,
                "ok": True,
                "text": decision,
                "trigger_score": trigger_score,
                "ignore_score": ignore_score,
                "elapsed_ms": elapsed_ms,
                "device": execution_devices,
            })
        except Exception as exc:
            log_line(log_fp, f"request failed id={request_id} error={exc}")
            write_response({
                "request_id": request_id,
                "ok": False,
                "error": str(exc),
                "traceback": traceback.format_exc(),
            })

    if log_fp is not None:
        log_fp.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
