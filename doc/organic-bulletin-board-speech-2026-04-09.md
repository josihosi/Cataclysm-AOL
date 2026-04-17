# Organic bulletin-board speech polish (2026-04-09)

Status: parked future polish item, not part of the current freeze-closeout packet

This packages Josef's concern about the surviving `job=1`-style follow-up wording on the bulletin-board / camp-job path.

## Problem

Even after the raw structured payload leak was fixed, one aesthetic seam still remains:
- internal routing tokens such as `job=1` are not acceptable as ordinary in-world speech
- they can exist internally for routing/debugging, but they should not surface as player-facing spoken output unless someone is explicitly in a debug path

## Desired player-facing behavior

### Organic triggers
Board/job interaction should be reachable through ordinary poor-survivor speech, not machine phrases.

Examples of the desired trigger shape:
- "craft"
- "got any craft work?"
- "what needs making?"
- "show me what needs doing"
- similar natural requests embedded in a sentence

The exact parser contract can stay structured internally, but the player-facing trigger surface should feel like normal camp talk.

### Organic answers
The answer should sound like poor people in a dump making it work for another day while the dead and worse roam outside.

Meaning:
- rough, practical, tired, in-world phrasing
- no exposed routing ids like `job=1`
- no exposed internal terms like `show_board`, `show_job`, `details=`
- no machine key=value residue in ordinary speech

## Internal/visible split rule

- internal structured routing may still use board/job tokens when useful
- debug tooling and logs may still expose that structure
- ordinary in-game speech should not expose it

## Acceptance

- board/job requests can be triggered through natural player-facing phrasing
- visible NPC answers sound organic and in-world
- no `job=<id>` / `show_board` / `show_job` tokens appear in ordinary speech
- internal routing/debug paths remain available where needed

## Scope discipline

This is a polish/future-lane item, not a reason to reopen the current freeze-closeout packet unless the visible wording becomes a release blocker.
