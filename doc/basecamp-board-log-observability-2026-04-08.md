# Basecamp board/log leak + observability contract (2026-04-08)

Status: required current-stack support task, not a future nice-to-have

This document packages two tightly related things that should stay narrow:

1. preserve the already-landed assigned-camp board baseline by stopping raw structured board/job payload text from leaking into the player-facing in-game message log
2. make the probe packet capture enough side-by-side evidence to show whether the internal structured call is correct and whether the in-game text still looks organic

## Trigger

Josef caught a live screenshot where the in-game message log showed raw structured payload text such as:
- `board=show_board`
- `details=show_job=1`
- `query=bandages`
- `status=blocked`
- `approval=waiting_player`
- `blockers=...`
- `next=job=1`

That is not acceptable player-facing output.

## Problem framing

Treat this as a preserved-baseline regression on the assigned-camp `show_board` path.

Do **not** widen the framing into:
- a general follower-NPC rewrite
- a general speech-system redesign
- a broad parser/LLM architecture rethink

Josef's explicit guardrail:
- ordinary follower NPC behavior should remain untouched unless the leak can actually be proven to originate there

## Required current bug task

### Goal
Stop raw structured board/job payload text from appearing in the ordinary in-game message log.

### Acceptance
- assigned-camp board/job routing still works on the real McWilliams path
- the internal structured call/log path can still carry the board/job token honestly
- the player-facing in-game message log shows only organic/immersive text rather than key=value machine payload text
- the fix does not casually change ordinary follower-NPC behavior

## Required observability subtask

Whenever the probe packet captures `llm_intent.log`, it must also capture the last few in-game message-log lines.

### Why
Andi needs to compare:
- **player-facing output**: does the in-game message log look organic, immersive, and complete?
- **internal call/log path**: does `llm_intent.log` show the correct structured call/token/payload?

Without both, the probe can lie by proving only the internal path while missing a user-facing leak.

### Minimum evidence shape
- screenshot of the in-game message log when useful
- last few in-game log lines captured as text/artifact
- relevant `llm_intent.log` excerpt for the same moment
- brief comparison note explaining whether the two paths match the intended split

## Scope discipline

- narrow board/basecamp path only
- preserve the already-landed assigned-camp routing baseline
- no boat-rocking follower changes
- no broad harness empire-building beyond what is needed to prove this path honestly
