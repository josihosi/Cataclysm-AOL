# R-036 correctness and feel closure audit

Independently close `R036-correctness-feel` without launching or replaying gameplay.

The assigned outcome is to verify two things from the retained artifacts: the matched baseline and hostile-ecology feature action windows have the same authoritative mechanical result, and the retained evidence supports no stronger subjective-feel claim than its exact ceiling.

Use checkpoint receipt `58a9b44d38843eb30a2849a80dfe7cce7c30a2906dcc4aa16da431b4b0a0670f` as the compact starting point. Inspect the cited native event streams and image hashes directly. The predecessor was restart-normalized after completing both native movements, and the coordinator recovered its still-running feature PID through advertised semantic save-and-quit and application-quit actions. Both exact PIDs are now absent.

Expected discriminators:

- Baseline run `665b16046551460184abc3da8adddc4b` and feature run `bf61e0502b5b407e816fa3ad16794ed7` should each show accepted `world.move.east` from `[3372,996,0]` to `[3373,996,0]` at game minute `8159`.
- The four retained before/after presentation captures have one identical SHA-256. Static byte-identical images cannot prove redraw timing, smoothness, responsiveness, avatar presentation, or a matched subjective-feel result.
- A worker-authored statement that no stutter was seen is an observation only. Do not promote it beyond the reproducible artifacts.
- OCR, terminal bytes, and rendered text cannot prove accepted input, elapsed time, gameplay state, or a passing checkpoint.

Do not edit production files or DE67 state. Do not launch a game or broker. Preserve unrelated dirty work. Confirm source/artifact identities and exact cleanup only as needed for this narrow audit. Return a compact worker result with a terminal `completed` disposition if the mechanical result and explicit no-subjective-feel ceiling are verified. Return a concrete artifact defect if either conclusion is not supported. Name the first remaining boundary and all evidence limits. The coordinator alone records receipts and closes the gap.
