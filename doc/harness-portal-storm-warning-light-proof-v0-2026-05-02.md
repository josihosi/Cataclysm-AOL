# CAOL-HARNESS-PORTAL-STORM-WARNING-LIGHT-v0 — proof/readout (2026-05-02)

Status: CHECKPOINTED GREEN V0 / HARNESS-HARDENING

Contract: `doc/harness-portal-storm-warning-light-packet-v0-2026-05-02.md`.

## What changed

`tools/openclaw_harness/startup_harness.py` now emits a reviewer-visible `portal_storm_warning` object for probe/handoff reports and repeatability summaries.

The warning reads saved dimension weather metadata via the existing `dimension_data.gsav` weather audit path and recognizes `portal_storm`, `early_portal_storm`, `distant_portal_storm`, `close_portal_storm`, and `WEATHER_PORTAL_STORM`/equivalent ids.

Scenario authors can opt in with any of:
- `allow_portal_storm: true` / `portal_storm_allowed: true`
- `require_portal_storm: true` / `portal_storm_required: true`
- `portal_storm: { "allow": true }`
- `portal_storm: { "require": true }`

Required portal storm implies allowed portal storm.

## Report/readout behavior

When a non-opted-in scenario observes portal-storm weather, reports now include:

`⚠ PORTAL STORM ACTIVE / HARNESS RESULT MAY BE CONTAMINATED`

and the step ledger gains a synthetic warning row with verdict:

`yellow_step_portal_storm_contamination`

That row makes `step_ledger_summary.status` become `yellow_step_local_proof_incomplete`, preventing silent green feature proof from a contaminated run.

When the scenario explicitly allows portal storms, the warning remains visible but the row is green:

`green_step_portal_storm_expected_allowed`

When the scenario requires portal storm weather and it is missing/unknown, the row is red:

`red_step_portal_storm_required_missing`

## Evidence

Narrow gate:

```sh
python3 tools/openclaw_harness/proof_classification_unit_test.py
python3 -m py_compile tools/openclaw_harness/startup_harness.py tools/openclaw_harness/proof_classification_unit_test.py
git diff --check
```

Result:

- unit suite: `Ran 13 tests ... OK`
- py_compile: pass
- diff check: pass

Covered rows in `tools/openclaw_harness/proof_classification_unit_test.py`:

- portal-storm positive saved-weather detection from synthetic `dimension_data.gsav` with `weather_id = portal_storm`;
- normal-weather negative control with `weather_id = clear`;
- unallowed portal-storm contamination adds `yellow_step_portal_storm_contamination`, makes the ledger yellow, and prevents feature proof;
- allowed portal-storm scenario path keeps the ledger green with `green_step_portal_storm_expected_allowed` while retaining visible warning text;
- repeatability text report includes `portal_storm=active` and the contamination warning summary.

## Interpretation rule for Andi/Schani

If a normal scenario reports `portal_storm_warning.classification = contaminating`, treat the run as harness-contaminated and rerun under controlled non-portal-storm weather unless the scenario intentionally tests portal storms.

If `classification = expected_allowed`, the portal storm is intentional/allowed and does not by itself invalidate the row.

If `classification = required_missing` or `required_unknown`, the intentional portal-storm scenario did not prove its required weather state and must not close green.
