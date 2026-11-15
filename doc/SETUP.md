# Setup and building

This guide describes the source tree and optional LLM configuration. This alpha is under development; releases and platform instructions may differ by revision. Consult the build instructions for your platform and preserve existing saves and configuration when updating.

## LLM runner overview

The game talks to `tools/llm_runner/runner.py`. You configure it in the in-game **[LLM]** options menu.

The important options are:

- **Enable LLM intent runner**
- **LLM backend** (`ollama`, `api`, or `openvino`)
- **OpenVINO venv path** — despite the name, this is effectively the Python path the game uses to launch `runner.py`
- **Ollama URL / model**
- **API key env var / API model**
- **OpenVINO model path / device**

Current backend choices:

- **Ollama** — recommended local path for most people
- **API via any-llm** — easiest way to get good responses quickly
- **OpenVINO** — advanced local path, especially relevant for Intel/NPU setups

Important: even for **API** or **Ollama**, the game still needs a valid Python executable/venv path so it can launch `tools/llm_runner/runner.py`. In the options menu that field is currently labeled **OpenVINO venv path**, which is a bit misleading but still the right place to point at your Python.

## Installation / setup

### 1. Easiest path: play a release

1. Check the [releases page](https://github.com/josihosi/Cataclysm-AOL/releases) for a suitable build.
2. Unpack it.
3. Start the game once.
4. Open the in-game **[LLM]** options.
5. Pick a backend and fill in the matching settings.

Availability and contents vary by release. For the current development version, build from source as described below.

### 2. Choose an LLM backend

#### Option A — API via any-llm (easiest overall)

This is the quickest way to get the fork working if you already have an API key.

Example setup in a Python venv:

```powershell
python -m venv .venv
.\.venv\Scripts\activate
pip install "any-llm-sdk[openai]"
$env:CATA_API_KEY = "your_key_here"
```

Then set these in the in-game **[LLM]** menu:

- **LLM backend** → `api`
- **OpenVINO venv path** → your Python executable or the venv you installed `any-llm` into
- **API key env var** → `CATA_API_KEY`
- **API model name** → e.g. `gpt-4.1-mini`

Notes:
- `CATA_API_KEY` is the default env-var name used by the game options. The assignment above applies to the current PowerShell session; start the game from that session so it inherits the key.
- `any-llm` supports multiple providers. If you are not using OpenAI, install the matching extra and set the provider/model accordingly.

#### Option B — Ollama (recommended local path)

If you want a local setup without the OpenVINO-specific baggage, Ollama is the easiest local backend.

1. Install Ollama.
2. Pull a model you actually want to run.
3. Point the game at that model in the **[LLM]** menu.

Example:

```powershell
ollama pull mistral
```

Then set these in the game:

- **LLM backend** → `ollama`
- **OpenVINO venv path** → a normal Python 3 executable or venv
- **Ollama URL** → usually `http://127.0.0.1:11434`
- **Ollama model name** → e.g. `mistral`

A normal Python 3 install is enough for the runner here; you do not need the OpenVINO stack just to use Ollama.

#### Option C — OpenVINO (advanced local path)

This path is mainly for local Intel/OpenVINO setups and is the most fiddly one.

Example venv install matching the tested setup:

```powershell
python -m venv C:\path\to\openvino_env
C:\path\to\openvino_env\Scripts\activate
pip install nncf==2.18.0 onnx==1.18.0 optimum-intel==1.25.2 transformers==4.51.3
pip install openvino==2025.4 openvino-tokenizers==2025.4 openvino-genai==2025.4
```

Then set these in the game:

- **LLM backend** → `openvino`
- **OpenVINO venv path** → path to the venv or directly to the Python executable
- **OpenVINO model path** → path to the local model directory
- **OpenVINO device** → usually `AUTO`, unless you know you want `CPU`, `GPU`, or `NPU`

This fork was heavily tested on an Intel Core Ultra machine, but OpenVINO is not the only supported path anymore.

### 3. Runner self-test

You can sanity-check the runner before launching the game.

#### API

```powershell
python tools\llm_runner\runner.py --self-test --backend api --api-provider openai --api-model gpt-4.1-mini --api-key-env CATA_API_KEY
```

#### Ollama

```powershell
python tools\llm_runner\runner.py --self-test --backend ollama --ollama-url http://127.0.0.1:11434 --ollama-model mistral
```

#### OpenVINO

```powershell
python tools\llm_runner\runner.py --self-test --backend openvino --model-dir "C:\path\to\ov_model" --device AUTO
```

If the self-test works but the game does not, the next thing to check is usually the in-game **[LLM]** option values rather than the runner itself.

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

