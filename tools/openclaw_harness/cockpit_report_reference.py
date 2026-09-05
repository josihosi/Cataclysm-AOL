"""Verified lazy views of unchanged full cockpit report artifacts.

The sidecar is an index, never independent proof. Its fully reconstructed JSON
must have the exact byte hash and size of the original report before any caller
receives the view. Reports without a sidecar retain the legacy JSON path.
"""
from __future__ import annotations

import json
import hashlib
from pathlib import Path
from collections.abc import Mapping

from cockpit_archive import Archive, find_archive, resolve_wire, write_json_stream, file_identity, value_digest


def write_report_reference(path, value):
    archive = find_archive(value)
    if archive is None:
        return
    identity = file_identity(path)
    wire = archive.wire(value, exported=identity)
    write_json_stream(Path(path).with_suffix(".ref.json"), wire, exclusive=False)


def load_report(path):
    """Return canonical path, unchanged byte SHA, and the complete logical value."""
    path = Path(path).resolve()
    reference_path = path.with_suffix(".ref.json")
    if not reference_path.is_file():
        raw = path.read_bytes()
        identity = {"path": str(path), "sha256": hashlib.sha256(raw).hexdigest(), "bytes": len(raw)}
        value = json.loads(raw.decode("utf-8"))
    else:
        identity = file_identity(path)
        wire = json.loads(reference_path.read_text(encoding="utf-8"))
        if not isinstance(wire, Mapping):
            raise ValueError("report_reference_is_not_an_object")
        envelope = wire.get("artifact_reference_envelope")
        if not isinstance(envelope, Mapping) or envelope.get("exported_response") != identity:
            raise ValueError("report_reference_export_identity_mismatch")
        references = envelope.get("references")
        if not isinstance(references, list) or not references:
            raise ValueError("report_reference_has_no_archive")
        archive_directory = Path(references[0]["reference"]["archive_path"]).resolve().parent
        value = resolve_wire(wire, directory=archive_directory,
                             binding_id=envelope["binding_id"], exported_path=path)
    if not isinstance(value, dict):
        raise ValueError("Report top level must be an object")
    return str(path), identity["sha256"], value


def journal_reference(*, report_path, report_sha256, selector, journal):
    return {"schema": "caol-playtest-journal-reference-v1",
            "report_path": str(Path(report_path).resolve()),
            "report_sha256": report_sha256, "selector": list(selector),
            "journal_sha256": journal["journal_sha256"]}


def load_stored_journal(value):
    """Support old inline journal rows and explicit new immutable references."""
    if value.get("schema") != "caol-playtest-journal-reference-v1":
        return value
    _, digest, report = load_report(value["report_path"])
    if digest != value["report_sha256"]:
        raise ValueError("stored_journal_report_identity_mismatch")
    journal = report
    for part in value["selector"]:
        journal = journal[part]
    if not isinstance(journal, Mapping) or journal.get("journal_sha256") != value["journal_sha256"]:
        raise ValueError("stored_journal_identity_mismatch")
    return journal


def store_journal_reference(journal):
    """Retain exact archived journal values without copying them into SQLite TEXT."""
    archive = find_archive(journal)
    if archive is None:
        return journal
    # A journal may compose several independently bound reports, so retain its
    # own complete canonical artifact instead of assuming one report contains it.
    digest = str(journal["journal_sha256"])
    if len(digest) != 64 or any(char not in "0123456789abcdef" for char in digest):
        raise ValueError("stored_journal_digest_invalid")
    path = archive.path.parent / ("witness-journal-" + digest + ".json")
    if not path.exists():
        directory = archive.path.parent / ("witness-journal-" + digest + ".archive")
        directory.mkdir(exist_ok=True)
        storage = Archive(directory / "cockpit-evidence.sqlite",
                          run_id="journal:" + digest, binding_id="journal:" + digest)
        try:
            # Copy each entry independently; nested macro receipt sequences are
            # copied by Archive.pack into this same owner. Native identities are
            # ordinary exact values and are never replaced by storage identities.
            entries = storage.sequence()
            for entry in journal["entries"]:
                entries.append(entry)
            retained = storage.decode(storage.encode({**journal, "entries": entries}))
            write_json_stream(path, retained)
            write_report_reference(path, retained)
        finally:
            storage.close()
    _, _, existing = load_report(path)
    if value_digest("stored-journal", existing) != value_digest("stored-journal", journal):
        raise ValueError("stored_journal_existing_artifact_mismatch")
    return journal_reference(report_path=path, report_sha256=file_identity(path)["sha256"],
                             selector=[], journal=journal)
