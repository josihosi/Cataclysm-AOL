#!/usr/bin/env python3
import argparse
import json
import os
import re
import shutil
import urllib.request
from typing import Any, Dict, List, Optional, Tuple


DEFAULT_MODEL_DIR = ""
PROMPT_DIRNAME = "llm_prompts"
DEFAULT_SUMMARIZER_PROMPT_FILENAME = "background_summarizer_prompt.txt"
STRICT_SUMMARIZER_PROMPT_FILENAME = "background_summarizer_strict_prompt.txt"


def read_text(path: str) -> str:
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        return handle.read()


def repo_root() -> str:
    return os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def prompt_override_dir() -> str:
    return os.path.join(repo_root(), "config", PROMPT_DIRNAME)


def bundled_prompt_dir() -> str:
    return os.path.join(repo_root(), "data", PROMPT_DIRNAME)


def render_prompt_template(template: str, replacements: Dict[str, str]) -> str:
    rendered = template
    for key, value in replacements.items():
        rendered = rendered.replace(key, value)
    return rendered


def has_required_tokens(template: str, required_tokens: List[str]) -> bool:
    if not template.strip():
        return False
    return all(token in template for token in required_tokens)


def seed_prompt_override_file(filename: str) -> None:
    os.makedirs(prompt_override_dir(), exist_ok=True)
    source = os.path.join(bundled_prompt_dir(), filename)
    dest = os.path.join(prompt_override_dir(), filename)
    if os.path.exists(source) and not os.path.exists(dest):
        shutil.copyfile(source, dest)


def load_prompt_template(filename: str, fallback_template: str, required_tokens: List[str]) -> str:
    try:
        seed_prompt_override_file(filename)
    except Exception:
        pass
    for path in (
        os.path.join(prompt_override_dir(), filename),
        os.path.join(bundled_prompt_dir(), filename),
    ):
        if not os.path.exists(path):
            continue
        try:
            template = read_text(path)
        except Exception:
            continue
        if has_required_tokens(template, required_tokens):
            return template
    return fallback_template


def default_summarizer_prompt_template(strict: bool = False) -> str:
    if strict:
        return (
            "{{story_text_block}}<System>\n"
            "You must analyze the personality of this fictional character.\n"
            "Return exactly one line in this exact format:\n"
            "word1, word2, word3, word4, word5 | quoted sentence\n"
            "Rules:\n"
            "- The first 5 items must each be a single descriptive word.\n"
            "- No numbering.\n"
            "- No labels.\n"
            "- No explanation.\n"
            "- Keep contractions intact inside the quote.\n"
            "- The quote must be copied from the NPC lines.\n"
            "- Do not include <think> tags or reasoning.\n"
            "</System>\n"
            "WORDS: "
        )
    return (
        "{{story_text_block}}<System>\n"
        "You must analyze the personality of this fictional character.\n"
        "Return exactly two sections, separated by ' | '.\n"
        "List 1: 5 distinct personality descriptors (single words), comma-separated.\n"
        "List 2: Repeat a notable sentence that the character said.\n"
        "Do not add labels or extra text.\n"
        "Do not include <think> tags or reasoning.\n"
        "</System>\n"
        "WORDS: "
    )


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
    story_text_block = f"{story_text}\n\n" if story_text else ""
    filename = STRICT_SUMMARIZER_PROMPT_FILENAME if strict else DEFAULT_SUMMARIZER_PROMPT_FILENAME
    template = load_prompt_template(
        filename,
        default_summarizer_prompt_template(strict=strict),
        ["{{story_text_block}}"],
    )
    return render_prompt_template(template, {"{{story_text_block}}": story_text_block})


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


def resolve_path(base_dir: str, raw_path: str) -> str:
    if not raw_path:
        return os.path.normpath(base_dir)
    if os.path.isabs(raw_path):
        return os.path.normpath(raw_path)
    return os.path.normpath(os.path.join(base_dir, raw_path))


def normalize_selector(selector: str) -> str:
    text = str(selector or "").replace("\r", "").replace("\n", " ")
    return " ".join(text.split())


def safe_entry_key(text: str) -> str:
    lowered = re.sub(r"[^A-Za-z0-9._:-]+", "_", text.strip().lower()).strip("_")
    return lowered or "unnamed"


def load_summary_registry(path: str) -> Dict[str, Any]:
    with open(path, "r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValueError(f"Registry must be a JSON object: {path}")
    entries_data = data.get("entries")
    if not isinstance(entries_data, list):
        raise ValueError(f"Registry is missing an entries array: {path}")

    registry_dir = os.path.dirname(path)
    path_root = resolve_path(registry_dir, str(data.get("path_root", ".")))
    generated_output = resolve_path(
        path_root,
        str(data.get(
            "generated_output",
            os.path.join("npcs", "Backgrounds", "Summaries_extra", "generated_named_npcs.txt"),
        )),
    )

    entries: List[Dict[str, Any]] = []
    for raw_entry in entries_data:
        if not isinstance(raw_entry, dict):
            continue
        entry_id = str(raw_entry.get("id", "")).strip()
        if not entry_id:
            continue
        tier = str(raw_entry.get("tier", "manual")).strip().lower()
        if tier not in {"manual", "auto"}:
            raise ValueError(f"Unsupported tier '{tier}' in {path} entry '{entry_id}'")
        raw_selectors = raw_entry.get("selectors")
        if not isinstance(raw_selectors, list):
            raise ValueError(f"Entry '{entry_id}' in {path} is missing selectors[]")
        selectors: List[str] = []
        for selector in raw_selectors:
            if not isinstance(selector, str):
                continue
            normalized = normalize_selector(selector)
            if normalized and normalized not in selectors:
                selectors.append(normalized)
        if not selectors:
            raise ValueError(f"Entry '{entry_id}' in {path} has no valid selectors")

        source_files: List[str] = []
        raw_source_files = raw_entry.get("source_files", [])
        if raw_source_files is None:
            raw_source_files = []
        if not isinstance(raw_source_files, list):
            raise ValueError(f"Entry '{entry_id}' in {path} has a non-list source_files")
        for source_file in raw_source_files:
            if not isinstance(source_file, str) or not source_file.strip():
                continue
            source_files.append(resolve_path(path_root, source_file.strip()))
        if tier == "auto" and not source_files:
            raise ValueError(f"Auto entry '{entry_id}' in {path} needs at least one source_files entry")

        entries.append({
            "id": entry_id,
            "tier": tier,
            "selectors": selectors,
            "source_files": source_files,
            "notes": str(raw_entry.get("notes", "")).strip(),
            "registry_path": path,
            "path_root": path_root,
            "generated_output": generated_output,
        })

    return {
        "path": path,
        "path_root": path_root,
        "generated_output": generated_output,
        "entries": entries,
    }


def collect_registry_paths(args: argparse.Namespace) -> List[str]:
    requested = [resolve_path(os.getcwd(), path) for path in (args.registry or []) if path]
    if requested:
        return requested
    if args.registry_tier or args.only_entry:
        default_path = os.path.join(repo_root(), "data", "json", "npcs", "Backgrounds", "summary_registry.json")
        if os.path.exists(default_path):
            return [default_path]
    return []


def collect_registry_entries(args: argparse.Namespace) -> List[Dict[str, Any]]:
    entries: List[Dict[str, Any]] = []
    registry_paths = collect_registry_paths(args)
    for registry_path in registry_paths:
        registry = load_summary_registry(registry_path)
        for entry in registry["entries"]:
            if args.registry_tier and args.registry_tier != "all" and entry["tier"] != args.registry_tier:
                continue
            if args.only_entry and entry["id"] != args.only_entry:
                continue
            entries.append(entry)
    return entries


def collect_story_lines_from_paths(paths: List[str]) -> Dict[str, Any]:
    dynamic_lines: List[str] = []
    responses: List[str] = []
    resolved_paths: List[str] = []
    for path in paths:
        if not os.path.exists(path):
            continue
        try:
            with open(path, "r", encoding="utf-8") as handle:
                data = json.load(handle)
        except Exception:
            continue
        if isinstance(data, dict):
            entries = [data]
        elif isinstance(data, list):
            entries = data
        else:
            continue
        resolved_paths.append(path)
        for entry in entries:
            if not isinstance(entry, dict):
                continue
            if entry.get("type") != "talk_topic":
                continue
            dyn_lines, topic_responses = read_talk_topic_fields(entry)
            if dyn_lines:
                dynamic_lines.extend(dyn_lines)
            if topic_responses:
                responses.extend(topic_responses)
    return {
        "dynamic_lines": dynamic_lines,
        "responses": responses,
        "resolved_paths": resolved_paths,
    }


def prepare_backend(args: argparse.Namespace, backend: str):
    if backend not in {"ollama", "openvino"}:
        print(f"Unsupported summary backend: {backend}. Aborting.")
        return None, 2
    if backend == "openvino":
        if not args.model_dir or not os.path.isdir(args.model_dir):
            print(f"LLM summary model dir not found: {args.model_dir}. Aborting.")
            return None, 2
        try:
            import openvino_genai  # noqa: F401
        except Exception as exc:
            print(f"LLM summary dependency missing: {exc}. Aborting.")
            return None, 2
        try:
            return load_pipeline(args.model_dir, args.device, args.max_prompt_len), 0
        except Exception as exc:
            print(f"Failed to load LLM pipeline: {exc}. Aborting.")
            return None, 2
    if not args.ollama_model.strip():
        print("LLM summary Ollama model is required. Aborting.")
        return None, 2
    return None, 0


def generate_summary_text(prompt: str, label: str, args: argparse.Namespace,
                          backend: str, pipe) -> Tuple[str, str]:
    prompt_modes = [False, True]
    attempts = max(1, args.retry_invalid + 1)
    for strict_prompt in prompt_modes:
        active_prompt = build_prompt_from_text(prompt, strict=strict_prompt)
        if args.debug_io:
            mode_name = "strict" if strict_prompt else "default"
            print(f"Prompt for {label} ({mode_name}):\n{active_prompt}\n")
        prompt_tokens = max(1, len(active_prompt.split()))
        max_length = min(args.max_prompt_len, prompt_tokens + args.max_tokens)
        for attempt in range(attempts):
            if backend == "openvino":
                try:
                    result = pipe.generate(
                        active_prompt,
                        max_length=max_length,
                        do_sample=True,
                        temperature=args.temperature,
                        top_p=args.top_p,
                        repetition_penalty=args.repetition_penalty,
                    )
                except TypeError:
                    result = pipe.generate(
                        active_prompt,
                        max_length=max_length,
                        do_sample=False,
                    )
                raw_text = "" if result is None else str(result)
            else:
                response = ollama_generate(
                    args.ollama_url,
                    args.ollama_model,
                    active_prompt,
                    args.max_tokens,
                    args.temperature,
                )
                raw_text = str(response.get("response", ""))
            if args.debug_io:
                print(f"Raw output for {label}:\n{raw_text}\n")
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
                        print(f"Invalid summary output for {label}: {raw_text!r}")
                    if attempt + 1 < attempts:
                        continue
            else:
                summary = raw_text.strip()
            if is_valid_words(summary):
                return summarize_to_lines(summary)
            if args.debug_invalid:
                print(f"Invalid summary output for {label}: {raw_text!r}")
            if attempt + 1 < attempts:
                continue
    return "", ""


def build_prompt_from_text(story_text: str, strict: bool = False) -> str:
    filename = STRICT_SUMMARIZER_PROMPT_FILENAME if strict else DEFAULT_SUMMARIZER_PROMPT_FILENAME
    template = load_prompt_template(
        filename,
        default_summarizer_prompt_template(strict=strict),
        ["{{story_text_block}}"],
    )
    story_text = story_text.strip()
    story_text_block = f"{story_text}\n\n" if story_text else ""
    return render_prompt_template(template, {"{{story_text_block}}": story_text_block})


def prune_registry_entries(existing: Dict[str, Dict[str, Any]], entry_ids: List[str], full_rebuild: bool) -> None:
    for key, value in list(existing.items()):
        source_tag = str(value.get("source_tag", ""))
        if full_rebuild:
            if ":registry:" in source_tag:
                existing.pop(key, None)
            continue
        for entry_id in entry_ids:
            if source_tag.endswith(f":registry:{entry_id}"):
                existing.pop(key, None)
                break


def run_background_topic_generation(args: argparse.Namespace, backend: str, pipe) -> int:
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

    if args.dry_run:
        print(f"Dry run: {len(topics)} background topic summaries would be considered.")
        for topic_id in topics:
            info = topic_index.get(topic_id)
            if not info:
                print(f"- missing source for {topic_id}")
                continue
            source_base = os.path.splitext(os.path.basename(info['source']))[0]
            out_path = os.path.join(args.out_dir, f"{source_base}_summary_short.txt")
            print(f"- {topic_id} -> {out_path}")
        return 0

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
        story_sections: List[str] = []
        if prompt_lines:
            story_sections.append("NPC lines:")
            story_sections.extend([f"- {line}" for line in prompt_lines])
        if prompt_responses:
            story_sections.append("Player responses:")
            story_sections.extend([f"- {line}" for line in prompt_responses])
        background_line, expression_line = generate_summary_text(
            "\n".join(story_sections),
            topic_id,
            args,
            backend,
            pipe,
        )
        if not background_line and not expression_line:
            print(f"Invalid summary for {topic_id}; skipping.")
            continue
        model_ref = args.model_dir if backend == "openvino" else args.ollama_model
        source_tag = build_local_source_tag(
            backend,
            model_ref,
            re.sub(r"_\\d+$", "", source_base)
        )
        existing[topic_id] = {
            "id": topic_id,
            "your_background": background_line,
            "your_expression": expression_line,
            "source_tag": source_tag,
        }
        write_entries(out_path, existing)
        generated_any = True
        print(f"Generated summary for {topic_id} -> {out_path}")

    if not generated_any:
        print("No new summaries needed.")
    return 0


def run_registry_generation(args: argparse.Namespace, backend: str, pipe) -> int:
    registry_entries = collect_registry_entries(args)
    if not registry_entries:
        print("No registry entries matched the current filters.")
        return 0

    if args.dry_run:
        print(f"Dry run: {len(registry_entries)} registry entries matched.")
        for entry in registry_entries:
            story = collect_story_lines_from_paths(entry["source_files"])
            print(
                f"- {entry['id']} [{entry['tier']}] -> {entry['generated_output']} "
                f"selectors={len(entry['selectors'])} source_files={len(entry['source_files'])} "
                f"resolved_files={len(story['resolved_paths'])} npc_lines={len(story['dynamic_lines'])}"
            )
        return 0

    grouped: Dict[str, List[Dict[str, Any]]] = {}
    for entry in registry_entries:
        if entry["tier"] != "auto":
            continue
        grouped.setdefault(entry["generated_output"], []).append(entry)

    if not grouped:
        print("No auto-tier registry entries matched the current filters.")
        return 0

    generated_any = False
    for output_path, entries in grouped.items():
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        existing = load_existing_entries(output_path)
        prune_registry_entries(existing, [entry["id"] for entry in entries], not args.only_entry)
        for entry in entries:
            story = collect_story_lines_from_paths(entry["source_files"])
            if not story["dynamic_lines"] and not story["responses"]:
                print(f"No talk_topic lines found for registry entry {entry['id']}; skipping.")
                continue
            prompt_sections: List[str] = []
            if story["dynamic_lines"]:
                prompt_sections.append("NPC lines:")
                prompt_sections.extend([f"- {line}" for line in story["dynamic_lines"]])
            if args.include_responses and story["responses"]:
                prompt_sections.append("Player responses:")
                prompt_sections.extend([f"- {line}" for line in story["responses"]])
            background_line, expression_line = generate_summary_text(
                "\n".join(prompt_sections),
                entry["id"],
                args,
                backend,
                pipe,
            )
            if not background_line and not expression_line:
                print(f"Invalid summary for registry entry {entry['id']}; skipping.")
                continue
            model_ref = args.model_dir if backend == "openvino" else args.ollama_model
            source_tag = build_local_source_tag(backend, model_ref, f"registry:{entry['id']}")
            for selector in entry["selectors"]:
                existing[selector] = {
                    "id": selector,
                    "your_background": background_line,
                    "your_expression": expression_line,
                    "source_tag": source_tag,
                }
            generated_any = True
            print(f"Generated named NPC summary for {entry['id']} -> {output_path}")
        write_entries(output_path, existing)

    if not generated_any:
        print("No new named NPC summaries needed.")
    return 0


def parse_args() -> argparse.Namespace:
    repo_root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    parser = argparse.ArgumentParser(description="Generate short background summaries.")
    parser.add_argument(
        "--data-dir",
        default=os.path.join(repo_root_dir, "data", "json", "npcs", "Backgrounds"),
        help="Backgrounds directory.",
    )
    parser.add_argument(
        "--toc-path",
        default=os.path.join(repo_root_dir, "data", "json", "npcs", "Backgrounds",
                             "backgrounds_table_of_contents.json"),
        help="Table of contents JSON path.",
    )
    parser.add_argument(
        "--out-dir",
        default=os.path.join(repo_root_dir, "data", "json", "npcs", "Backgrounds", "Summaries_short"),
        help="Output directory for background topic summaries.",
    )
    parser.add_argument(
        "--registry",
        action="append",
        default=[],
        help="Path to a named-NPC summary registry JSON file. Repeat to load multiple registries.",
    )
    parser.add_argument(
        "--registry-tier",
        choices=["auto", "manual", "all"],
        default="",
        help="Filter named-NPC registry entries by tier. If set without --registry, the default core registry is used.",
    )
    parser.add_argument(
        "--only-entry",
        default="",
        help="If set with registry mode, only generate the matching registry entry id.",
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
        "--dry-run",
        action="store_true",
        help="Validate inputs and print planned outputs without calling the LLM backend.",
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
    registry_mode = bool(args.registry or args.registry_tier or args.only_entry)

    pipe = None
    if not args.dry_run:
        pipe, rc = prepare_backend(args, backend)
        if rc != 0:
            return rc

    if registry_mode:
        if not args.registry_tier:
            args.registry_tier = "auto"
        return run_registry_generation(args, backend, pipe)
    return run_background_topic_generation(args, backend, pipe)


if __name__ == "__main__":
    raise SystemExit(main())
