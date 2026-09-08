"""Recoverable, source-bound ingestion for the evidence-search trial.

This is deliberately a derived store.  It never changes an evidence artifact
and callers must recover excerpts through ``cockpit_evidence.record_artifact``.
The database records source generations rather than treating a changed path as
the same evidence.  Its transaction is the publication boundary: a cursor is
never advanced without the corresponding occurrences.
"""
from __future__ import annotations

from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import sqlite3
from typing import Any, Iterable, Iterator

from cockpit_evidence import parse_record
from evidence_events import envelopes, parse


SCHEMA_VERSION = 2
CHUNKING_VERSION = "complete-record-v1"
DEFAULT_SUFFIXES = frozenset({".jsonl", ".log", ".md", ".json"})


def _sha(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def _line_text(raw: bytes) -> str:
    return raw.decode("utf-8", errors="replace").rstrip("\r\n")


class EvidenceIndex:
    """SQLite source inventory and exact record occurrence store.

    A caller supplies explicit roots; ``discover`` skips the no-go directory
    and only selects evidence-like file types.  This makes reconciliation
    automatic without quietly indexing all workspace history.
    """

    def __init__(self, database: Path, *, model_id: str, model_version: str,
                 chunking_version: str = CHUNKING_VERSION) -> None:
        self.database = Path(database)
        self.model_id = model_id
        self.model_version = model_version
        self.chunking_version = chunking_version
        self.database.parent.mkdir(parents=True, exist_ok=True)
        self.db = sqlite3.connect(self.database)
        self.db.row_factory = sqlite3.Row
        self.db.execute("PRAGMA foreign_keys = ON")
        self._create()

    def close(self) -> None:
        self.db.close()

    def _create(self) -> None:
        self.db.executescript("""
        CREATE TABLE IF NOT EXISTS sources (
          path TEXT PRIMARY KEY, active_generation INTEGER, status TEXT NOT NULL,
          last_error TEXT, updated_ns INTEGER NOT NULL
        );
        CREATE TABLE IF NOT EXISTS generations (
          generation_id INTEGER PRIMARY KEY, path TEXT NOT NULL REFERENCES sources(path),
          generation INTEGER NOT NULL, prefix_sha256 TEXT NOT NULL, committed_bytes INTEGER NOT NULL,
          file_bytes INTEGER NOT NULL, status TEXT NOT NULL, UNIQUE(path, generation)
        );
        CREATE TABLE IF NOT EXISTS chunks (
          chunk_sha256 TEXT NOT NULL, model_id TEXT NOT NULL, model_version TEXT NOT NULL,
          chunking_version TEXT NOT NULL, text TEXT NOT NULL, embedding_json TEXT,
          PRIMARY KEY(chunk_sha256, model_id, model_version, chunking_version)
        );
        CREATE TABLE IF NOT EXISTS occurrences (
          occurrence_id TEXT PRIMARY KEY, generation_id INTEGER NOT NULL REFERENCES generations(generation_id),
          ordinal INTEGER NOT NULL, path TEXT NOT NULL, offset INTEGER NOT NULL, length INTEGER NOT NULL,
          raw_sha256 TEXT NOT NULL, chunk_sha256 TEXT NOT NULL, event_id TEXT, event TEXT,
          run_id TEXT, actor_id TEXT, request_id TEXT, source_handle TEXT NOT NULL,
          UNIQUE(generation_id, offset, raw_sha256)
        );
        CREATE TABLE IF NOT EXISTS index_meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);
        CREATE INDEX IF NOT EXISTS occurrences_generation ON occurrences(generation_id, ordinal);
        CREATE INDEX IF NOT EXISTS occurrences_event ON occurrences(event, run_id, actor_id);
        """)
        # Existing trial databases created before vector activation remain
        # readable; their chunks are intentionally re-embedded on demand.
        try:
            self.db.execute("ALTER TABLE chunks ADD COLUMN embedding_json TEXT")
        except sqlite3.OperationalError:
            pass
        # Version 1 wrote the per-source revision number into
        # ``sources.active_generation`` even though all readers interpret it
        # as the globally unique ``generations.generation_id``.  Repair that
        # reference on open before publishing the new schema version.  The
        # migration is deliberately source-local and preserves every
        # generation/occurrence row; only the pointer is corrected.
        previous = self.db.execute(
            "SELECT value FROM index_meta WHERE key='schema_version'"
        ).fetchone()
        if previous is None or int(previous[0]) < 2:
            self._migrate_generation_references()
        self.db.execute("INSERT OR REPLACE INTO index_meta(key, value) VALUES (?, ?)",
                        ("schema_version", str(SCHEMA_VERSION)))
        self.db.commit()

    def _migrate_generation_references(self) -> None:
        """Point each source at its own current global generation ID.

        Indexes written by schema v1 may have a pointer that accidentally
        resolves to another source's generation (typically after ingesting
        two files whose local revision is both ``1``).  Prefer the source's
        current generation, falling back to its newest generation when the
        source is unavailable or an interrupted legacy write left no current
        status.  No occurrence or chunk data is rewritten.
        """
        rows = self.db.execute("SELECT path, status FROM sources").fetchall()
        for row in rows:
            generation = self.db.execute(
                "SELECT generation_id FROM generations "
                "WHERE path=? AND status='current' ORDER BY generation DESC, generation_id DESC LIMIT 1",
                (row["path"],),
            ).fetchone()
            if generation is None:
                generation = self.db.execute(
                    "SELECT generation_id FROM generations "
                    "WHERE path=? ORDER BY generation DESC, generation_id DESC LIMIT 1",
                    (row["path"],),
                ).fetchone()
            if generation is not None:
                self.db.execute(
                    "UPDATE sources SET active_generation=? WHERE path=?",
                    (generation["generation_id"], row["path"]),
                )

    @staticmethod
    def discover(roots: Iterable[Path]) -> list[Path]:
        result: list[Path] = []
        for root in roots:
            root = Path(root)
            if not root.exists():
                continue
            if root.is_file():
                candidates = [root]
            else:
                candidates = (path for path in root.rglob("*")
                              if ".de67/no-go-zone" not in str(path))
            for path in candidates:
                if path.is_file() and path.suffix.lower() in DEFAULT_SUFFIXES:
                    result.append(path.resolve())
        return sorted(set(result))

    @contextmanager
    def _transaction(self) -> Iterator[None]:
        try:
            self.db.execute("BEGIN IMMEDIATE")
            yield
        except BaseException:
            self.db.rollback()
            raise
        else:
            self.db.commit()

    def _source_row(self, path: str) -> sqlite3.Row | None:
        return self.db.execute("SELECT * FROM sources WHERE path = ?", (path,)).fetchone()

    def _new_generation(self, path: str, raw: bytes, *, status: str = "current") -> sqlite3.Row:
        number = self.db.execute(
            "SELECT COALESCE(MAX(generation), 0) + 1 FROM generations WHERE path=?",
            (path,),
        ).fetchone()[0]
        now = int(os.stat(self.database).st_mtime_ns) if self.database.exists() else 0
        # Publish the source row first so the generations foreign key remains
        # valid; its pointer is updated to the global row ID immediately after
        # the generation insert succeeds.
        self.db.execute("INSERT INTO sources(path, active_generation, status, last_error, updated_ns) VALUES (?, NULL, ?, NULL, ?) "
                        "ON CONFLICT(path) DO UPDATE SET status=excluded.status, last_error=NULL, updated_ns=excluded.updated_ns",
                        (path, status, now))
        cursor = self.db.execute("INSERT INTO generations(path, generation, prefix_sha256, committed_bytes, file_bytes, status) VALUES (?, ?, ?, 0, ?, ?)",
                                 (path, number, _sha(b""), len(raw), status))
        generation_id = cursor.lastrowid
        self.db.execute("UPDATE sources SET active_generation=? WHERE path=?", (generation_id, path))
        return self.db.execute("SELECT * FROM generations WHERE generation_id = ?", (cursor.lastrowid,)).fetchone()

    @staticmethod
    def _records(path: str, raw: bytes, start: int) -> Iterator[tuple[int, bytes]]:
        """Yield only newline-complete records, retaining a partial tail."""
        # JSON response/artifact files are one complete value, not JSONL.  They
        # retain their own artifact identity and need not gain a fake newline.
        if Path(path).suffix == ".json" and start == 0:
            try:
                json.loads(raw)
            except (UnicodeDecodeError, json.JSONDecodeError):
                return
            if raw.strip():
                yield 0, raw
            return
        position = start
        for part in raw[start:].splitlines(keepends=True):
            if not part.endswith((b"\n", b"\r")):
                break
            if part.strip():
                yield position, part
            position += len(part)

    def _store_record(self, generation: sqlite3.Row, ordinal: int, offset: int, raw: bytes) -> None:
        record = parse(raw)
        source = {"path": generation["path"], "offset": offset, "length": len(raw),
                  "sha256": _sha(raw), "producer": Path(generation["path"]).name}
        event = next(envelopes(record, source))
        text = _line_text(raw)
        chunk = _sha(text.encode("utf-8"))
        self.db.execute("INSERT OR IGNORE INTO chunks(chunk_sha256, model_id, model_version, chunking_version, text) VALUES (?, ?, ?, ?, ?)",
                        (chunk, self.model_id, self.model_version, self.chunking_version, text))
        occurrence = _sha((str(generation["generation_id"]) + ":" + str(offset) + ":" + source["sha256"]).encode())
        handle = {key: source[key] for key in ("path", "offset", "length", "sha256")}
        self.db.execute("INSERT OR IGNORE INTO occurrences VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                        (occurrence, generation["generation_id"], ordinal, generation["path"], offset, len(raw),
                         source["sha256"], chunk, event.get("event_id"), event.get("event"), event.get("run_id"),
                         event.get("actor_id"), event.get("request_id"), json.dumps(handle, sort_keys=True)))

    def ingest(self, path: Path, *, fail_after_records: int | None = None) -> dict[str, Any]:
        """Reconcile one source. ``fail_after_records`` is a test-only crash seam."""
        path = str(Path(path).resolve())
        try:
            raw = Path(path).read_bytes()
        except OSError as error:
            with self._transaction():
                self.db.execute("UPDATE sources SET status='unavailable', last_error=? WHERE path=?", (str(error), path))
                self.db.execute("UPDATE generations SET status='unavailable' WHERE path=? AND status='current'", (path,))
            return {"path": path, "status": "unavailable", "error": str(error)}
        with self._transaction():
            source = self._source_row(path)
            generation = None
            if source and source["active_generation"]:
                generation = self.db.execute("SELECT * FROM generations WHERE generation_id=?", (source["active_generation"],)).fetchone()
            if generation is None:
                generation = self._new_generation(path, raw)
            elif len(raw) < generation["committed_bytes"] or _sha(raw[:generation["committed_bytes"]]) != generation["prefix_sha256"]:
                self.db.execute("UPDATE generations SET status='stale' WHERE generation_id=?", (generation["generation_id"],))
                generation = self._new_generation(path, raw)
            start = int(generation["committed_bytes"])
            ordinal = self.db.execute("SELECT COUNT(*) FROM occurrences WHERE generation_id=?", (generation["generation_id"],)).fetchone()[0]
            inserted = 0
            end = start
            for offset, record_raw in self._records(path, raw, start):
                self._store_record(generation, ordinal, offset, record_raw)
                ordinal += 1; inserted += 1; end = offset + len(record_raw)
                if fail_after_records is not None and inserted >= fail_after_records:
                    raise RuntimeError("simulated_ingest_crash")
            self.db.execute("UPDATE generations SET prefix_sha256=?, committed_bytes=?, file_bytes=?, status='current' WHERE generation_id=?",
                            (_sha(raw[:end]), end, len(raw), generation["generation_id"]))
            self.db.execute("UPDATE sources SET status='current', last_error=NULL WHERE path=?", (path,))
        return {"path": path, "status": "current", "generation": generation["generation"],
                "ingested_records": inserted, "committed_bytes": end, "file_bytes": len(raw),
                "pending_tail_bytes": len(raw) - end}

    def reconcile(self, paths: Iterable[Path]) -> list[dict[str, Any]]:
        selected = {str(Path(path).resolve()) for path in paths}
        results = [self.ingest(path) for path in sorted(selected)]
        for row in self.db.execute("SELECT path FROM sources WHERE status='current'"):
            if row["path"] not in selected:
                results.append(self.ingest(Path(row["path"])))
        return results

    def rebuild_model_cache(self) -> dict[str, Any]:
        """Populate this model/chunking namespace from retained current originals.

        Occurrence identities are intentionally immutable across a model
        replacement.  The content-addressed chunks are namespaced by model and
        chunker so a caller must explicitly rebuild rather than silently mixing
        representations from two versions.
        """
        rebuilt = 0
        with self._transaction():
            rows = self.db.execute("SELECT * FROM generations WHERE status='current' ORDER BY path, generation").fetchall()
            for generation in rows:
                try:
                    raw = Path(generation["path"]).read_bytes()
                except OSError:
                    continue
                for offset, record_raw in self._records(generation["path"], raw, 0):
                    ordinal = self.db.execute("SELECT ordinal FROM occurrences WHERE generation_id=? AND offset=?", (generation["generation_id"], offset)).fetchone()
                    if ordinal is None:
                        continue
                    self._store_record(generation, ordinal["ordinal"], offset, record_raw)
                    rebuilt += 1
        return {"model_id": self.model_id, "model_version": self.model_version,
                "chunking_version": self.chunking_version, "rebuilt_records": rebuilt}

    def embed_missing(self, backend: Any, *, batch_size: int = 32) -> dict[str, Any]:
        """Persist vectors once per content/model/chunker identity.

        Embedding happens after source publication, so a local model outage
        leaves a queryable exact inventory rather than losing evidence.  A
        backend identity mismatch is explicit instead of mixing vectors.
        """
        if getattr(backend, "model_id", None) != self.model_id or \
                getattr(backend, "model_version", None) != self.model_version:
            raise ValueError("embedding_backend_model_identity_mismatch")
        if batch_size < 1:
            raise ValueError("invalid_embedding_batch_size")
        rows = self.db.execute("SELECT chunk_sha256, text FROM chunks WHERE model_id=? AND model_version=? AND chunking_version=? AND embedding_json IS NULL ORDER BY chunk_sha256",
                               (self.model_id, self.model_version, self.chunking_version)).fetchall()
        embedded = 0
        for start in range(0, len(rows), batch_size):
            batch = rows[start:start + batch_size]
            vectors = backend.embed([row["text"] for row in batch])
            if len(vectors) != len(batch):
                raise RuntimeError("embedding_backend_vector_count_mismatch")
            with self._transaction():
                for row, vector in zip(batch, vectors):
                    self.db.execute("UPDATE chunks SET embedding_json=? WHERE chunk_sha256=? AND model_id=? AND model_version=? AND chunking_version=? AND embedding_json IS NULL",
                                    (json.dumps(vector, separators=(",", ":")), row["chunk_sha256"], self.model_id,
                                     self.model_version, self.chunking_version))
                    embedded += 1
        return {"embedded_chunks": embedded, "model_id": self.model_id,
                "model_version": self.model_version, "chunking_version": self.chunking_version,
                "backend": getattr(backend, "backend_id", type(backend).__name__)}

    def occurrences(self, *, include_stale: bool = False) -> list[dict[str, Any]]:
        where = "" if include_stale else "WHERE g.status = 'current'"
        rows = self.db.execute("SELECT o.*, g.generation, g.status AS generation_status FROM occurrences o JOIN generations g ON g.generation_id=o.generation_id " + where + " ORDER BY o.path, o.offset").fetchall()
        return [dict(row) | {"source_handle": json.loads(row["source_handle"])} for row in rows]

    def coverage(self) -> list[dict[str, Any]]:
        return [dict(row) for row in self.db.execute("SELECT s.path, s.status, s.active_generation, g.committed_bytes, g.file_bytes, g.status AS generation_status FROM sources s LEFT JOIN generations g ON g.generation_id=s.active_generation ORDER BY s.path")]
