# Plan status summary command (2026-04-20)

Status: checkpointed tooling contract

This packages Josef's request for a small read-only command surface that prints current plan categories from `Plan.md` so active, greenlit, and parked work stop feeling like a black box when Andi or Schani reshuffle canon.

## Problem

The roadmap canon currently distinguishes active work, greenlit backlog, and parked concept chains, but the human-facing readout is still mostly manual.
That leaves Josef depending on Schani or Andi to summarize state correctly, which is annoying and easier to mistrust than a direct canon readout.

## Goal

Give Josef a quick trustworthy plan-status readout without making him reread whole `Plan.md` sections or trust hand-waved status summaries.

## Command shape

The intended v1 surface is:
- `/plan active`
- `/plan greenlit`
- `/plan parked`
- optional compact `/plan` that prints all three sections in one reply

## Output contract

- `active` prints the currently active lane or lanes only.
- `greenlit` prints all approved items in execution order.
  - active comes first
  - queued greenlit items follow
  - bottom-of-stack items are **not** split into a separate command; they simply appear last in the numbered greenlit list
- `parked` prints parked items only.
- output stays compact and Discord-friendly rather than dumping whole roadmap prose.
- numbering matters most for the greenlit view and should preserve canon order.
- each item should stay terse and legible, for example: name, status, one-line gist, and canonical doc pointer when useful.

## Source of truth

- Read `Plan.md` only.
- Do not infer status from chat history, memory files, or agent narration when canon disagrees.
- If canon headings or labels are too contradictory or too thin to classify cleanly, warn instead of inventing certainty.

## Scope

- provide a compact plan-status readout for `active`, `greenlit`, and `parked`
- preserve numbered ordering where it carries meaning, especially for `greenlit`
- keep the parser bounded to current canon shape instead of building workflow magic
- make the result easy for Josef to call ad hoc when Andi shifts canon around

## Non-goals

- do not mutate canon or reclassify items from the command path
- do not create a separate bottom-of-stack command
- do not redesign the whole planning workflow or replace the existing Plan/Aux pipeline
- do not let a helper summary become a hidden source of truth over `Plan.md`

## Success state

- [x] A user can request a plan readout and see compact numbered `active`, `greenlit`, and `parked` summaries derived from current `Plan.md` canon.
- [x] The greenlit readout preserves execution order, with active first and any bottom-of-stack entries simply appearing last in that numbered list.
- [x] If canon headings are contradictory or missing enough structure to classify cleanly, the command warns instead of inventing certainty.
- [x] The output stays short and Discord-friendly rather than dumping whole roadmap prose.

## Testing impact

- needed: yes, but still narrow and command-level rather than game-build-level
- landed validation: `python3 tools/plan_status_summary.py --self-test` plus direct sample readouts from current canon via `python3 tools/plan_status_summary.py /plan`, `/plan active`, `/plan greenlit`, and `/plan parked`
- contradictory/thin-canon handling is covered by the built-in self-test cases instead of broad game builds

## Notes on packaging

This greenlight was first normalized through `tools/plan_aux_pipeline_helper.py` in preview/emit mode.
The current helper was useful for schema validation, contract preview, and reviewer-visible snippet emission.
The final canon paste was still manual because the helper's bounded `apply` path only patches known existing headings and does not yet conjure brand-new roadmap or success sections.

The landed first-pass implementation seam is `tools/plan_status_summary.py`.
It stays deliberately small: read `Plan.md`, classify sections from explicit status lines first, fall back to heading keywords only when canon is thin, and print compact summaries without mutating anything.
