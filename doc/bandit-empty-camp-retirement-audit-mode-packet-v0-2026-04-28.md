# Bandit empty-camp retirement audit-mode packet v0 (2026-04-28)

Status: IMPLEMENTED / DETERMINISTIC PROOF GREEN

## Why this exists

Josef pointed out a cleanup gap in the live bandit-site ledger: if a hostile camp has no one left at home and no dispatched group still alive outside, it should stop participating in AI calculations. Otherwise an empty camp can keep acting like pressure still exists, wasting compute and inviting weird ghost-camp behavior.

The important constraint is conjunctive: **retire/prune only when both sides are empty**. A camp with an outside dispatch is not empty. A camp with home/inside members is not empty. One missing side is not enough. Na bravo, we are apparently teaching the code the difference between nobody and somebody elsewhere.

## Scope

1. Add or verify a bounded retirement/tombstone rule for hostile camp/site records that have:
   - no at-home / inside-camp live members or spawn-tile home headcount, and
   - no active dispatch/outbound/local-contact group or active member ids.
2. Keep the rule profile/site-safe: bandit camps first, with any broader hostile-site behavior explicit rather than accidental.
3. Preserve non-empty sites:
   - home members but no dispatch -> keep site active;
   - active dispatch/outbound/local-contact members but no home members -> keep site active;
   - unresolved local-contact/returning-home aftermath -> keep site active until resolved.
4. Make the retired state visible to review: logs/debug report should distinguish `retired_empty_site` / equivalent from normal dispatch skip, home-reserve block, or no-signal/no-target.
5. Test it in audit mode: action -> proof, with deterministic and/or live harness evidence that each precondition and each negative case is proven before the retirement action is credited.

## Non-goals

- Do not retire a camp merely because the current dispatch was killed while home members remain.
- Do not retire a camp merely because the camp interior is empty while an active dispatch/outbound/local-contact member still exists.
- Do not silently delete useful history needed for debugging; tombstone/retire is acceptable if complete deletion would hide evidence.
- Do not broaden this into a global hostile-site economy rewrite, multi-camp dogpile rewrite, or writhing-stalker implementation.
- Do not use load-and-close or a raw debug kill command as feature proof. Debug kill is setup/action only until same-run metadata/log/screen proof shows the expected world-state transition.

## Success state

- [x] Deterministic coverage proves the retirement predicate is conjunctive: no home/inside members or spawn-tile home headcount **and** no active dispatch/outside pressure are required.
- [x] Negative coverage proves the site is kept when home members remain, even if there is no active dispatch.
- [x] Negative coverage proves the site is kept when active dispatch/outbound/local-contact members remain, even if no members are at home.
- [x] Negative coverage proves unresolved local-contact/returning-home aftermath keeps the site active until the active side is resolved.
- [x] Positive coverage proves a site with no live home members/spawn-tile headcount and no active dispatch/outside pressure is retired from active AI calculations.
- [x] Review output/logs distinguish empty-site retirement from ordinary dispatch blocking or signal absence via `retired_empty_site`, retirement report text, dispatch block notes, and signal-mark guard behavior.
- [x] No live harness probe was used for closure; deterministic proof is the credited evidence class, so no debug-kill/load-and-close feature proof is claimed.

## Audit-mode playtest shape

A possible live proof is fine, but it must be audit-shaped:

1. Start from a current disposable profile/world with a known owned bandit camp/site.
2. Preflight same-run metadata: site id, home/inside member count, active group id/member ids, spawn-tile headcount, and current runtime commit.
3. Walk/teleport/debug-position to the camp only as setup; record the setup path and camp identity.
4. Debug-kill or otherwise remove all bandits in/inside the camp; prove the post-action home/inside count is zero.
5. For the negative dispatch case, create or preserve an active dispatch/outbound/local-contact member and prove the site does **not** retire.
6. For the positive case, resolve/kill/mark missing the active dispatch too; prove no active group/member ids remain.
7. Advance the exact cadence that runs site maintenance, then prove the site is tombstoned/removed from active AI calculations with a named log/report field.

The live recipe is optional if deterministic coverage plus a narrow saved-state harness primitive proves the same predicate honestly. If live is used, it must not skip the action -> proof ledger just because the debug menu feels convenient.

## Handoff note

Andi should treat this as the next Schani/Josef-named harness-audit seam unless a higher-priority active proof-freeze primitive is already in flight. The phrase to keep nailed to the table: **not one of the two emptiness checks — both**.
