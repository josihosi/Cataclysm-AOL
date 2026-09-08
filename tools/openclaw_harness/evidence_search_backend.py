"""Semantic backend capability checks for the evidence-search trial.

No model is downloaded, uploaded to, or started by this module.  The query
surface can ask this capability object whether a pre-authorized local Ollama
server exposes real embeddings and otherwise must report degraded search.
"""
from __future__ import annotations

import json
import math
from urllib.error import URLError
from urllib.request import Request, urlopen


class OllamaEmbeddingCapability:
    def __init__(self, model: str, endpoint: str = "http://127.0.0.1:11434") -> None:
        self.model, self.endpoint = model, endpoint.rstrip("/")

    def probe(self, timeout: float = 20.0) -> dict:
        request = Request(self.endpoint + "/api/embed",
                          data=json.dumps({"model": self.model, "input": "semantic evidence probe"}).encode(),
                          headers={"Content-Type": "application/json"}, method="POST")
        try:
            with urlopen(request, timeout=timeout) as response:
                body = json.loads(response.read())
            embeddings = body.get("embeddings")
            if isinstance(embeddings, list) and embeddings and isinstance(embeddings[0], list):
                return {"available": True, "backend": "ollama-api-embed", "model": self.model,
                        "dimensions": len(embeddings[0]), "endpoint": self.endpoint}
            return {"available": False, "backend": "ollama-api-embed", "model": self.model,
                    "error": "embedding_response_missing_vectors", "endpoint": self.endpoint}
        except (URLError, OSError, ValueError) as error:
            return {"available": False, "backend": "ollama-api-embed", "model": self.model,
                    "error": str(error), "endpoint": self.endpoint}


class LlamaCppEmbeddingBackend:
    """Task-owned llama.cpp embedding endpoint over an already-installed model.

    The caller owns starting and stopping the isolated server.  This object has
    no network route beyond that explicit loopback endpoint and keeps the model
    identity in every returned ranking for the index/query layer to persist.
    """
    backend_id = "llama.cpp-embedding"

    def __init__(self, model_id: str, model_version: str,
                 endpoint: str = "http://127.0.0.1:59617") -> None:
        self.model_id, self.model_version = model_id, model_version
        self.endpoint = endpoint.rstrip("/")

    def embed(self, texts: list[str], timeout: float = 60.0) -> list[list[float]]:
        request = Request(self.endpoint + "/embedding",
                          data=json.dumps({"content": texts}).encode(),
                          headers={"Content-Type": "application/json"}, method="POST")
        try:
            with urlopen(request, timeout=timeout) as response:
                payload = json.loads(response.read())
        except (URLError, OSError, ValueError) as error:
            raise RuntimeError("embedding_backend_unavailable: " + str(error)) from error
        if not isinstance(payload, list):
            raise RuntimeError("embedding_backend_invalid_response")
        vectors: list[list[float]] = []
        for item in payload:
            vector = item.get("embedding") if isinstance(item, dict) else None
            # llama.cpp currently returns a 2-D array for each supplied input.
            if isinstance(vector, list) and vector and isinstance(vector[0], list):
                vector = vector[0]
            if not isinstance(vector, list) or not vector or not all(isinstance(v, (int, float)) for v in vector):
                raise RuntimeError("embedding_backend_invalid_vector")
            vectors.append([float(value) for value in vector])
        if len(vectors) != len(texts) or len({len(vector) for vector in vectors}) != 1:
            raise RuntimeError("embedding_backend_vector_count_or_dimension_mismatch")
        return vectors

    def rank(self, query: str, candidates: list[str]) -> list[dict]:
        if not candidates:
            return []
        vectors = self.embed([query, *candidates])
        query_vector = vectors[0]
        query_norm = math.sqrt(sum(value * value for value in query_vector))
        if query_norm == 0:
            raise RuntimeError("embedding_backend_zero_query_vector")
        rows = []
        for index, vector in enumerate(vectors[1:]):
            norm = math.sqrt(sum(value * value for value in vector))
            if norm == 0:
                score = 0.0
            else:
                score = sum(a * b for a, b in zip(query_vector, vector)) / (query_norm * norm)
            rows.append({"candidate_index": index, "semantic_score": score,
                         "backend": self.backend_id, "model_id": self.model_id,
                         "model_version": self.model_version})
        return sorted(rows, key=lambda row: row["semantic_score"], reverse=True)
