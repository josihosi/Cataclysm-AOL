#!/usr/bin/env python3
import argparse
import json
import os
import sys
import time
import traceback
import urllib.request
import urllib.error
from typing import Any, Dict, List, Optional, TextIO, Tuple

TEMPERATURE = 0.6
TOP_P = 0.9
REPETITION_PENALTY = 1.0


def strip_think_tags(text: str) -> str:
    if "<think>" in text and "</think>" in text:
        head, rest = text.split("<think>", 1)
        _think, tail = rest.split("</think>", 1)
        return f"{head}{tail}"
    return text


def sanitize_text(text: Any) -> str:
    if not isinstance(text, str):
        return ""
    try:
        return text.encode("utf-8", "surrogateescape").decode("utf-8", "replace")
    except Exception:
        return text.encode("utf-8", "replace").decode("utf-8", "replace")


def sanitize_jsonish(value: Any) -> Any:
    if isinstance(value, str):
        return sanitize_text(value)
    if isinstance(value, list):
        return [sanitize_jsonish(item) for item in value]
    if isinstance(value, dict):
        return {key: sanitize_jsonish(item) for key, item in value.items()}
    return value


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="OpenVINO GenAI LLM runner (stdin/stdout JSON).",
    )
    parser.add_argument(
        "--backend",
        default="openvino",
        choices=["openvino", "api", "ollama"],
        help="Backend selection: openvino, api, or ollama.",
    )
    parser.add_argument("--model-dir", help="Path to model directory.")
    parser.add_argument("--device", default="AUTO", help="Target device (default: AUTO).")
    parser.add_argument(
        "--max-tokens",
        type=int,
        default=256,
        help="Default max tokens per request.",
    )
    parser.add_argument(
        "--max-prompt-len",
        type=int,
        default=20000,
        help="Maximum prompt length for the pipeline.",
    )
    parser.add_argument(
        "--force-npu",
        action="store_true",
        help="Fail if NPU is not available.",
    )
    parser.add_argument(
        "--cache-dir",
        default="",
        help="Directory for OpenVINO cache (optional).",
    )
    parser.add_argument(
        "--log-file",
        default="",
        help="Path to a log file for runner diagnostics.",
    )
    parser.add_argument(
        "--use-api",
        action="store_true",
        help="Use any-llm API calls instead of local OpenVINO inference.",
    )
    parser.add_argument(
        "--api-provider",
        default="",
        help="Provider name for any-llm (e.g., openai, mistral).",
    )
    parser.add_argument(
        "--api-model",
        default="",
        help="Model name to pass to any-llm.",
    )
    parser.add_argument(
        "--api-key-env",
        default="",
        help="Environment variable name that stores the API key.",
    )
    parser.add_argument(
        "--ollama-url",
        default="http://127.0.0.1:11434",
        help="Base URL for local Ollama server.",
    )
    parser.add_argument(
        "--ollama-model",
        default="",
        help="Model tag to use with Ollama.",
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="Run a minimal backend sanity check and exit.",
    )
    parser.add_argument(
        "--self-test-prompt",
        default="Hello from the LLM self-test.",
        help="Prompt used for --self-test.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Validate arguments and exit without loading OpenVINO.",
    )
    return parser.parse_args()


def ensure_npu_only() -> None:
    try:
        import openvino as ov
    except Exception as exc:
        raise RuntimeError(f"OpenVINO not available: {exc}")

    core = ov.Core()
    devices = core.available_devices
    if "NPU" not in devices:
        raise RuntimeError(f"NPU not available. Devices: {devices}")


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


def load_pipeline(model_dir: str, device: str, cache_dir: str, max_prompt_len: int):
    import openvino_genai as ov_genai

    if cache_dir:
        os.makedirs(cache_dir, exist_ok=True)
    is_cpu = device.upper() == "CPU"
    use_cpu_fallback = not is_cpu
    enable_cpu_fallback = "NO" if use_cpu_fallback else None
    try:
        kwargs = {}
        if not is_cpu:
            kwargs["MAX_PROMPT_LEN"] = max_prompt_len
        if cache_dir:
            kwargs["CACHE_DIR"] = cache_dir
        if enable_cpu_fallback:
            kwargs["ENABLE_CPU_FALLBACK"] = enable_cpu_fallback
        if kwargs:
            return ov_genai.LLMPipeline(model_dir, device, **kwargs)
        return ov_genai.LLMPipeline(model_dir, device)
    except TypeError:
        config = {}
        if not is_cpu:
            config["MAX_PROMPT_LEN"] = max_prompt_len
        if cache_dir:
            config["CACHE_DIR"] = cache_dir
        if enable_cpu_fallback:
            config["ENABLE_CPU_FALLBACK"] = enable_cpu_fallback
        if config:
            return ov_genai.LLMPipeline(
                model_dir,
                device,
                config,
            )
        return ov_genai.LLMPipeline(model_dir, device)


def build_tokenizer(model_dir: str):
    try:
        import openvino_genai as ov_genai

        tokenizer_cls = getattr(ov_genai, "Tokenizer", None)
        if tokenizer_cls is None:
            return None
        return tokenizer_cls(model_dir)
    except Exception:
        return None


def count_tokens(tokenizer, text: str) -> Tuple[int, str]:
    if tokenizer is None:
        return max(0, len(text.strip().split())), "whitespace"
    try:
        token_ids = tokenizer.encode(text)
        if isinstance(token_ids, dict) and "input_ids" in token_ids:
            token_ids = token_ids["input_ids"]
        try:
            count = int(token_ids.size)
        except Exception:
            try:
                count = len(token_ids)
            except Exception:
                count = max(0, len(text.strip().split()))
                return count, "whitespace"
        return count, "openvino_genai.Tokenizer"
    except Exception:
        return max(0, len(text.strip().split())), "whitespace"


def get_npu_metrics() -> Dict[str, Any]:
    try:
        import openvino as ov

        core = ov.Core()
        metrics = {}
        for key in ["DEVICE_UTILIZATION", "DEVICE_TOTAL_MEM_SIZE", "DEVICE_FREE_MEM_SIZE"]:
            try:
                metrics[key.lower()] = core.get_property("NPU", key)
            except Exception:
                continue
        return metrics
    except Exception:
        return {}


def read_request(line: str) -> Dict[str, Any]:
    return sanitize_jsonish(json.loads(line))


def write_response(payload: Dict[str, Any]) -> None:
    if "text" in payload:
        payload["text"] = sanitize_text(payload.get("text"))
    if "error" in payload:
        payload["error"] = sanitize_text(payload.get("error"))
    sys.stdout.write(json.dumps(payload, ensure_ascii=True) + "\n")
    sys.stdout.flush()


def handle_request(
    pipe,
    tokenizer,
    request: Dict[str, Any],
    default_max_tokens: int,
    max_prompt_len: int,
    build_time_ms: float,
    total_load_time_ms: float,
    log_fp: Optional[TextIO],
) -> Dict[str, Any]:
    request_id = request.get("request_id", "unknown")
    if request.get("command") == "shutdown":
        return {"request_id": request_id, "ok": True, "shutdown": True}

    prompt = request.get("prompt")
    if not isinstance(prompt, str) or not prompt.strip():
        return {"request_id": request_id, "ok": False, "error": "Missing prompt"}

    max_tokens = request.get("max_tokens", default_max_tokens)
    if not isinstance(max_tokens, int) or max_tokens <= 0:
        max_tokens = default_max_tokens

    temperature = request.get("temperature")
    if not isinstance(temperature, (int, float)) or temperature <= 0:
        temperature = TEMPERATURE
    top_p = request.get("top_p")
    if not isinstance(top_p, (int, float)) or top_p <= 0 or top_p > 1.0:
        top_p = TOP_P
    repetition_penalty = request.get("repetition_penalty")
    if not isinstance(repetition_penalty, (int, float)) or repetition_penalty <= 0:
        repetition_penalty = REPETITION_PENALTY

    prompt_tokens, token_count_method = count_tokens(tokenizer, prompt)
    max_length = prompt_tokens + max_tokens
    if token_count_method == "whitespace" and max_prompt_len:
        max_length = max(max_length, max_prompt_len)
    if max_length <= prompt_tokens:
        max_length = prompt_tokens + max(1, max_tokens)
    start_time = time.perf_counter()
    try:
        do_sample = temperature is not None or top_p is not None
        gen_kwargs = {
            "max_length": max_length,
            "do_sample": do_sample,
        }
        if temperature is not None:
            gen_kwargs["temperature"] = float(temperature)
        if top_p is not None:
            gen_kwargs["top_p"] = float(top_p)
        if repetition_penalty is not None:
            gen_kwargs["repetition_penalty"] = float(repetition_penalty)
        try:
            result = pipe.generate(prompt, **gen_kwargs)
        except TypeError as exc:
            log_line(log_fp, f"generate params unsupported, retrying without sampling args: {exc}")
            result = pipe.generate(
                prompt,
                max_length=max_length,
                do_sample=False,
            )
        elapsed_ms = (time.perf_counter() - start_time) * 1000.0
        text = "" if result is None else strip_think_tags(str(result))
        text = sanitize_text(text)
        generated_tokens, _ = count_tokens(tokenizer, text)
        total_tokens = prompt_tokens + generated_tokens
        tokens_per_sec = 0.0
        if elapsed_ms > 0.0:
            tokens_per_sec = (generated_tokens / elapsed_ms) * 1000.0
        metrics = {
            "build_time_ms": build_time_ms,
            "total_load_time_ms": total_load_time_ms,
            "gen_time_ms": elapsed_ms,
            "tokens_per_sec": tokens_per_sec,
            "prompt_tokens": prompt_tokens,
            "generated_tokens": generated_tokens,
            "total_tokens": total_tokens,
            "max_length": max_length,
            "max_new_tokens": max_tokens,
            "token_count_method": token_count_method,
            "npu": get_npu_metrics(),
            "use_api": False,
        }
        return {"request_id": request_id, "ok": True, "text": text, "metrics": metrics}
    except Exception as exc:
        log_line(log_fp, f"request exception: {exc}")
        log_line(log_fp, traceback.format_exc())
        return {"request_id": request_id, "ok": False, "error": str(exc)}


def try_import_openvino_genai(log_fp: Optional[TextIO]) -> Optional[object]:
    try:
        import openvino_genai as ov_genai
        return ov_genai
    except Exception as exc:
        log_line(log_fp, f"openvino_genai unavailable: {exc}")
        return None


def select_device(requested: str, log_fp: Optional[TextIO]) -> str:
    requested = (requested or "").strip().upper()
    if requested and requested != "AUTO":
        return requested
    try:
        import openvino as ov

        core = ov.Core()
        devices = [d.upper() for d in core.available_devices]
        for candidate in ["NPU", "GPU", "CPU"]:
            if candidate in devices:
                return candidate
    except Exception as exc:
        log_line(log_fp, f"device auto-detect failed: {exc}")
    return "CPU"


def run_openvino_self_test(args: argparse.Namespace, log_fp: Optional[TextIO]) -> int:
    if not args.model_dir:
        print("Missing --model-dir for OpenVINO self-test.", file=sys.stderr)
        return 1
    cache_dir = args.cache_dir or os.path.join(args.model_dir, ".ov_cache")
    device = select_device(args.device, log_fp)
    try:
        log_line(log_fp, "self-test: loading OpenVINO pipeline")
        pipe = load_pipeline(args.model_dir, device, cache_dir, args.max_prompt_len)
        tokenizer = build_tokenizer(args.model_dir)
        request = {
            "request_id": "self_test",
            "prompt": args.self_test_prompt,
            "max_tokens": min(args.max_tokens, 16),
            "temperature": 0.2,
            "top_p": 0.9,
            "repetition_penalty": 1.0,
        }
        response = handle_request(
            pipe,
            tokenizer,
            request,
            args.max_tokens,
            args.max_prompt_len,
            build_time_ms=0.0,
            total_load_time_ms=0.0,
            log_fp=log_fp,
        )
        if response.get("ok"):
            print("self-test ok")
            return 0
        error = response.get("error", "Unknown error")
        print(f"self-test failed: {error}", file=sys.stderr)
        return 1
    except Exception as exc:
        log_line(log_fp, f"self-test exception: {exc}")
        log_line(log_fp, traceback.format_exc())
        print(f"self-test failed: {exc}", file=sys.stderr)
        return 1


def ollama_generate(ollama_url: str, model: str, prompt: str, max_tokens: int, temperature: float) -> Dict[str, Any]:
    payload = {
        "model": model,
        "prompt": prompt,
        "stream": False,
        "options": {
            "temperature": float(temperature),
            "num_predict": int(max_tokens),
        },
    }
    url = ollama_url.rstrip("/") + "/api/generate"
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=600) as resp:
        return json.loads(resp.read().decode("utf-8"))


def ollama_unload(ollama_url: str, model: str, log_fp: Optional[TextIO]) -> None:
    payload = {
        "model": model,
        "keep_alive": 0,
        "stream": False,
    }
    url = ollama_url.rstrip("/") + "/api/generate"
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            body = resp.read().decode("utf-8", "replace")
            log_line(log_fp, f"ollama unload response: {body[:400]}")
    except Exception as exc:
        log_line(log_fp, f"ollama unload exception: {exc}")


def run_ollama_self_test(args: argparse.Namespace, log_fp: Optional[TextIO]) -> int:
    model = (args.ollama_model or "").strip()
    if not model:
        print("Ollama model is required for Ollama self-test.", file=sys.stderr)
        return 1
    try:
        response = ollama_generate(args.ollama_url, model, args.self_test_prompt, min(args.max_tokens, 16), 0.2)
        text = strip_think_tags(sanitize_text(response.get("response", "")))
        if not text.strip():
            print("self-test failed: empty Ollama response", file=sys.stderr)
            return 1
        print("self-test ok")
        return 0
    except Exception as exc:
        log_line(log_fp, f"ollama self-test exception: {exc}")
        log_line(log_fp, traceback.format_exc())
        print(f"self-test failed: {exc}", file=sys.stderr)
        return 1


def apply_provider_api_key_env(provider: str, api_key: str) -> None:
    provider_key = (provider or "").strip().lower()
    if not api_key:
        return
    if provider_key == "openai":
        os.environ["OPENAI_API_KEY"] = api_key


def extract_chat_stream_delta(chunk: Any) -> str:
    try:
        choices = getattr(chunk, "choices", None)
        if not choices:
            return ""
        delta = getattr(choices[0], "delta", None)
        if delta is None:
            return ""
        content = getattr(delta, "content", None)
        if isinstance(content, str):
            return content
        if isinstance(content, list):
            parts: List[str] = []
            for item in content:
                text = getattr(item, "text", None)
                if isinstance(text, str) and text:
                    parts.append(text)
            return "".join(parts)
    except Exception:
        return ""
    return ""


def stream_openai_chat_completion(
    provider: str,
    model: str,
    prompt: str,
    api_key: str,
    request_id: str,
    max_tokens: int,
    temperature: float,
    top_p: float,
) -> Tuple[str, float]:
    from openai import OpenAI

    client = OpenAI(api_key=api_key)
    accumulated = ""
    start_time = time.perf_counter()
    stream = client.chat.completions.create(
        model=model,
        messages=[{"role": "user", "content": prompt}],
        stream=True,
        max_completion_tokens=int(max_tokens),
        temperature=float(temperature),
        top_p=float(top_p),
    )
    for chunk in stream:
        delta = sanitize_text(extract_chat_stream_delta(chunk))
        if not delta:
            continue
        accumulated += delta
        write_response({"request_id": request_id, "event": "partial", "text": accumulated})
    elapsed_ms = (time.perf_counter() - start_time) * 1000.0
    return strip_think_tags(accumulated), elapsed_ms


def run_api_self_test(args: argparse.Namespace, log_fp: Optional[TextIO]) -> int:
    try:
        from any_llm import completion
    except Exception as exc:
        log_line(log_fp, f"any-llm import failed: {exc}")
        print(f"any-llm import failed: {exc}", file=sys.stderr)
        return 1

    provider = (args.api_provider or "").strip()
    model = (args.api_model or "").strip()
    if not provider or not model:
        print("API provider and model are required for API self-test.", file=sys.stderr)
        return 1

    api_key = ""
    if args.api_key_env:
        api_key = os.environ.get(args.api_key_env, "")
        if args.use_api and not api_key:
            print(f"API key env var not set: {args.api_key_env}", file=sys.stderr)
            return 1

    apply_provider_api_key_env(provider, api_key)

    try:
        response = completion(
            model=model,
            provider=provider,
            messages=[{"role": "user", "content": args.self_test_prompt}],
            api_key=api_key,
        )
        text = strip_think_tags(extract_completion_text(response))
        if not text.strip():
            print("self-test failed: empty API response", file=sys.stderr)
            return 1
        print("self-test ok")
        return 0
    except Exception as exc:
        log_line(log_fp, f"api self-test exception: {exc}")
        log_line(log_fp, traceback.format_exc())
        print(f"self-test failed: {exc}", file=sys.stderr)
        return 1


def main() -> int:
    args = parse_args()
    log_fp = setup_logger(args.log_file)
    if args.dry_run:
        log_line(log_fp, "dry-run ok")
        print("dry-run ok")
        return 0
    backend = (args.backend or "openvino").strip().lower()
    if args.use_api:
        backend = "api"

    if backend == "api":
        if args.self_test:
            return run_api_self_test(args, log_fp)
        return run_api_mode(args, log_fp)

    if backend == "ollama":
        if args.self_test:
            return run_ollama_self_test(args, log_fp)
        return run_ollama_mode(args, log_fp)

    if not args.model_dir:
        print("Missing --model-dir for OpenVINO mode.", file=sys.stderr)
        return 1
    if args.self_test:
        return run_openvino_self_test(args, log_fp)

    start_time = time.perf_counter()
    if args.force_npu:
        try:
            log_line(log_fp, "checking NPU availability")
            ensure_npu_only()
        except Exception as exc:
            log_line(log_fp, f"NPU check failed: {exc}")
            print(f"NPU check failed: {exc}", file=sys.stderr)
            return 1

    try:
        log_line(log_fp, "loading pipeline")
        build_start = time.perf_counter()
        cache_dir = args.cache_dir or os.path.join(args.model_dir, ".ov_cache")
        device = select_device(args.device, log_fp)
        pipe = load_pipeline(args.model_dir, device, cache_dir, args.max_prompt_len)
        build_time_ms = (time.perf_counter() - build_start) * 1000.0
        total_load_time_ms = (time.perf_counter() - start_time) * 1000.0
        log_line(log_fp, f"pipeline loaded (build {build_time_ms:.1f} ms, total {total_load_time_ms:.1f} ms)")
        tokenizer = build_tokenizer(args.model_dir)
        log_line(log_fp, "tokenizer ready" if tokenizer else "tokenizer unavailable")
    except Exception as exc:
        log_line(log_fp, f"Failed to load pipeline: {exc}")
        log_line(log_fp, traceback.format_exc())
        print(f"Failed to load pipeline: {exc}", file=sys.stderr)
        return 1

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            request = read_request(line)
        except Exception as exc:
            log_line(log_fp, f"invalid request: {exc}")
            write_response({"request_id": "unknown", "ok": False, "error": str(exc)})
            continue

        response = handle_request(
            pipe,
            tokenizer,
            request,
            args.max_tokens,
            args.max_prompt_len,
            build_time_ms,
            total_load_time_ms,
            log_fp,
        )
        if not response.get("ok"):
            log_line(log_fp, f"request failed: {response.get('error', '')}")
        else:
            text = response.get("text", "")
            if isinstance(text, str) and text:
                snippet = sanitize_text(text)
                snippet = snippet if len(snippet) <= 4000 else snippet[:4000] + "...[truncated]"
                log_line(log_fp, f"response raw: {snippet}")
        write_response(response)
        if response.get("shutdown"):
            return 0

    return 0


def extract_message_content_text(content: Any) -> str:
    if isinstance(content, str):
        return content
    if isinstance(content, list):
        parts: List[str] = []
        for item in content:
            if isinstance(item, str):
                parts.append(item)
            elif isinstance(item, dict):
                text = item.get("text")
                if isinstance(text, str) and text:
                    parts.append(text)
                elif item.get("type") == "output_text":
                    maybe_text = item.get("text")
                    if isinstance(maybe_text, str) and maybe_text:
                        parts.append(maybe_text)
            else:
                maybe_text = getattr(item, "text", None)
                if isinstance(maybe_text, str) and maybe_text:
                    parts.append(maybe_text)
        return "".join(parts)
    if isinstance(content, dict):
        text = content.get("text")
        if isinstance(text, str):
            return text
    maybe_text = getattr(content, "text", None)
    if isinstance(maybe_text, str):
        return maybe_text
    return ""


def extract_completion_text(response: Any) -> str:
    if response is None:
        return ""
    if isinstance(response, dict):
        choices = response.get("choices")
        if isinstance(choices, list) and choices:
            message = choices[0].get("message", {})
            text = extract_message_content_text(message.get("content"))
            if text:
                return text
    choices = getattr(response, "choices", None)
    if isinstance(choices, list) and choices:
        message = getattr(choices[0], "message", None)
        text = extract_message_content_text(getattr(message, "content", None))
        if text:
            return text
    output_text = getattr(response, "output_text", None)
    if isinstance(output_text, str) and output_text:
        return output_text
    return str(response)


def run_ollama_mode(args: argparse.Namespace, log_fp: Optional[TextIO]) -> int:
    model = (args.ollama_model or "").strip()
    if not model:
        print("Ollama model is required for Ollama mode.", file=sys.stderr)
        return 1

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            request = read_request(line)
        except Exception as exc:
            log_line(log_fp, f"invalid request: {exc}")
            write_response({"request_id": "unknown", "ok": False, "error": str(exc)})
            continue

        request_id = request.get("request_id", "unknown")
        if request.get("command") == "shutdown":
            ollama_unload(args.ollama_url, model, log_fp)
            write_response({"request_id": request_id, "ok": True, "shutdown": True})
            return 0

        prompt = request.get("prompt")
        if not isinstance(prompt, str) or not prompt.strip():
            write_response({"request_id": request_id, "ok": False, "error": "Missing prompt"})
            continue

        max_tokens = request.get("max_tokens")
        if not isinstance(max_tokens, int) or max_tokens <= 0:
            max_tokens = 128
        temperature = request.get("temperature")
        if not isinstance(temperature, (int, float)) or temperature <= 0:
            temperature = TEMPERATURE

        start_time = time.perf_counter()
        try:
            response = ollama_generate(args.ollama_url, model, prompt, max_tokens, temperature)
            elapsed_ms = (time.perf_counter() - start_time) * 1000.0
            text = strip_think_tags(sanitize_text(response.get("response", "")))
            payload = {"request_id": request_id, "ok": True, "text": text}
            payload["metrics"] = {
                "gen_time_ms": elapsed_ms,
                "max_new_tokens": max_tokens,
                "use_api": False,
                "backend": "ollama",
                "prompt_eval_count": response.get("prompt_eval_count"),
                "eval_count": response.get("eval_count"),
                "eval_duration": response.get("eval_duration"),
            }
            write_response(payload)
        except Exception as exc:
            log_line(log_fp, f"ollama request exception: {exc}")
            log_line(log_fp, traceback.format_exc())
            write_response({"request_id": request_id, "ok": False, "error": str(exc)})
    return 0


def run_api_mode(args: argparse.Namespace, log_fp: Optional[TextIO]) -> int:
    try:
        from any_llm import completion
    except Exception as exc:
        log_line(log_fp, f"any-llm import failed: {exc}")
        print(f"any-llm import failed: {exc}", file=sys.stderr)
        return 1

    provider = (args.api_provider or "").strip()
    model = (args.api_model or "").strip()
    if not provider or not model:
        print("API provider and model are required for API mode.", file=sys.stderr)
        return 1

    api_key = ""
    if args.api_key_env:
        api_key = os.environ.get(args.api_key_env, "")

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            request = read_request(line)
        except Exception as exc:
            log_line(log_fp, f"invalid request: {exc}")
            write_response({"request_id": "unknown", "ok": False, "error": str(exc)})
            continue

        request_id = request.get("request_id", "unknown")
        if request.get("command") == "shutdown":
            ollama_unload(args.ollama_url, model, log_fp)
            write_response({"request_id": request_id, "ok": True, "shutdown": True})
            return 0

        prompt = request.get("prompt")
        if not isinstance(prompt, str) or not prompt.strip():
            write_response({"request_id": request_id, "ok": False, "error": "Missing prompt"})
            continue

        max_tokens = request.get("max_tokens")
        if not isinstance(max_tokens, int) or max_tokens <= 0:
            max_tokens = args.max_tokens if args.max_tokens > 0 else 128
        temperature = request.get("temperature")
        if not isinstance(temperature, (int, float)) or temperature <= 0:
            temperature = TEMPERATURE
        top_p = request.get("top_p")
        if not isinstance(top_p, (int, float)) or top_p <= 0 or top_p > 1.0:
            top_p = TOP_P
        wants_stream = bool(request.get("stream"))

        start_time = time.perf_counter()
        apply_provider_api_key_env(provider, api_key)
        try:
            if wants_stream and provider.lower() == "openai":
                text, elapsed_ms = stream_openai_chat_completion(
                    provider=provider,
                    model=model,
                    prompt=prompt,
                    api_key=api_key,
                    request_id=request_id,
                    max_tokens=max_tokens,
                    temperature=temperature,
                    top_p=top_p,
                )
                text = sanitize_text(text)
                payload = {"request_id": request_id, "ok": True, "text": text}
                payload["metrics"] = {
                    "gen_time_ms": elapsed_ms,
                    "use_api": True,
                    "streamed": True,
                }
                write_response(payload)
                continue

            completion_kwargs: Dict[str, Any] = {
                "model": model,
                "provider": provider,
                "messages": [{"role": "user", "content": prompt}],
                "api_key": api_key,
            }
            response = completion(
                **completion_kwargs,
            )
            elapsed_ms = (time.perf_counter() - start_time) * 1000.0
            text = strip_think_tags(extract_completion_text(response))
            text = sanitize_text(text)
            payload = {"request_id": request_id, "ok": True, "text": text}
            payload["metrics"] = {
                "gen_time_ms": elapsed_ms,
                "use_api": True,
            }
            write_response(payload)
        except Exception as exc:
            log_line(log_fp, f"api request exception: {exc}")
            log_line(log_fp, traceback.format_exc())
            write_response({"request_id": request_id, "ok": False, "error": str(exc)})
    return 0


if __name__ == "__main__":
    sys.exit(main())
