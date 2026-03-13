#!/usr/bin/env python3
import argparse
import json
import os
import re
import urllib.request
from typing import Any, Dict, List, Optional, Tuple


DEFAULT_MODEL_DIR = ""


def strip_think_tags(text: str) -> str:
    if "</think>" in text:
        return text.rsplit("</think>", 1)[-1]
    return text


def load_pipeline(model_dir: str, device: str, max_prompt_len: int):
    import openvino_genai as ov_genai

    try:
        return ov_genai.LLMPipeline(
            model_dir,
            device,
            MAX_PROMPT_LEN=max_prompt_len,
            ENABLE_CPU_FALLBACK="NO",
        )
    except TypeError:
        config = {"MAX_PROMPT_LEN": max_prompt_len}
        return ov_genai.LLMPipeline(
            model_dir,
            device,
            config,
            ENABLE_CPU_FALLBACK="NO",
        )


def ollama_generate(ollama_url: str, model: str, prompt: str,
                    max_tokens: int, temperature: float) -> Dict[str, Any]:
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


def extract_traits_from_condition(cond: Dict[str, Any], out: List[str]) -> None:
    if not isinstance(cond, dict):
        return
    trait = cond.get("npc_has_trait")
    if isinstance(trait, str):
        out.append(trait)
    for key in ("and", "or"):
        block = cond.get(key)
        if isinstance(block, list):
            for entry in block:
                extract_traits_from_condition(entry, out)


def build_trait_to_topic(toc_path: str) -> Dict[str, str]:
    with open(toc_path, "r", encoding="utf-8") as handle:
        data = json.load(handle)
    mapping: Dict[str, str] = {}
    if not isinstance(data, list):
        return mapping
    for entry in data:
        if not isinstance(entry, dict):
            continue
        if entry.get("type") != "talk_topic":
            continue
        responses = entry.get("responses")
        if not isinstance(responses, list):
            continue
        for resp in responses:
            if not isinstance(resp, dict):
                continue
            topic = resp.get("topic")
            cond = resp.get("condition")
            if not isinstance(topic, str) or not isinstance(cond, dict):
                continue
            traits: List[str] = []
            extract_traits_from_condition(cond, traits)
            for trait in traits:
                if trait not in mapping:
                    mapping[trait] = topic
    return mapping


def read_talk_topic_fields(obj: Dict[str, Any]) -> Tuple[List[str], List[str]]:
    dynamic_lines: List[str] = []
    responses: List[str] = []
    dynamic_line = obj.get("dynamic_line")
    if isinstance(dynamic_line, str):
        dynamic_lines.append(dynamic_line)
    elif isinstance(dynamic_line, dict):
        gendered = dynamic_line.get("gendered_line")
        if isinstance(gendered, str):
            dynamic_lines.append(gendered)
        lines = dynamic_line.get("lines")
        if isinstance(lines, list):
            for line in lines:
                if isinstance(line, str):
                    dynamic_lines.append(line)
    resp_list = obj.get("responses")
    if isinstance(resp_list, list):
        for resp in resp_list:
            if isinstance(resp, dict):
                text = resp.get("text")
                if isinstance(text, str):
                    responses.append(text)
    return dynamic_lines, responses


def index_talk_topics(background_dir: str) -> Dict[str, Dict[str, Any]]:
    index: Dict[str, Dict[str, Any]] = {}
    file_to_lines: Dict[str, Dict[str, List[str]]] = {}
    for root, _dirs, files in os.walk(background_dir):
        for name in files:
            if not name.endswith(".json"):
                continue
            path = os.path.join(root, name)
            try:
                with open(path, "r", encoding="utf-8") as handle:
                    data = json.load(handle)
            except Exception:
                continue
            if not isinstance(data, list):
                continue
            file_entry = file_to_lines.setdefault(path, {"dynamic_lines": [], "responses": []})
            for entry in data:
                if not isinstance(entry, dict):
                    continue
                if entry.get("type") != "talk_topic":
                    continue
                topic_id = entry.get("id")
                if not isinstance(topic_id, str):
                    continue
                dyn_lines, responses = read_talk_topic_fields(entry)
                if dyn_lines:
                    file_entry["dynamic_lines"].extend(dyn_lines)
                if responses:
                    file_entry["responses"].extend(responses)
                index[topic_id] = {
                    "source": path,
                    "dynamic_lines": dyn_lines,
                    "responses": responses,
                }
    for topic_id, info in index.items():
        file_info = file_to_lines.get(info["source"], {"dynamic_lines": [], "responses": []})
        info["file_dynamic_lines"] = file_info["dynamic_lines"]
        info["file_responses"] = file_info["responses"]
    return index


def build_prompt(dynamic_lines: List[str], responses: List[str], strict: bool = False) -> str:
    sections: List[str] = []
    if dynamic_lines:
        sections.append("NPC lines:")
        sections.extend([f"- {line}" for line in dynamic_lines])
    if responses:
        sections.append("Player responses:")
        sections.extend([f"- {line}" for line in responses])
    story_text = "\n".join(sections).strip()
    if strict:
        system_block = "\n".join(
            [
                "<System>",
                "You must analyze the personality of this fictional character.",
                "Return exactly one line in this exact format:",
                "word1, word2, word3, word4, word5 | quoted sentence",
                "Rules:",
                "- The first 5 items must each be a single descriptive word.",
                "- No numbering.",
                "- No labels.",
                "- No explanation.",
                "- Keep contractions intact inside the quote.",
                "- The quote must be copied from the NPC lines.",
                "- Do not include <think> tags or reasoning.",
                "</System>",
            ]
        )
    else:
        system_block = "\n".join(
            [
                "<System>",
                "You must analyze the personality of this fictional character.",
                "Return exactly two sections, separated by ' | '.",
                "List 1: 5 distinct personality descriptors (single words), comma-separated.",
                "List 2: Repeat a notable sentence that the character said.",
                "Do not add labels or extra text.",
                "Do not include <think> tags or reasoning.",
                "</System>",
            ]
        )
    if story_text:
        return f"{story_text}\n\n{system_block}\nWORDS: "
    return f"{system_block}\nWORDS: "


def normalize_output(text: str) -> str:
    cleaned = strip_think_tags(text).strip().replace("\r", "").replace("\n", " ")
    while "  " in cleaned:
        cleaned = cleaned.replace("  ", " ")
    return cleaned


def words_from_output(text: str) -> List[str]:
    return re.findall(r"[A-Za-z0-9_-]+", text)


def quoted_words(text: str) -> List[str]:
    return re.findall(r"\"([A-Za-z0-9_-]+)\"", text)


def stopword_set() -> set:
    return {
        "the", "and", "that", "with", "this", "they", "them", "their", "then", "from", "have", "has",
        "had", "for", "are", "was", "were", "but", "not", "you", "your", "she", "his", "her", "him",
        "about", "into", "just", "like", "first", "next", "words", "word", "output", "exactly",
        "personality", "expressive", "mannerisms", "descriptors", "dialogue", "npc", "character",
        "think", "okay", "lets", "tackle", "analyze", "understand", "user", "wants", "need", "should",
        "theyre", "dont", "didnt", "its", "im", "ive", "we", "our", "ours", "me", "before", "started",
        "crappy", "job", "flipping", "burgers", "sambal", "grille", "losing", "come", "mind", "let",
        "start", "starts", "talking", "talk", "mention", "mentions", "phrase", "phrases", "use", "uses",
        "focus", "based", "now", "next", "parents", "parent", "mom", "dad", "family",
        "telling", "told", "love", "loved",
    }


def keyword_fallback(text: str, limit: int, exclude: Optional[set] = None) -> List[str]:
    stopwords = stopword_set()
    exclude = exclude or set()
    candidates: List[str] = []
    for word in words_from_output(text):
        lowered = word.lower()
        if lowered in stopwords:
            continue
        if len(lowered) < 3:
            continue
        if lowered in exclude:
            continue
        candidates.append(word)
        if len(candidates) >= limit:
            break
    return candidates


def words_from_phrase_list(text: str) -> List[str]:
    words: List[str] = []
    for match in re.findall(r"[Ww]ords like ([^\\.]+)", text):
        words.extend(words_from_output(match))
    return words


def summarize_to_lines(text: str) -> Tuple[str, str]:
    if "WORDS:" in text:
        text = text.rsplit("WORDS:", 1)[-1].strip()
    if "|" in text:
        left, right = text.split("|", 1)
        left_items = [item.strip() for item in left.split(",") if item.strip()]
        if len(left_items) >= 5 and right.strip():
            return ", ".join(left_items[:5]), right.strip()
    line_parts = [line.strip() for line in text.replace("\r", "").split("\n") if line.strip()]
    if len(line_parts) >= 2:
        left_items = [item.strip() for item in line_parts[0].split(",") if item.strip()]
        if len(left_items) >= 5 and line_parts[1]:
            return ", ".join(left_items[:5]), line_parts[1]
    words = words_from_output(text)
    if len(words) >= 10:
        background = ", ".join(words[:5])
        expression = ", ".join(words[5:10])
        return background, expression
    if words:
        background = ", ".join(words)
        return background, ""
    return text.strip(), ""


def looks_like_bad_summary(background: str, expression: str) -> bool:
    bg_items = [item.strip() for item in background.split(",") if item.strip()]
    if len(bg_items) < 5 or not expression.strip():
        return True
    weak_words = {
        "i", "we", "you", "he", "she", "it", "they", "re", "ll", "ve", "d",
        "still", "guess", "just", "like", "really", "very", "sort", "kind",
    }
    for item in bg_items[:5]:
        lowered = item.lower()
        if any(ch.isdigit() for ch in lowered):
            return True
        if lowered in weak_words:
            return True
        if len(lowered) < 3:
            return True
    if len(expression.split()) < 3:
        return True
    return False


def is_valid_words(text: str) -> bool:
    if "WORDS:" in text:
        text = text.rsplit("WORDS:", 1)[-1].strip()
    if "|" in text:
        left, right = text.split("|", 1)
        left_items = [item.strip() for item in left.split(",") if item.strip()]
        if len(left_items) >= 5 and bool(right.strip()):
            return not looks_like_bad_summary(", ".join(left_items[:5]), right.strip())
        return False
    line_parts = [line.strip() for line in text.replace("\r", "").split("\n") if line.strip()]
    if len(line_parts) >= 2:
        left_items = [item.strip() for item in line_parts[0].split(",") if item.strip()]
        if len(left_items) >= 5 and bool(line_parts[1]):
            return not looks_like_bad_summary(", ".join(left_items[:5]), line_parts[1])
    words = words_from_output(text)
    if len(words) >= 10:
        return not looks_like_bad_summary(", ".join(words[:5]), ", ".join(words[5:10]))
    return False


def load_existing_entries(path: str) -> Dict[str, Dict[str, Any]]:
    if not os.path.exists(path):
        return {}
    entries: Dict[str, Dict[str, Any]] = {}
    try:
        with open(path, "r", encoding="utf-8") as handle:
            for raw in handle:
                line = raw.strip()
                if not line or line.startswith("#"):
                    continue
                parts = [part.strip() for part in line.split("|")]
                if len(parts) < 3:
                    continue
                entry_id, background, expression = parts[0], parts[1], parts[2]
                source_tag = parts[3] if len(parts) > 3 else ""
                entries[entry_id] = {
                    "id": entry_id,
                    "your_background": background,
                    "your_expression": expression,
                    "source_tag": source_tag,
                }
    except Exception:
        return {}
    return entries


def write_entries(path: str, entries: Dict[str, Dict[str, Any]]) -> None:
    ordered = [entries[key] for key in sorted(entries.keys())]
    with open(path, "w", encoding="utf-8") as handle:
        for entry in ordered:
            handle.write(
                f"{entry['id']}|{entry.get('your_background', '')}|"
                f"{entry.get('your_expression', '')}|{entry.get('source_tag', '')}\n"
            )


def build_local_source_tag(backend: str, model_ref: str, source_base: str) -> str:
    model_name = os.path.basename(os.path.normpath(model_ref)) if model_ref else "unknown-model"
    safe_backend = re.sub(r"[^A-Za-z0-9._-]+", "_", backend).strip("_") or "unknown-backend"
    safe_model_name = re.sub(r"[^A-Za-z0-9._:-]+", "_", model_name).strip("_") or "unknown-model"
    return f"local:{safe_backend}:{safe_model_name}:{source_base}"


def parse_args() -> argparse.Namespace:
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    parser = argparse.ArgumentParser(description="Generate short background summaries.")
    parser.add_argument(
        "--data-dir",
        default=os.path.join(repo_root, "data", "json", "npcs", "Backgrounds"),
        help="Backgrounds directory.",
    )
    parser.add_argument(
        "--toc-path",
        default=os.path.join(repo_root, "data", "json", "npcs", "Backgrounds",
                             "backgrounds_table_of_contents.json"),
        help="Table of contents JSON path.",
    )
    parser.add_argument(
        "--out-dir",
        default=os.path.join(repo_root, "data", "json", "npcs", "Backgrounds", "Summaries_short"),
        help="Output directory for summaries.",
    )
    parser.add_argument(
        "--backend",
        default=os.environ.get("LLM_SUMMARY_BACKEND", "ollama"),
        help="Summary backend to use (ollama or openvino).",
    )
    parser.add_argument(
        "--model-dir",
        default=os.environ.get("LLM_SUMMARY_MODEL_DIR", DEFAULT_MODEL_DIR),
        help="Path to local OpenVINO model directory.",
    )
    parser.add_argument(
        "--device",
        default=os.environ.get("LLM_SUMMARY_DEVICE", "NPU"),
        help="OpenVINO device to target.",
    )
    parser.add_argument(
        "--ollama-url",
        default=os.environ.get("LLM_SUMMARY_OLLAMA_URL", "http://127.0.0.1:11434"),
        help="Local Ollama server URL.",
    )
    parser.add_argument(
        "--ollama-model",
        default=os.environ.get("LLM_SUMMARY_OLLAMA_MODEL", "mistral"),
        help="Local Ollama model name.",
    )
    parser.add_argument(
        "--max-prompt-len",
        type=int,
        default=4096,
        help="Maximum prompt length for the pipeline.",
    )
    parser.add_argument(
        "--max-tokens",
        type=int,
        default=20000,
        help="Maximum tokens to generate.",
    )
    parser.add_argument(
        "--temperature",
        type=float,
        default=0.4,
        help="Sampling temperature.",
    )
    parser.add_argument(
        "--top-p",
        type=float,
        default=0.9,
        help="Top-p sampling value.",
    )
    parser.add_argument(
        "--repetition-penalty",
        type=float,
        default=1.05,
        help="Repetition penalty.",
    )
    parser.add_argument(
        "--only-topic",
        default="",
        help="If set, generate only this talk_topic id.",
    )
    parser.add_argument(
        "--include-responses",
        action="store_true",
        help="Include player responses in the prompt.",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Regenerate even if an entry already exists.",
    )
    parser.add_argument(
        "--retry-invalid",
        type=int,
        default=0,
        help="Retry generation when output is invalid.",
    )
    parser.add_argument(
        "--debug-invalid",
        action="store_true",
        help="Print raw output when generation is invalid.",
    )
    parser.add_argument(
        "--debug-io",
        action="store_true",
        help="Print raw prompt and raw model output.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    backend = (args.backend or "ollama").strip().lower()
    if backend not in {"ollama", "openvino"}:
        print(f"Unsupported summary backend: {backend}. Aborting.")
        return 2
    if backend == "openvino":
        if not args.model_dir or not os.path.isdir(args.model_dir):
            print(f"LLM summary model dir not found: {args.model_dir}. Aborting.")
            return 2
        try:
            import openvino_genai  # noqa: F401
        except Exception as exc:
            print(f"LLM summary dependency missing: {exc}. Aborting.")
            return 2
    else:
        if not args.ollama_model.strip():
            print("LLM summary Ollama model is required. Aborting.")
            return 2

    try:
        os.makedirs(args.out_dir, exist_ok=True)
    except Exception as exc:
        print(f"Failed to create summary dir: {exc}. Aborting.")
        return 2

    trait_to_topic = build_trait_to_topic(args.toc_path)
    if not trait_to_topic:
        print("No background traits found; nothing to summarize.")
        return 0

    topics = sorted(set(trait_to_topic.values()))
    if args.only_topic:
        if args.only_topic in topics:
            topics = [args.only_topic]
        else:
            print(f"Unknown topic: {args.only_topic}")
            return 0
    topic_index = index_talk_topics(args.data_dir)

    pipe = None
    if backend == "openvino":
        try:
            pipe = load_pipeline(args.model_dir, args.device, args.max_prompt_len)
        except Exception as exc:
            print(f"Failed to load LLM pipeline: {exc}. Aborting.")
            return 2

    generated_any = False
    for topic_id in topics:
        info = topic_index.get(topic_id)
        if not info:
            print(f"Missing talk_topic source for {topic_id}.")
            continue
        source_path = info["source"]
        source_base = os.path.splitext(os.path.basename(source_path))[0]
        out_path = os.path.join(args.out_dir, f"{source_base}_summary_short.txt")
        existing = load_existing_entries(out_path)
        if topic_id in existing and not args.force:
            continue
        existing.pop(topic_id, None)
        prompt_lines = info.get("file_dynamic_lines", info["dynamic_lines"])
        prompt_responses: List[str] = []
        if args.include_responses:
            prompt_responses = info.get("file_responses", info["responses"])
        prompt_modes = [False, True]
        attempts = max(1, args.retry_invalid + 1)
        summary = ""
        background_line = ""
        expression_line = ""
        for strict_prompt in prompt_modes:
            prompt = build_prompt(prompt_lines, prompt_responses, strict=strict_prompt)
            if args.debug_io:
                mode_name = "strict" if strict_prompt else "default"
                print(f"Prompt for {topic_id} ({mode_name}):\n{prompt}\n")
            prompt_tokens = max(1, len(prompt.split()))
            max_length = min(args.max_prompt_len, prompt_tokens + args.max_tokens)
            for attempt in range(attempts):
                if backend == "openvino":
                    try:
                        result = pipe.generate(
                            prompt,
                            max_length=max_length,
                            do_sample=True,
                            temperature=args.temperature,
                            top_p=args.top_p,
                            repetition_penalty=args.repetition_penalty,
                        )
                    except TypeError:
                        result = pipe.generate(
                            prompt,
                            max_length=max_length,
                            do_sample=False,
                        )
                    raw_text = "" if result is None else str(result)
                else:
                    response = ollama_generate(
                        args.ollama_url,
                        args.ollama_model,
                        prompt,
                        args.max_tokens,
                        args.temperature,
                    )
                    raw_text = str(response.get("response", ""))
                if args.debug_io:
                    print(f"Raw output for {topic_id}:\n{raw_text}\n")
                summary = ""
                if "</think>" in raw_text:
                    summary = normalize_output(raw_text)
                elif "WORDS:" in raw_text:
                    summary = normalize_output(raw_text.rsplit("WORDS:", 1)[-1])
                elif "<think>" in raw_text and "</think>" not in raw_text:
                    stopwords = stopword_set()
                    quoted = quoted_words(raw_text)
                    phrase_words = words_from_phrase_list(raw_text)
                    selected: List[str] = []
                    for word in quoted + phrase_words:
                        lowered = word.lower()
                        if lowered in stopwords or len(lowered) < 3:
                            continue
                        if word not in selected:
                            selected.append(word)
                    raw_lower = raw_text.lower()
                    expressions: List[str] = []
                    expressive_candidates = [
                        "raw", "emotional", "reflective", "regretful", "vulnerable", "blunt",
                        "casual", "weary", "nostalgic", "guilt-ridden", "introspective", "urgent",
                        "trauma", "guarded", "broken",
                    ]
                    used = {word.lower() for word in selected}
                    for cand in expressive_candidates:
                        if cand in raw_lower and cand not in used:
                            expressions.append(cand)
                            used.add(cand)
                            if len(expressions) >= 5:
                                break
                    if len(selected) < 5:
                        selected.extend(keyword_fallback(raw_text, 5 - len(selected), used))
                        used.update(word.lower() for word in selected)
                    if len(expressions) < 5:
                        expressions.extend(keyword_fallback(raw_text, 5 - len(expressions), used))
                    if len(selected) >= 5 and len(expressions) >= 5:
                        summary = " ".join(selected[:5] + expressions[:5])
                    else:
                        if args.debug_invalid:
                            print(f"Invalid summary output for {topic_id}: {raw_text!r}")
                        if attempt + 1 < attempts:
                            continue
                else:
                    summary = raw_text.strip()
                if is_valid_words(summary):
                    background_line, expression_line = summarize_to_lines(summary)
                    break
                if args.debug_invalid:
                    print(f"Invalid summary output for {topic_id}: {raw_text!r}")
                if attempt + 1 < attempts:
                    continue
            if background_line and expression_line:
                break
        if not background_line and not expression_line:
            print(f"Invalid summary for {topic_id}; skipping.")
            continue
        model_ref = args.model_dir if backend == "openvino" else args.ollama_model
        source_tag = build_local_source_tag(
            backend,
            model_ref,
            re.sub(r"_\\d+$", "", source_base)
        )
        entry = {
            "id": topic_id,
            "your_background": background_line,
            "your_expression": expression_line,
            "source_tag": source_tag,
        }
        existing[topic_id] = entry
        write_entries(out_path, existing)
        generated_any = True
        print(f"Generated summary for {topic_id} -> {out_path}")

    if not generated_any:
        print("No new summaries needed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
