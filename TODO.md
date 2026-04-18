# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active queue: **Hackathon ambient log trigger v0**

Current execution order:
1. run the first real gameplay probe on the branch build:
   - one boring line should do nothing
   - one eyebrow-raising line should provoke ambient NPC chatter
2. check `config/llm_ambient_log_gate.log` after that probe:
   - confirm the tagged trigger line
   - confirm the gate decision
   - confirm whether an ambient request was queued
3. if the gameplay probe is too noisy or too quiet, tune only the narrow demo levers:
   - cooldown
   - burst size
   - boring-line filter
   - prompt wording for the ambient reaction itself

Current chosen gate-model footing:
- chosen demo model:
  - `MoritzLaurer/MiniLM-L6-mnli-binary`
- chosen local NPU artifact:
  - `C:\Users\josef\openvino_models\ambient_gate_minilm_l6_mnli_binary_ov_static64_fixed`
- accepted hackathon tradeoff:
  - a bit worse than the PyTorch reference model
  - but real on the NPU, fast enough, and good enough to keep moving
