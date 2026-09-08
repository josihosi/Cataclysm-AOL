"""Shared query and verified expansion surface for the derived evidence index.

The index is only a ranking aid.  Every excerpt is recovered from its original
source span through :func:`cockpit_evidence.record_artifact` before it is
returned, so stale or modified sources cannot masquerade as evidence.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import sqlite3
import sys
from typing import Any

from cockpit_evidence import record_artifact
from evidence_search_backend import LlamaCppEmbeddingBackend
from evidence_search_index import EvidenceIndex


def _terms(value: str) -> set[str]:
    return {part for part in value.casefold().split() if part}


# These fields are deliberately denormalized into ``occurrences`` and may be
# used to reject a candidate before recovering its original record.  Other
# filter keys belong to the original JSON record and must be evaluated after
# recovery (for example, ``feature`` or any producer-specific field).
_OCCURRENCE_FILTER_FIELDS = frozenset({
    "occurrence_id", "generation_id", "generation", "generation_status",
    "ordinal", "path", "offset", "length", "raw_sha256", "chunk_sha256",
    "event_id", "event", "run_id", "actor_id", "request_id", "source_handle",
})


class EvidenceSearch:
    def __init__(self, index: EvidenceIndex, backend: Any | None = None) -> None:
        self.index, self.backend = index, backend

    def query(self, text: str, *, filters: dict[str, Any] | None = None,
              offset: int = 0, limit: int = 20, expand: int = 0) -> dict[str, Any]:
        if offset < 0 or limit < 1 or expand < 0:
            return {"ok": False, "status": "invalid", "error": "invalid_paging"}
        filters = dict(filters or {})
        rows = self.index.occurrences()
        candidates: list[dict[str, Any]] = []
        source_errors: list[dict[str, Any]] = []
        semantic_errors: list[str] = []
        query_terms = _terms(text)
        query_vector: list[float] | None = None
        if self.backend:
            try:
                query_vector = self.backend.embed([text])[0]
            except (RuntimeError, ValueError, KeyError, IndexError) as error:
                semantic_errors.append(str(error))
        for row in rows:
            if any(row.get(key) != expected for key, expected in filters.items()
                   if key in _OCCURRENCE_FILTER_FIELDS):
                continue
            # Event fields are retained as columns where stable; arbitrary
            # filters can still target the original JSON record below.
            try:
                original = record_artifact(Path(row["source_handle"]["path"]),
                                            row["source_handle"]["offset"],
                                            row["source_handle"]["length"],
                                            row["source_handle"]["sha256"], [])
            except (OSError, TypeError, KeyError):
                original = {"ok": False, "error": "source_unavailable"}
            if not original.get("ok"):
                source_errors.append({"occurrence_id": row["occurrence_id"],
                                      "source": row["source_handle"],
                                      "error": original.get("error", "source_unavailable")})
                continue
            record = original.get("record", {})
            # A filter for null means an explicit JSON null.  It must not
            # quietly match a producer that omitted the key altogether.
            if any(key not in record or record[key] != expected for key, expected in filters.items()
                   if key not in _OCCURRENCE_FILTER_FIELDS):
                continue
            body = row.get("event", "") + " " + json.dumps(record, sort_keys=True)
            overlap = len(query_terms & _terms(body))
            exact = text.casefold() in body.casefold() if text else True
            if self.backend and query_vector is not None:
                # Stored vectors are intentionally optional.  Querying with a
                # backend is allowed to fail into an explicit lexical result.
                vectors: list[str] = []
                try:
                    vector_rows = self.index.db.execute(
                        "SELECT c.embedding_json FROM occurrence_chunks oc JOIN chunks c "
                        "ON c.chunk_sha256=oc.chunk_sha256 AND c.model_id=oc.model_id "
                        "AND c.model_version=oc.model_version AND c.chunking_version=oc.chunking_version "
                        "WHERE oc.occurrence_id=? AND oc.model_id=? AND oc.model_version=? "
                        "AND oc.chunking_version=? ORDER BY oc.chunk_ordinal",
                        (row["occurrence_id"], self.index.model_id, self.index.model_version,
                         self.index.chunking_version),
                    ).fetchall()
                    vectors = [item["embedding_json"] for item in vector_rows if item["embedding_json"]]
                    if vectors:
                        norm = math.sqrt(sum(value * value for value in query_vector))
                        score = max(
                            (sum(a * b for a, b in zip(query_vector, json.loads(vector))) /
                             (norm * math.sqrt(sum(value * value for value in json.loads(vector)))))
                            if norm and math.sqrt(sum(value * value for value in json.loads(vector))) else 0.0
                            for vector in vectors
                        )
                        if len(vectors) != len(vector_rows):
                            semantic_errors.append("indexed_embedding_missing")
                    else:
                        semantic_errors.append("indexed_embedding_missing")
                        score = float(overlap)
                except (RuntimeError, ValueError, KeyError, IndexError) as error:
                    semantic_errors.append(str(error))
                    score = float(overlap)
                reason = "semantic similarity" if vectors and query_vector is not None else "term overlap fallback"
            else:
                score, reason = float(overlap), ("exact text" if exact else "term overlap")
            if score <= 0 and not exact:
                continue
            candidates.append({"row": row, "record": record, "score": score,
                               "reason": reason, "exact": exact})
        candidates.sort(key=lambda item: (-item["score"], item["row"]["occurrence_id"]))
        page = candidates[offset:offset + limit]
        results = []
        unavailable = source_errors
        for item in page:
            row = item["row"]
            handle = row["source_handle"]
            recovered = record_artifact(Path(handle["path"]), handle["offset"],
                                         handle["length"], handle["sha256"], [])
            if not recovered.get("ok"):
                unavailable.append({"occurrence_id": row["occurrence_id"],
                                    "error": recovered.get("error", "unavailable")})
                continue
            result = {"occurrence_id": row["occurrence_id"], "excerpt": recovered.get("raw"),
                      "match_reason": item["reason"], "score": item["score"],
                      "source": handle, "generation": row["generation"],
                      "generation_status": row["generation_status"],
                      "bindings": {key: row.get(key) for key in ("run_id", "actor_id", "request_id")},
                      "freshness": "current"}
            if expand:
                result["context"] = self._expand(row, expand)
            results.append(result)
        coverage = self.index.coverage()
        status = "matched" if results else "no_match"
        if unavailable or semantic_errors or any(item["status"] != "current" or item["committed_bytes"] < item["file_bytes"]
                               for item in coverage):
            status = "partial" if results else "degraded"
        return {"ok": True, "status": status, "query": text, "filters": filters,
                "rows": results, "matched": len(candidates), "scanned": len(rows),
                "unavailable": unavailable, "coverage": coverage,
                "semantic_backend": {"requested": self.backend is not None,
                                      "available": not semantic_errors if self.backend else False,
                                      "errors": semantic_errors},
                "next_offset": offset + limit if offset + limit < len(candidates) else None,
                "exact_fallback": {"available": True, "hint": "use text with filters for literal matching"}}

    def _expand(self, row: dict[str, Any], radius: int) -> list[dict[str, Any]]:
        all_rows = self.index.occurrences()
        siblings = [item for item in all_rows if item["path"] == row["path"] and
                    item["generation"] == row["generation"]]
        pos = next((i for i, item in enumerate(siblings) if item["occurrence_id"] == row["occurrence_id"]), 0)
        expanded = []
        for item in siblings[max(0, pos - radius):pos + radius + 1]:
            handle = item["source_handle"]
            recovered = record_artifact(Path(handle["path"]), handle["offset"], handle["length"], handle["sha256"], [])
            if recovered.get("ok"):
                expanded.append({"occurrence_id": item["occurrence_id"], "excerpt": recovered.get("raw"), "source": handle})
        return expanded


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="query indexed CAOL evidence")
    parser.add_argument("database", type=Path)
    parser.add_argument("text")
    parser.add_argument("--model", default="local-model")
    parser.add_argument("--version", default="v1")
    parser.add_argument("--endpoint", help="authorized local llama.cpp embedding endpoint")
    parser.add_argument("--filter", action="append", default=[], metavar="KEY=VALUE")
    parser.add_argument("--offset", type=int, default=0)
    parser.add_argument("--limit", type=int, default=20)
    parser.add_argument("--expand", type=int, default=0)
    args = parser.parse_args(argv)
    filters = dict(item.split("=", 1) for item in args.filter if "=" in item)
    try:
        index = EvidenceIndex(args.database, model_id=args.model, model_version=args.version)
    except (sqlite3.DatabaseError, OSError) as error:
        print(json.dumps({"ok": False, "status": "degraded", "error": "index_unavailable",
                          "detail": str(error), "exact_fallback": {"available": True,
                          "hint": "use cockpit_evidence query"}}))
        return 2
    try:
        backend = (LlamaCppEmbeddingBackend(args.model, args.version, args.endpoint)
                   if args.endpoint else None)
        print(json.dumps(EvidenceSearch(index, backend).query(args.text, filters=filters, offset=args.offset,
                                                     limit=args.limit, expand=args.expand), indent=2))
    finally:
        index.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
