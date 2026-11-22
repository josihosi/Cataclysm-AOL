# Setup and building

You can play C-AOL without an LLM. If you enable one, nearby followers can respond to what you type and propose actions the game knows how to carry out. Start with an **API** connection if you already have a key, or **Ollama** if you want to run a model locally.

## What the model actually sees

Say “Mara, stay here” near a follower. C-AOL builds a snapshot from that NPC’s perspective and sends it with your words and the list of actions they can request.

The prompt includes the follower’s condition and disposition, recent conversation, equipment, and a local ASCII map centered on the NPC. If the game finds a matching background summary, it also includes tone and a sample line of dialogue. The coordinate labels let the model refer to a destination. Creature letters let it refer to a target.

- Follow close or at a distance.
- Wait here or hold a position.
- Move to a nearby tile, then wait or hold.
- Equip a gun, melee weapon, or bow.
- Attack a creature marked on the map.
- Flee or stop fleeing.
- Look around and pick up a nearby item.
- Check inventory to wear, wield, activate, drop, or give an item.

<details>
<summary>See a complete follower-command prompt</summary>

This example uses the game’s default action template and a captured NPC snapshot, with a bundled evacuee background summary and a follower wait command. The full map and system instructions are included.

```text
Situation:
id: req_0
player name: Alex Xiong
player utterance present: true
player utterance: Miloch, stay here.

Recent conversation newest first:
(none)

your_name: Miloch
your_profession: no_past
your_tone: Survivalist, Resourceful, Desperate, Brave, Complacent
your_example_expression: "I started venturing towards the outskirts of town, picking off zombies here and there."
your_follow_mode: follow-close
your_state[0-10]: morale=4 hunger=0 thirst=0 pain=0 stamina=10 sleepiness=0 hp_percent=10 effects=[npc_flee_player:1]
your_emotions[0-10]: danger_assessment=0 panic=0 confidence=10
your_personality[0-10]: aggression=9 bravery=4 collector=10 altruism=4
your_opinion_of_player[0-10]: trust=7 intimidation=4 respect=6 anger=5

threats: (none)
friendlies: (none)

wielded: wielded="AR "pistol"+1"
weapons: [AR "pistol"+1 (30/30)]
bandage_possible: true

legend:
- ... open area
0 ... obstructive area (movement speed > 100)
6 ... obstructed area
[a - z] ... creature
[A - Z] ... obstructed creature
? ... unlettered creature (not a target handle)
| ... You (NPC)
creature legend with attitude and threat level:
a ... player neutral threat=0/10
map axes: +x east/right, -x west/left, +y north/up, -y south/down
map:
        -20      -10        0        +10      +20
        |.........|.........|.........|.........|
dy=+20                                          
dy=+19                                          
dy=+18                                          
dy=+17                                          
dy=+16                                          
dy=+15                                          
dy=+14                                          
dy=+13                                          
dy=+12                                          
dy=+11                                          
dy=+10                                          
dy=+09                                          
dy=+08                                          
dy=+07                                          
dy=+06                 6666666666               
dy=+05                 6------- 6               
dy=+04              6666--------6               
dy=+03              6 ----------66              
dy=+02              6 00000-----0  6            
dy=+01              6 ----------0  6            
dy=+00              6 00000|----0  6            
dy=-01              6 ------a---0  6            
dy=-02              6 00000------  6            
dy=-03              6 ------6666-  6            
dy=-04              6  6666-       6            
dy=-05              6      -       6            
dy=-06                                          
dy=-07                                          
dy=-08                                          
dy=-09                                          
dy=-10                     6                    
dy=-11                                          
dy=-12                                          
dy=-13                                          
dy=-14                                          
dy=-15                                          
dy=-16                                          
dy=-17                                          
dy=-18                                          
dy=-19                                          
dy=-20                                          
<System>You are controlling a human survivor NPC in a cataclysmic world, exhausted, armed, and trying not to die.Return a single line only, with correct syntax, to be parsed by the game.This line has two to four fields separated by ‘|’ :
<Field 1>The first field is an answer to player_utterance.If player_utterance_present is false, this is a spontaneous check-in with no direct player input.You have decided to team up with the player for now, and must answer as the NPC.Stick to your role, with your emotions and opinions.</Field 1>
<Fields 2-4>Write 1-3 of the following allowed actions exactly:
wait_here, hold_position, follow_close, follow_far, equip_gun, equip_melee, equip_bow, panic_on, panic_off, look_around, look_inventory, idle, attack=<target>, move=<dx>,<dy> <state>
<Explanation allowed actions>
Snapshot field your_follow_mode can be follow-close, follow-afar, or guard/hold.
If the player directly asks you to wait, stay, hold, guard, or stop, use 'wait_here' or 'hold_position' instead of follow actions.
Only use 'idle' when no action change is needed. Do not use 'idle' as a substitute for wait/hold commands.
'wait_here' to stay put, wait here, stand by, and remain with no special tactical position. Use this for ordinary requests to wait or stay.
'hold_position' to hold this exact spot tactically: a doorway, corner, choke point, post, or guarded position. Use this only for explicit hold-this-position / keep-watch / stakeout style orders, not ordinary waiting.
'follow_close' to walk behind, follow close, come here.
'follow_far' to follow from farther away, hang back, give me space.
'equip_gun' to equip gun, rifle, thrower, get ready to shoot.
'equip_melee' to equip melee, get ready to bash, cut, kick, stab.
'equip_bow' to use bow, crossbow, stealth.
'panic_on' to start running, get out of here.
'panic_off' if convincing, to stop fleeing and get your act together.
'look_around' to view your surroundings and pick-up, grab, search, explore for items around you.
'look_inventory': Inspect your inventory to wear, wield, activate, drop on the ground, or give an item.
'move=<dx>,<dy> <state>' to move to a relative destination offset on your snapshot map. Positive x is east/right, negative x is west/left, positive y is north/up, and negative y is south/down. The snapshot map includes dx column markers and dy row labels using that same signed-delta convention. Always include either 'wait_here' or 'hold_position' after the delta.
'attack=<target>' to attack a target with the letter from your map. Any creature with a map letter is a valid explicit target handle, including the player, friendlies, neutrals, and hostiles.
'idle' if none of the above.
</Explanation allowed actions>
</Fields 2-4>
Print only Fields 1-4, separated by | .If you break this format, you have failed.Output a single line with an answer and actions from the allowed list, in fields separated by ‘|’ and no additional text.
<Example Output 1>Blow me.|idle</Example Output 1>
<Example Output 2>Lets put those fucks in the ground.|equip_melee|attack=a</Example Output 2>
<Example Output 3>Providing cover!|wait_here|equip_gun</Example Output 3>
<Example Output 4>Lets get some dinner!|equip_gun|attack=b</Example Output 4>
<Example Output 5>Don't worry, I'm ready to kick some teeth in.|equip_melee</Example Output 5>
<Example Output 6>Locked and loaded.|equip_gun</Example Output 6>
<Example Output 7>Nope, not doing that!|panic_on</Example Output 7>
<Example Output 8>Moving down and holding there.|move=0,-5 hold_position|equip_gun</Example Output 8>
/no_think
Answer directly. No reasoning.
</System>
```

</details>

For a recruited follower, the game asks for one line with spoken dialogue and one to three action tokens separated by pipes. One built-in example is `Providing cover!|wait_here|equip_gun`.

The game parses the reply and checks the requested actions before applying them. Some requests take two calls. For example, when the model asks to look in its inventory, the follow-up lists actual items and possible recipients so it can choose what to give and to whom. The game checks whether the order and item are still valid when the answer comes back.

With an API backend, the provider receives the prompt, including dialogue and gameplay context. A local Ollama model on `127.0.0.1` can handle that on your own machine.

## LLM Setup

Choose a backend for your first setup:

- **API** runs the model through a provider. Use this if you already have an API key.
- **Ollama** runs a model on your computer. Local models need no API key.
- **OpenVINO** is the legacy local option for existing OpenVINO setups.

### Python setup

C-AOL uses Python to launch its LLM runner for every backend. Install Python 3, unpack your [C-AOL build](https://github.com/josihosi/Cataclysm-AOL/releases), and open a terminal in the unpacked game folder. It should contain `tools/llm_runner/runner.py`.

For the API setup, create a virtual environment in that folder, then print the full path to its Python executable.

**Windows PowerShell**

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -c "import sys; print(sys.executable)"
```

**macOS and Linux**

```bash
python3 -m venv .venv
./.venv/bin/python -c "import sys; print(sys.executable)"
```

For Ollama, you can use your regular Python installation. Run `python -c "import sys; print(sys.executable)"` on Windows or the same command with `python3` on macOS and Linux. For OpenVINO, use the Python executable from your existing OpenVINO environment.

**Now open the game and go to Options → LLM → Venv path. Paste the full Python executable path into that field.** Older builds call it **OpenVINO venv path**. Use that same Python for the backend self-test below.

### API backend

This example uses an OpenAI API key with access to `gpt-6-luna`. If your account does not have access to Luna, use an available model in both the self-test and the game settings. The runner uses the [any-llm Python SDK](https://docs.mozilla.ai/any-llm/).

#### Windows PowerShell

```powershell
.\.venv\Scripts\python.exe -m pip install "any-llm-sdk[openai]"
$env:CATA_API_KEY = "YOUR_API_KEY"
.\.venv\Scripts\python.exe tools/llm_runner/runner.py --self-test --backend api --api-provider openai --api-model gpt-6-luna --api-key-env CATA_API_KEY
```

#### macOS and Linux

```bash
./.venv/bin/python -m pip install "any-llm-sdk[openai]"
export CATA_API_KEY="YOUR_API_KEY"
./.venv/bin/python tools/llm_runner/runner.py --self-test --backend api --api-provider openai --api-model gpt-6-luna --api-key-env CATA_API_KEY
```

Start the game from the same terminal so it inherits the key. In **Options → LLM**, set:

| Setting | Value |
| --- | --- |
| Enable LLM intent runner | On |
| LLM backend | `api` |
| Venv path | Absolute path to `.venv\Scripts\python.exe` on Windows or `.venv/bin/python` on macOS and Linux |
| API key env var | `CATA_API_KEY` |
| API model name | `gpt-6-luna` or your chosen model |

The **API key env var** field takes the variable name. Keep the key itself out of game settings and shared logs.

### Ollama backend

Install and start [Ollama](https://docs.ollama.com/quickstart). If the server is not already running, start it with `ollama serve` in a separate terminal. Pull a local model, for example [Mistral](https://ollama.com/library/mistral):

```sh
ollama pull mistral
```

Run this from the game directory with your Python 3 interpreter. Use `python3` instead of `python` if that’s how it’s installed on your system.

```sh
python tools/llm_runner/runner.py --self-test --backend ollama --ollama-url http://127.0.0.1:11434 --ollama-model mistral
```

Use that same interpreter in **Options → LLM**:

| Setting | Value |
| --- | --- |
| Enable LLM intent runner | On |
| LLM backend | `ollama` |
| Venv path | Absolute path to your Python 3 executable |
| Ollama URL | `http://127.0.0.1:11434` |
| Ollama model name | `mistral` or the exact tag you downloaded |

For this setup, Python’s standard library is enough. The local model needs no API key.

### Try it in game

Stand near a recruited follower who can hear you. Open the chat menu and choose **Say a sentence**. Try addressing the follower by name and asking them to wait or follow you.

If it fails, run the self-test with the exact interpreter and model you entered in the game. For API, check that the game inherited the key. For Ollama, check that the server is running and the model appears in `ollama list`. A passing self-test checks the backend connection. The in-game conversation checks the rest of the path.

To see actual prompts, replies, and action events, turn on **LLM runner debug logging** in **Options → LLM** for a short run. Look for `llm_intent.log` and `llm_intent_events.log`. Review them for private text before sharing, then turn logging off.

### OpenVINO (legacy local backend)

The `openvino` backend is still available for existing Intel/OpenVINO setups. It needs a compatible local model and a Python environment with the OpenVINO dependencies installed. Set **Venv path**, **OpenVINO model path**, and **OpenVINO device** (usually `AUTO`). For a fresh local setup, start with Ollama.

## Building from source

Only do this if you want the source tree, want to build the game yourself, or want to work on the fork.

### Clone the repo

```powershell
git clone https://github.com/josihosi/Cataclysm-AOL.git
cd Cataclysm-AOL
```

For the current development alpha, select `dev`:

```powershell
git checkout dev
```

Older `port/*` branches and their releases target different upstream versions; their contents and behavior can differ from the current alpha.

### Build shortcuts in this repo

The repo includes some convenience scripts:

- **Windows build+run** → `build_and_run.cmd`
- **Windows build only** → `just_build.cmd`
- **Linux build+run** → `build_and_run_linux.cmd`
- **Linux build only** → `just_build_linux.cmd`
- **macOS build+run** → `build_and_run_macos.sh`
- **macOS build only** → `just_build_macos.sh`

Examples:

```powershell
.\build_and_run.cmd --unclean
.\just_build.cmd --unclean
```

```bash
./build_and_run_macos.sh --unclean
./just_build_macos.sh --unclean
```

For fuller platform docs, still read the upstream compile docs too:

- [COMPILING.md](c++/COMPILING.md)
- [COMPILING-MSYS.md](c++/COMPILING-MSYS.md)
- [COMPILING-VS-VCPKG.md](c++/COMPILING-VS-VCPKG.md)
- [COMPILER_SUPPORT.md](c++/COMPILER_SUPPORT.md)

## NPC backstory summaries (optional, for modders)

**Players can skip this.** C-AOL bundles short summaries for existing NPC backgrounds. The game loads them when it builds a prompt. An NPC without a matching summary can still talk and take commands. Their prompt simply omits the tone and sample dialogue fields.

If you add or change NPC backstories, you can write the short summaries by hand or generate them ahead of time from existing dialogue. The game reads topic summaries from `npcs/Backgrounds/Summaries_short/` and named NPC overrides from `npcs/Backgrounds/Summaries_extra/`, relative to the game data or an active mod. The bundled [topic examples](../data/json/npcs/Backgrounds/Summaries_short) and [named NPC examples](../data/json/npcs/Backgrounds/Summaries_extra) show the formats.

### Generate a summary for a mod NPC

The [core summary registry](../data/json/npcs/Backgrounds/summary_registry.json) lists named NPCs selected for summaries. For a new character, create a separate `summary_registry.json` at your mod root and add an entry like this:

```json
{
  "version": 1,
  "path_root": ".",
  "generated_output": "npcs/Backgrounds/Summaries_extra/generated_named_npcs.json",
  "entries": [
    {
      "id": "mara",
      "tier": "auto",
      "selectors": ["name:Mara", "topic:TALK_MYMOD_MARA"],
      "source_files": ["npcs/mara_dialogue.json"]
    }
  ]
}
```

Point `source_files` at the JSON files containing the NPC’s `talk_topic` dialogue. The generator reads the NPC lines from those files. Selectors tell the game which NPC should receive the finished summary, by name or by a matching dialogue topic. Use a topic specific to that character if names might overlap. Input and output paths are relative to `path_root`, which is relative to the registry file.

From the C-AOL source checkout, pass your registry to the [background summarizer](../tools/llm_runner/background_summarizer.py). Preview the entry first:

```sh
python tools/llm_runner/background_summarizer.py --registry path/to/my_mod/summary_registry.json --backend ollama --ollama-model mistral --only-entry mara --dry-run
```

Check that the preview reports a resolved source file and NPC lines. Remove `--dry-run` with Ollama running to write the summary, then review it and ship the generated JSON with your mod. C-AOL reads that output when the mod is active.

For backgrounds shared by multiple NPCs, the generator uses the [backgrounds table of contents](../data/json/npcs/Backgrounds/backgrounds_table_of_contents.json) and takes a topic ID via `--only-topic`. Run `--help` for custom source and output paths. Summary generation can use Ollama or OpenVINO independently of the in-game dialogue backend.
