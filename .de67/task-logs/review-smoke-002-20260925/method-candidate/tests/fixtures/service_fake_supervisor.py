#!/usr/bin/env python3
"""Long-lived harmless supervisor fixture for the opt-in service integration test."""

from __future__ import annotations

import signal
import time


stopping = False


def stop(_signal: int, _frame: object) -> None:
    global stopping
    stopping = True


signal.signal(signal.SIGTERM, stop)
signal.signal(signal.SIGINT, stop)
while not stopping:
    time.sleep(0.1)
