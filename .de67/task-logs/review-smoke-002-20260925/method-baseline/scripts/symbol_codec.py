#!/usr/bin/env python3
"""Canonical symbol-table codec for compact DE67 policy values."""

from __future__ import annotations

import struct
from typing import Any


NULL = 0
FALSE = 1
TRUE = 2
INTEGER = 3
STRING = 4
LIST = 5
OBJECT = 6


class CodecError(ValueError):
    pass


def _varint(value: int) -> bytes:
    if value < 0:
        raise CodecError("varint cannot encode a negative value")
    output = bytearray()
    while value >= 0x80:
        output.append((value & 0x7F) | 0x80)
        value >>= 7
    output.append(value)
    return bytes(output)


def _read_varint(data: bytes, offset: int) -> tuple[int, int]:
    value = 0
    shift = 0
    while True:
        if offset >= len(data) or shift > 63:
            raise CodecError("invalid varint")
        byte = data[offset]
        offset += 1
        value |= (byte & 0x7F) << shift
        if not byte & 0x80:
            return value, offset
        shift += 7


def _strings(value: Any, found: set[str]) -> None:
    if isinstance(value, str):
        found.add(value)
    elif isinstance(value, list):
        for item in value:
            _strings(item, found)
    elif isinstance(value, dict):
        for key, item in value.items():
            if not isinstance(key, str):
                raise CodecError("object keys must be strings")
            found.add(key)
            _strings(item, found)
    elif value is not None and not isinstance(value, (bool, int)):
        raise CodecError(f"unsupported value type: {type(value).__name__}")


def encode(value: Any) -> bytes:
    found: set[str] = set()
    _strings(value, found)
    strings = sorted(found)
    indexes = {item: index for index, item in enumerate(strings)}
    table = bytearray(_varint(len(strings)))
    for item in strings:
        raw = item.encode("utf-8")
        table.extend(_varint(len(raw)))
        table.extend(raw)

    def emit(item: Any) -> bytes:
        if item is None:
            return bytes([NULL])
        if item is False:
            return bytes([FALSE])
        if item is True:
            return bytes([TRUE])
        if isinstance(item, int):
            zigzag = item * 2 if item >= 0 else (-item * 2) - 1
            return bytes([INTEGER]) + _varint(zigzag)
        if isinstance(item, str):
            return bytes([STRING]) + _varint(indexes[item])
        if isinstance(item, list):
            return bytes([LIST]) + _varint(len(item)) + b"".join(emit(v) for v in item)
        if isinstance(item, dict):
            pairs = sorted(item.items())
            return (
                bytes([OBJECT])
                + _varint(len(pairs))
                + b"".join(_varint(indexes[key]) + emit(v) for key, v in pairs)
            )
        raise CodecError(f"unsupported value type: {type(item).__name__}")

    return bytes(table) + emit(value)


def decode(data: bytes) -> Any:
    count, offset = _read_varint(data, 0)
    strings: list[str] = []
    for _ in range(count):
        size, offset = _read_varint(data, offset)
        end = offset + size
        if end > len(data):
            raise CodecError("truncated string table")
        try:
            strings.append(data[offset:end].decode("utf-8"))
        except UnicodeDecodeError as error:
            raise CodecError("invalid UTF-8 in string table") from error
        offset = end
    if strings != sorted(set(strings)):
        raise CodecError("string table is not canonical")

    def take(position: int) -> tuple[Any, int]:
        if position >= len(data):
            raise CodecError("truncated value")
        opcode = data[position]
        position += 1
        if opcode == NULL:
            return None, position
        if opcode == FALSE:
            return False, position
        if opcode == TRUE:
            return True, position
        if opcode == INTEGER:
            raw, position = _read_varint(data, position)
            return (raw // 2 if raw % 2 == 0 else -(raw // 2) - 1), position
        if opcode == STRING:
            index, position = _read_varint(data, position)
            if index >= len(strings):
                raise CodecError("invalid string reference")
            return strings[index], position
        if opcode == LIST:
            size, position = _read_varint(data, position)
            result = []
            for _ in range(size):
                item, position = take(position)
                result.append(item)
            return result, position
        if opcode == OBJECT:
            size, position = _read_varint(data, position)
            result = {}
            last = -1
            for _ in range(size):
                index, position = _read_varint(data, position)
                if index >= len(strings) or index <= last:
                    raise CodecError("object keys are invalid or noncanonical")
                last = index
                item, position = take(position)
                result[strings[index]] = item
            return result, position
        raise CodecError(f"unknown opcode: {opcode}")

    value, end = take(offset)
    if end != len(data):
        raise CodecError("trailing bytes after value")
    if encode(value) != data:
        raise CodecError("symbol tape is not canonical")
    return value
