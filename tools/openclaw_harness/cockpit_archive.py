"""Exact disk-backed cockpit evidence; native authority stays in the channel.

Sequences stream without an event cache. Exported JSON and witness canonical
hashes retain their existing values; only live transport uses references.
"""
from __future__ import annotations

from collections.abc import Mapping, MutableMapping, Sequence
import hashlib
import itertools
import json
from pathlib import Path
import sqlite3
import uuid


def is_sequence(value):
    return isinstance(value, (list, tuple, ArchiveSequence))


def json_chunks(value, *, sort_keys=False):
    """The existing compact JSON encoding, emitted one bounded value at a time."""
    if isinstance(value, Mapping):
        yield "{"
        keys = sorted(value) if sort_keys else value.keys()
        for index, key in enumerate(keys):
            if index:
                yield ","
            yield json.dumps(key, ensure_ascii=False)
            yield ":"
            yield from json_chunks(value[key], sort_keys=sort_keys)
        yield "}"
    elif isinstance(value, ArchiveSequence):
        yield from value.json_chunks(sort_keys=sort_keys)
    elif is_sequence(value):
        yield "["
        for index, item in enumerate(value):
            if index:
                yield ","
            yield from json_chunks(item, sort_keys=sort_keys)
        yield "]"
    else:
        yield json.dumps(value, ensure_ascii=False, separators=(",", ":"), sort_keys=sort_keys)


def value_digest(label, value):
    digest = hashlib.sha256((label + ":").encode())
    for chunk in json_chunks(value, sort_keys=True):
        digest.update(chunk.encode("utf-8"))
    return digest.hexdigest()


def find_archive(value):
    """Find a lazy container without visiting its archived history."""
    if isinstance(value, ArchiveSequence):
        return value.archive
    items = value.values() if isinstance(value, Mapping) else value if isinstance(value, (list, tuple)) else ()
    for item in items:
        archive = find_archive(item)
        if archive is not None:
            return archive
    return None


def write_json_stream(path, value, *, exclusive=True):
    path = Path(path).resolve()
    digest = hashlib.sha256()
    size = 0
    with Path(path).open("xb" if exclusive else "wb") as destination:
        for chunk in json_chunks(value):
            raw = chunk.encode("utf-8")
            destination.write(raw)
            digest.update(raw)
            size += len(raw)
    return {"path": str(path), "sha256": digest.hexdigest(), "bytes": size}


class Archive:
    def __init__(self, path, *, run_id, binding_id, readonly=False):
        self.path = Path(path).resolve()
        self.run_id, self.binding_id = run_id, binding_id
        self.readonly = readonly
        self.connection = sqlite3.connect(
            self.path.as_uri() + "?mode=ro" if readonly else str(self.path), uri=readonly)
        if not readonly:
            self.connection.execute("PRAGMA journal_mode=WAL")
            self.connection.executescript("""
                CREATE TABLE IF NOT EXISTS metadata (key TEXT PRIMARY KEY, value TEXT);
                CREATE TABLE IF NOT EXISTS streams (id TEXT PRIMARY KEY, count INTEGER NOT NULL);
                CREATE TABLE IF NOT EXISTS records (stream TEXT, seq INTEGER, value TEXT NOT NULL,
                    entry_key TEXT, kind TEXT, raw_sha256 TEXT, PRIMARY KEY(stream, seq));
                CREATE INDEX IF NOT EXISTS record_key ON records(stream, entry_key);
                CREATE TABLE IF NOT EXISTS objects (namespace TEXT, id TEXT, value TEXT,
                    used INTEGER, macro_stop INTEGER, identity TEXT, PRIMARY KEY(namespace,id));
                CREATE INDEX IF NOT EXISTS object_identity ON objects(namespace,identity);
            """)
            for key, value in (("run_id", run_id), ("binding_id", binding_id)):
                self.connection.execute("INSERT OR IGNORE INTO metadata VALUES (?,?)", (key, value))
            columns = {row[1] for row in self.connection.execute("PRAGMA table_info(records)")}
            if "raw_sha256" not in columns:
                self.connection.execute("ALTER TABLE records ADD COLUMN raw_sha256 TEXT")
                # A legacy archive has no append-only sidecars, but retaining
                # per-row integrity makes a later migrated stream tamper
                # evident without decoding its logical observation values.
                for stream, sequence, raw in self.connection.execute(
                        "SELECT stream,seq,value FROM records WHERE raw_sha256 IS NULL"):
                    self.connection.execute(
                        "UPDATE records SET raw_sha256=? WHERE stream=? AND seq=?",
                        (hashlib.sha256(raw.encode("utf-8")).hexdigest(), stream, sequence),
                    )
            self.connection.commit()
        metadata = dict(self.connection.execute("SELECT key,value FROM metadata"))
        if metadata != {"run_id": run_id, "binding_id": binding_id}:
            self.connection.close()
            raise ValueError("archive_run_or_binding_mismatch")

    def close(self):
        self.connection.close()

    def __del__(self):
        connection = getattr(self, "connection", None)
        if connection is not None:
            connection.close()

    def sequence(self):
        stream = uuid.uuid4().hex
        with self.connection:
            self.connection.execute("INSERT INTO streams VALUES (?,0)", (stream,))
        # Keep two append-only, index-addressed serializations beside the
        # database rows.  They let final export and canonical reference
        # hashing stream the original records without decoding every archived
        # observation at terminalization time.  The SQLite records remain the
        # authoritative indexed retrieval store.
        ArchiveSequence(self, stream)._ensure_sidecars()
        return ArchiveSequence(self, stream)

    def derived(self, purpose):
        """Own derived evidence separately, including when sources are read-only."""
        identity = purpose + "-" + uuid.uuid4().hex
        directory = self.path.parent / identity
        directory.mkdir()
        return Archive(directory / "cockpit-evidence.sqlite",
                       run_id=identity, binding_id=identity)

    def pack(self, value):
        # Tag every container internally so native dictionaries cannot forge
        # sequence references by resembling an implementation marker.
        if isinstance(value, ArchiveSequence):
            if value.archive.path != self.path:
                copied = self.sequence()
                for item in value:
                    copied.append(item)
                value = copied
            return {"s": [value.stream, len(value)]}
        if isinstance(value, Mapping):
            return {"m": {key: self.pack(item) for key, item in value.items()}}
        if isinstance(value, (list, tuple)):
            return {"l": [self.pack(item) for item in value]}
        if isinstance(value, set):
            return {"t": [self.pack(item) for item in sorted(value)]}
        return value

    def unpack(self, value):
        if not isinstance(value, dict):
            return value
        if set(value) == {"s"}:
            return ArchiveSequence(self, value["s"][0], count=value["s"][1])
        if set(value) == {"m"}:
            return {key: self.unpack(item) for key, item in value["m"].items()}
        if set(value) == {"l"}:
            return [self.unpack(item) for item in value["l"]]
        if set(value) == {"t"}:
            return {self.unpack(item) for item in value["t"]}
        raise ValueError("archive_container_corrupt")

    def encode(self, value):
        return json.dumps(self.pack(value), ensure_ascii=False, separators=(",", ":"))

    def decode(self, value):
        return self.unpack(json.loads(value))

    def wire(self, response, *, exported=None):
        references = []
        def project(value, path):
            if isinstance(value, ArchiveSequence):
                reference = value.reference()
                references.append({"path": path, "reference": reference})
                return {"schema": "caol-archive-sequence-ref-v1", **reference}
            if isinstance(value, Mapping):
                return {key: project(item, [*path, key]) for key, item in value.items()}
            if isinstance(value, (list, tuple)):
                return [project(item, [*path, index]) for index, item in enumerate(value)]
            return value
        result = project(response, [])
        if references:
            # A live response only needs an authenticated wire skeleton: the
            # archive references below carry the lazy history and their
            # per-stream digests protect retrieval.  Exporting ``response``
            # here used to materialize every archived row once for each
            # transport reply (run.witness, run.finish, and the bridge's
            # final response).  Report sidecars still pass ``exported`` and
            # retain the v1 full-logical-export contract.
            wire_schema = "caol-live-artifact-reference-v1"
            if exported is None:
                exported = write_json_stream(
                    self.path.parent / ("response-export-" + uuid.uuid4().hex + ".json"),
                    result,
                )
                wire_schema = "caol-live-artifact-reference-v2"
            result["artifact_reference_envelope"] = {
                "schema": wire_schema, "references": references,
                "exported_response": exported, "run_id": self.run_id, "binding_id": self.binding_id}
        return result


class ArchiveSequence(Sequence):
    def __init__(self, archive, stream, count=None):
        self.archive, self.stream, self.count = archive, stream, count

    def __len__(self):
        if self.count is not None:
            return self.count
        row = self.archive.connection.execute("SELECT count FROM streams WHERE id=?", (self.stream,)).fetchone()
        if row is None:
            raise ValueError("archive_sequence_missing")
        return row[0]

    def _sidecar_path(self, *, sort_keys: bool) -> Path:
        flavor = "canonical" if sort_keys else "export"
        return self.archive.path.parent / (
            self.archive.path.stem + ".stream-" + self.stream + "." + flavor + ".jsonl"
        )

    def _ensure_sidecars(self) -> None:
        if self.archive.readonly:
            return
        for sort_keys in (False, True):
            self._sidecar_path(sort_keys=sort_keys).touch(exist_ok=True)

    def _append_sidecars(self, value, sequence: int) -> None:
        """Append one record in both exact JSON orders without a history scan."""
        for sort_keys in (False, True):
            path = self._sidecar_path(sort_keys=sort_keys)
            with path.open("a", encoding="utf-8") as stream:
                if sequence:
                    stream.write(",")
                for chunk in json_chunks(value, sort_keys=sort_keys):
                    stream.write(chunk)

    def _rebuild_sidecars(self) -> None:
        if self.archive.readonly:
            return
        for sort_keys in (False, True):
            path = self._sidecar_path(sort_keys=sort_keys)
            temporary = path.with_suffix(path.suffix + ".tmp")
            with temporary.open("w", encoding="utf-8") as stream:
                for index, item in enumerate(self):
                    if index:
                        stream.write(",")
                    for chunk in json_chunks(item, sort_keys=sort_keys):
                        stream.write(chunk)
            temporary.replace(path)

    def json_chunks(self, *, sort_keys=False):
        """Emit a complete JSON array from its append-only sidecar when safe.

        Older archives have no sidecar and retain the legacy indexed decode
        path.  New archives avoid terminal full-history decode churn while
        retaining byte-for-byte logical JSON and indexed retrieval.
        """
        path = self._sidecar_path(sort_keys=sort_keys)
        if path.is_file():
            if not self._sidecar_records_intact():
                raise ValueError("archive_sequence_gap_or_truncated_digest_mismatch")
            yield "["
            with path.open("r", encoding="utf-8") as stream:
                while True:
                    chunk = stream.read(64 * 1024)
                    if not chunk:
                        break
                    yield chunk
            yield "]"
            return
        yield "["
        for index, item in enumerate(self):
            if index:
                yield ","
            yield from json_chunks(item, sort_keys=sort_keys)
        yield "]"

    def _sidecar_records_intact(self) -> bool:
        """Validate compact indexed row hashes before trusting a sidecar.

        The check streams stored JSON text rather than decoding its nested
        maps.  It catches direct SQLite mutation while avoiding the former
        full logical action/observation materialization at final export.
        """
        count = 0
        cursor = self.archive.connection.execute(
            "SELECT seq,value,raw_sha256 FROM records WHERE stream=? AND seq<? ORDER BY seq",
            (self.stream, len(self)),
        )
        for sequence, raw, expected in cursor:
            if sequence != count or not isinstance(expected, str) or \
                    hashlib.sha256(raw.encode("utf-8")).hexdigest() != expected:
                return False
            count += 1
        return count == len(self)

    def __iter__(self):
        cursor = self.archive.connection.execute(
            "SELECT seq,value FROM records WHERE stream=? AND seq<? ORDER BY seq", (self.stream, len(self)))
        count = 0
        for sequence, raw in cursor:
            if sequence != count:
                raise ValueError("archive_sequence_gap")
            count += 1
            yield self.archive.decode(raw)
        if count != len(self):
            raise ValueError("archive_sequence_truncated")

    def __getitem__(self, index):
        if isinstance(index, slice):
            return [self[item] for item in range(*index.indices(len(self))) ]
        if index < 0:
            index += len(self)
        if index < 0 or index >= len(self):
            raise IndexError(index)
        row = self.archive.connection.execute("SELECT value FROM records WHERE stream=? AND seq=?",
                                              (self.stream, index)).fetchone()
        if row is None:
            raise ValueError("archive_sequence_gap")
        return self.archive.decode(row[0])

    def append(self, value):
        if self.count is not None:
            raise ValueError("archive_snapshot_is_immutable")
        raw = self.archive.encode(value)
        raw_sha256 = hashlib.sha256(raw.encode("utf-8")).hexdigest()
        key = value.get("citation_id") if isinstance(value, Mapping) else None
        kind = value.get("kind") if isinstance(value, Mapping) else None
        sequence = len(self)
        # Serialize the current record once, at append time.  The result is
        # bounded by one action/observation rather than every record in the
        # terminal transcript; SQLite remains committed before it becomes
        # visible through the stream count.
        self._append_sidecars(value, sequence)
        with self.archive.connection:
            self.archive.connection.execute(
                "INSERT INTO records (stream,seq,value,entry_key,kind,raw_sha256) VALUES (?,?,?,?,?,?)",
                (self.stream, sequence, raw, key, kind, raw_sha256),
            )
            self.archive.connection.execute("UPDATE streams SET count=count+1 WHERE id=?", (self.stream,))

    def __delitem__(self, index):
        if not isinstance(index, slice) or index.stop is not None or index.step is not None or self.count is not None:
            raise ValueError("only_unpublished_tail_rollback_is_supported")
        start = index.start or 0
        if start < 0:
            start += len(self)
        with self.archive.connection:
            self.archive.connection.execute("DELETE FROM records WHERE stream=? AND seq>=?", (self.stream, start))
            self.archive.connection.execute("UPDATE streams SET count=? WHERE id=?", (start, self.stream))
        self._rebuild_sidecars()

    def lookup(self, key):
        row = self.archive.connection.execute(
            "SELECT value FROM records WHERE stream=? AND entry_key=? AND seq<?", (self.stream, key, len(self))).fetchone()
        return self.archive.decode(row[0]) if row else None

    def reference(self):
        digest = hashlib.sha256()
        size = 0
        for chunk in self.json_chunks(sort_keys=True):
            raw = chunk.encode("utf-8")
            digest.update(raw)
            size += len(raw)
        return {"archive_path": str(self.archive.path), "stream": self.stream, "count": len(self),
                "sha256": digest.hexdigest(), "json_bytes": size,
                "run_id": self.archive.run_id, "binding_id": self.archive.binding_id}

    @property
    def json_bytes(self):
        return self.reference()["json_bytes"]

    def __eq__(self, other):
        return is_sequence(other) and len(self) == len(other) and all(left == right for left, right in zip(self, other))


class _Record(dict):
    def __init__(self, value, changed):
        super().__init__(value)
        self.changed = changed

    def __setitem__(self, key, value):
        super().__setitem__(key, value)
        if key in {"used", "macro_stop_decision"}:
            self.changed(key, value)


class ArchiveMap(MutableMapping):
    """One hot object; old objects and mutable authority flags stay indexed."""
    def __init__(self, archive, namespace):
        self.archive, self.namespace = archive, namespace
        self.hot_key, self.hot_value = None, None

    def __setitem__(self, key, value):
        with self.archive.connection:
            self.archive.connection.execute("INSERT OR REPLACE INTO objects VALUES (?,?,?,?,?,?)", (
                self.namespace, key, self.archive.encode(value), bool(value.get("used")),
                bool(value.get("macro_stop_decision")), value.get("identity")))
        self.hot_key, self.hot_value = None, None
        self.hot_value = self[key]
        self.hot_key = key

    def __getitem__(self, key):
        if key == self.hot_key:
            return self.hot_value
        row = self.archive.connection.execute("SELECT value,used,macro_stop FROM objects WHERE namespace=? AND id=?",
                                              (self.namespace, key)).fetchone()
        if row is None:
            raise KeyError(key)
        value = self.archive.decode(row[0])
        for flag, stored in (("used", row[1]), ("macro_stop_decision", row[2])):
            if flag in value or self.namespace == "observations":
                value[flag] = bool(stored)
        def changed(flag, updated):
            column = "used" if flag == "used" else "macro_stop"
            with self.archive.connection:
                # Observation authority is single-use.  Its full public
                # result remains in the ordered transcript, so retaining the
                # native frame here after consumption only duplicates a large
                # live response for the rest of the session.  Keep a small
                # indexed tombstone so replay attempts still fail closed.
                if self.namespace == "observations" and flag == "used" and bool(updated):
                    compact = {
                        name: value[name] for name in (
                            "run_id", "actions", "action_stable_ids", "handles",
                            "native_interruption", "macro_stop_decision",
                        ) if name in value
                    }
                    compact["used"] = True
                    self.archive.connection.execute(
                        "UPDATE objects SET value=?,used=? WHERE namespace=? AND id=?",
                        (self.archive.encode(compact), True, self.namespace, key),
                    )
                    if self.hot_key == key:
                        self.hot_value = _Record(compact, changed)
                else:
                    self.archive.connection.execute(f"UPDATE objects SET {column}=? WHERE namespace=? AND id=?",
                                                     (bool(updated), self.namespace, key))
        return _Record(value, changed)

    def __iter__(self):
        for row in self.archive.connection.execute("SELECT id FROM objects WHERE namespace=?", (self.namespace,)):
            yield row[0]

    def __len__(self):
        return self.archive.connection.execute("SELECT count(*) FROM objects WHERE namespace=?", (self.namespace,)).fetchone()[0]

    def __delitem__(self, key):
        with self.archive.connection:
            self.archive.connection.execute("DELETE FROM objects WHERE namespace=? AND id=?", (self.namespace, key))
        if self.hot_key == key:
            self.hot_key, self.hot_value = None, None

    def clear(self):
        with self.archive.connection:
            self.archive.connection.execute("DELETE FROM objects WHERE namespace=?", (self.namespace,))
        self.hot_key, self.hot_value = None, None

    def revoke_all(self):
        with self.archive.connection:
            self.archive.connection.execute("UPDATE objects SET used=1 WHERE namespace=?", (self.namespace,))
        self.hot_key, self.hot_value = None, None

    def find_identity(self, identity):
        row = self.archive.connection.execute("SELECT id FROM objects WHERE namespace=? AND identity=?",
                                              (self.namespace, identity)).fetchone()
        return row[0] if row else None


def file_identity(path):
    path = Path(path).resolve()
    digest, size = hashlib.sha256(), 0
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
            size += len(chunk)
    return {"path": str(path), "sha256": digest.hexdigest(), "bytes": size}


def resolve_wire(response, *, directory, binding_id, exported_path=None):
    envelope = response.get("artifact_reference_envelope")
    if envelope is None:
        return response
    schema = envelope.get("schema")
    if schema not in {"caol-live-artifact-reference-v1", "caol-live-artifact-reference-v2"} or \
            envelope.get("binding_id") != binding_id:
        raise ValueError("archive_envelope_binding_mismatch")
    exported = envelope.get("exported_response")
    if not isinstance(exported, Mapping):
        raise ValueError("archive_export_identity_missing")
    actual_path = Path(exported["path"]).resolve()
    if exported_path is not None:
        allowed = actual_path == Path(exported_path).resolve()
    else:
        allowed = (actual_path.parent == Path(directory).resolve() and
                   actual_path.name.startswith("response-export-") and actual_path.suffix == ".json")
    if not allowed:
        raise ValueError("archive_export_path_invalid")
    if file_identity(actual_path) != exported:
        raise ValueError("archive_export_identity_mismatch")
    result = dict(response)
    result.pop("artifact_reference_envelope")
    if schema == "caol-live-artifact-reference-v2":
        # v2 authenticates the compact wire skeleton itself.  The archive
        # references are checked below before their placeholders are replaced
        # with lazy sequences, so no full history needs to be decoded merely
        # to validate a transport response.
        digest, size = hashlib.sha256(), 0
        for chunk in json_chunks(result):
            raw = chunk.encode("utf-8")
            digest.update(raw)
            size += len(raw)
        if digest.hexdigest() != exported["sha256"] or size != exported["bytes"]:
            raise ValueError("archive_export_reconstruction_mismatch")
    archive = None
    try:
        for item in envelope["references"]:
            reference = item["reference"]
            path = Path(reference["archive_path"]).resolve()
            if path.parent != Path(directory).resolve() or path.name != "cockpit-evidence.sqlite" or reference["binding_id"] != binding_id:
                raise ValueError("archive_reference_path_or_binding_invalid")
            if reference["run_id"] != envelope["run_id"]:
                raise ValueError("archive_reference_run_mismatch")
            if archive is None:
                archive = Archive(path, run_id=reference["run_id"], binding_id=binding_id, readonly=True)
            sequence = ArchiveSequence(archive, reference["stream"], count=reference["count"])
            if sequence.reference() != reference:
                raise ValueError("archive_reference_digest_mismatch")
            parent = result
            for part in item["path"][:-1]:
                parent = parent[part]
            final = item["path"][-1]
            if parent[final] != {"schema": "caol-archive-sequence-ref-v1", **reference}:
                raise ValueError("archive_reference_placeholder_mismatch")
            parent[final] = sequence
        if schema == "caol-live-artifact-reference-v1":
            digest, size = hashlib.sha256(), 0
            for chunk in json_chunks(result):
                raw = chunk.encode("utf-8")
                digest.update(raw)
                size += len(raw)
            if digest.hexdigest() != exported["sha256"] or size != exported["bytes"]:
                raise ValueError("archive_export_reconstruction_mismatch")
    except Exception as error:
        if archive is not None:
            archive.close()
        if isinstance(error, sqlite3.Error):
            raise ValueError("archive_database_unreadable: " + str(error)) from error
        raise
    return result
