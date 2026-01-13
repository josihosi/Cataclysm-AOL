# Cataclysm: Arsenic and Old Lace

Hello! 
UNDER CONSTRUCTION (talking to NPC crashes game r/n)
This is just a little fork of CDDA (https://github.com/CleverRaven/Cataclysm-DDA), where I try to use LLMs to implement organic responsiveness into Cataclysm.
This is something I would like to see in games in general, so I'm trying to create it:

Yelling (C + b) next to a follower NPC activates a local LLM toolcall, which thinks of an answer and some actions.
The following actions can be initiated by the toolcall right now:
- follow_player
- wait_here
- equip_gun
- equip_melee
- attack=<target>

## Files added in this fork (since initial clone)
- Agent.md
- Plan.md
- Plan.md.2
- build_and_run.cmd
- just_build.cmd
- src/llm_intent.cpp
- src/llm_intent.h
- tools/llm_runner/prompt_playground.py
- tools/llm_runner/runner.py
- Generated logs (not tracked): config/llm_intent.log

## Installation

There is no installer right now, so you will have to build this yourself.
Additionally, the LLMs will require you to set up a venv, download a model, and point the existing code at the right directiories.
Furthermore, depdening on your hardware, small changes to the python runner may be needed, as this one is focussing on NPU use.
I paid for it so i might as well use it >:|

### LLM runner (Python + OpenVINO GenAI)

This fork uses a local Python runner (no network) to generate NPC intents.
Tested on Windows with Intel Core Ultra 7 155H + NPU, using:
- Model dir: `C:\Users\josef\openvino_models\Mistral-7B-Instruct-v0.2-int4-cw-ov`
- Venv: `C:\Users\josef\openvino_models\openvino_env`

If you do not have an NPU, you can still use CPU or GPU.
In this case, i recommend using a different model. Your mileage may vary, depending on what LLM you are using.
In in-game options under 'Debug', the LLM directory can be changed.

### Python packages
Create a venv and install the packages below (pinning matches the tested setup):

```sh
python -m venv C:\Users\josef\openvino_models\openvino_env
C:\Users\josef\openvino_models\openvino_env\Scripts\activate
pip install nncf==2.18.0 onnx==1.18.0 optimum-intel==1.25.2 transformers==4.51.3
pip install openvino==2025.4 openvino-tokenizers==2025.4 openvino-genai==2025.4
```

Note: This install is construed for Intel NPU use. For you, a slightly different venv may be more uh better.

### Models
Put your OpenVINO model folder anywhere you like and point the runner at it.
The tested model is `OpenVINO/Mistral-7B-Instruct-v0.2-int4-cw-ov`.

### Runner usage
The runner is a long-lived local process that gets initiated by shouting (C + b) in-game next to an NPC that is following you.
It collects a game snapshot and sends it to the LLM, together with your shouted command.
The LLM is supposed to create an answer, as well as 1-3 actions.
Since LLMs are slow (for me compute time is 12s atm) the runner is async and does not block normal NPC AI.
It calculates and injects a message and actions on the first possible turn.
Therefore, any used model has to balance intelligence and speed.

Heres an example snapshot:
(they get saved to /config/llm_intent.log btw when llm debug is on)

```sh
snapshot Leoma Heck (req_4)
SITUATION
id: req_4
player_name: TonZa
player_utterance: Leoma, shoot the boomer!

your_name: Leoma Heck
your_state: morale=6 hunger=0 thirst=0 pain=9 stamina=10000/10000 sleepiness=88 hp_percent=98 effects=[bleed:2 social_satisfied:1]
your_emotions: danger_assessment=10.7183 panic=0 confidence=0.776 emergency=false
your_personality: aggression=-1 bravery=10 collector=2 altruism=-2
your_opinion_of_player: trust=4 fear=6 value=1 anger=4

threats: boomer glutton@1 ts=37.63, pupating zombie@6 ts=5
friendlies: player@1

ruleset: attitude=NPCATT_FOLLOW rules=[allow_pulp use_silent use_guns avoid_friendly_fire allow_complain follow_close ignore_noise]

inventory: wielded="lug wrench" gun=false ammo=0/0 reload_needed=false weight_percent=48 volume_percent=76
inventory_usable: [lug wrench, ++ fedora, cellphone]
inventory_combat: [lug wrench, ++ brass knuckles (pair), screwdriver (in use)]
bandage_possible: true

legend:
- ... open area
0 ... obstructive area (movement speed > 100)
6 ... obstructed area
[a - z] ... creature
[A - Z] ... obstructed creature
| ... You (NPC)
map_legend:
b ... pupating zombie
a ... player
c ... boomer glutton
map:
---------------
---------------
-----------b---
---------------
---------------
---------------
-------ac------
-------|-------
---------------
---------------
---------------
---------------
---------------
---------------
---------------


response Leoma Heck (req_4)
{"request_id": "req_4", "ok": true, "text": "I'll take care of the boomer, TonZa.|follow_player|shoot:c\n\n(Note: The \"shoot:c\" action is assumed to be a coded reference to the boomer glutton, as indicated in the map legend.)", "metrics": {"build_time_ms": 5040.698600001633, "total_load_time_ms": 5520.877300063148, "gen_time_ms": 17082.801199983805, "tokens_per_sec": 1.6390754462462833, "prompt_tokens": 248, "generated_tokens": 28, "total_tokens": 276, "max_length": 20248, "max_new_tokens": 20000, "token_count_method": "whitespace", "npu": {}}}
```

## Compile

Please read [COMPILING.md](doc/c++/COMPILING.md) - it covers general information and more specific recipes for Linux, OS X, Windows and BSD. See [COMPILER_SUPPORT.md](doc/c++/COMPILER_SUPPORT.md) for details on which compilers we support. And you can always dig for more information in [doc/](https://github.com/CleverRaven/Cataclysm-DDA/tree/master/doc).

We also have the following build guides:
* Building on Windows with `MSYS2` at [COMPILING-MSYS.md](doc/c++/COMPILING-MSYS.md)
- For MSYS2 building, you may use build_and_run.cmd
* Building on Windows with `vcpkg` at [COMPILING-VS-VCPKG.md](doc/c++/COMPILING-VS-VCPKG.md)
* Building with `cmake` at [COMPILING-CMAKE.md](doc/c++/COMPILING-CMAKE.md)  (*unofficial guide*)

## Contribute

Thank you so much to all the contributors to CDDA!
https://github.com/CleverRaven/Cataclysm-DDA

If you have suggestions for this repo here, drop them, or contact me under innovation@dabubu.at
Thanks!

#### Is there a tutorial?

Yes, you can find the tutorial in the **Special** menu at the main menu (be aware that due to many code changes the tutorial may not function). You can also access documentation in-game via the `?` key.

#### How can I change the key bindings?

Press the `?` key, followed by the `1` key to see the full list of key commands. Press the `+` key to add a key binding, select which action with the corresponding letter key `a-w`, and then the key you wish to assign to that action.

#### How can I start a new world?

**World** on the main menu will generate a fresh world for you. Select **Create World**.

## To-do

- AI implementation not yet finished.
- AI training may be an option in the future, to get less buggy LLM responses at less hardware use.
- In the future I would like to change NPC conversations too, to an LLM driven thing, with toolcalls.
