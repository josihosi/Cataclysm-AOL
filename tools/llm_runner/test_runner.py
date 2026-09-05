"""Final-response contract at the real Ollama runner boundary; no inference."""
import argparse
import io
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import runner


class RunnerFinalResponseTest(unittest.TestCase):
    def test_ollama_request_explicitly_disables_thinking(self):
        raw = {"response": "Holding.|wait_here", "thinking": "diagnostic only"}
        with patch.object(runner.urllib.request, "urlopen", return_value=io.BytesIO(json.dumps(raw).encode())) as send:
            self.assertEqual(runner.ollama_generate("http://127.0.0.1:11434", "installed-model", "game snapshot", 256, 0.6), raw)
        request = send.call_args.args[0]
        self.assertEqual(request.full_url, "http://127.0.0.1:11434/api/generate")
        self.assertEqual(json.loads(request.data), {
            "model": "installed-model", "prompt": "game snapshot", "stream": False,
            "think": False, "options": {"temperature": 0.6, "num_predict": 256},
        })

    def run_response(self, raw):
        args = argparse.Namespace(ollama_model="installed-model", ollama_url="http://127.0.0.1:11434")
        request = {"request_id": "native-request", "prompt": "game snapshot", "max_tokens": 256}
        incoming = io.StringIO(json.dumps(request) + "\n")
        outgoing, log = io.StringIO(), io.StringIO()
        with patch.object(runner.sys, "stdin", incoming), patch.object(runner.sys, "stdout", outgoing), patch.object(runner, "ollama_generate", return_value=raw):
            self.assertEqual(runner.run_ollama_mode(args, log), 0)
        result = json.loads(outgoing.getvalue())
        self.assertEqual(result["request_id"], "native-request")
        # Exact raw backend evidence survives separately from the playable text.
        self.assertIn(json.dumps(raw, ensure_ascii=True), log.getvalue())
        return result

    def test_runner_rejects_thought_only_but_retains_raw_evidence(self):
        for raw in (
            {"response": "<|channel>thought\nThinking Process: follow_close", "done_reason": "length"},
            {"response": "<think>Thinking Process: follow_close", "done_reason": "length"},
            {"response": "", "thinking": "Reasoning without an answer", "done_reason": "length"},
            {"response": "<|channel>thought\nOnly reasoning<channel|>"},
            {"response": "<|channel>thought\nReasoning<channel|>Holding.|wait_here"},
            {"response": "Holding.|wait_here", "thinking": "Leaked separate reasoning"},
        ):
            with self.subTest(raw=raw):
                result = self.run_response(raw)
                self.assertFalse(result["ok"])
                self.assertEqual(result["text"], "")
                self.assertIn("no acceptable final text", result["error"])

    def test_runner_keeps_final_field_and_supported_plain_formats(self):
        for text in ("Holding here.|hold_position", "Speech|bogus follow_close", "Moving+move=-20,+20 wait_here"):
            with self.subTest(text=text):
                result = self.run_response({"response": text, "thinking": ""})
                self.assertTrue(result["ok"])
                self.assertEqual(result["text"], text)

    def test_self_test_uses_same_nonthinking_final_text_contract(self):
        args = argparse.Namespace(ollama_model="installed-model", ollama_url="http://127.0.0.1:11434", self_test_prompt="fixture", max_tokens=256)
        for raw, expected in (({"response": "Ready"}, 0),
                              ({"response": "Ready", "thinking": "leak"}, 1),
                              ({"response": "<|channel>thought\nunfinished"}, 1)):
            with self.subTest(raw=raw), patch.object(runner, "ollama_generate", return_value=raw), patch.object(runner.sys, "stdout", io.StringIO()), patch.object(runner.sys, "stderr", io.StringIO()):
                self.assertEqual(runner.run_ollama_self_test(args, io.StringIO()), expected)

    def test_empty_reasoning_delimiters_preserve_final_text(self):
        result = self.run_response({"response": "<think></think><|channel>thought\n<channel|>Holding.|wait_here"})
        self.assertTrue(result["ok"])
        self.assertEqual(result["text"], "Holding.|wait_here")
        self.assertEqual(runner.strip_think_tags("I think we should wait.|wait_here"), "I think we should wait.|wait_here")
        self.assertEqual(runner.strip_think_tags("<channel|>secret|attack=a"), "")


if __name__ == "__main__":
    unittest.main()
