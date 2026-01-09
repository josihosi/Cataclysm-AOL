# Cataclysm: Arsenic and Old Lace

Hello! This is just a little fork, where I try to use LLMs to improve immersion and responsiveness of the world.
This is something I would actually like to see, so I'm trying to make it.
Check out the original repo, its awesome https://github.com/CleverRaven/Cataclysm-DDA
Big thanks to the team, for making this awesome game!

<p align="center">
    <img src="./data/screenshots/ultica-showcase-sep-2021.png" alt="Tileset: Ultica">
</p>

## Downloads

### LLM runner (Python + OpenVINO GenAI)

This fork uses a local Python runner (no network) to generate NPC intents.
Tested on Windows with Intel Core Ultra 7 155H + NPU, using:
- Model dir: `C:\Users\josef\openvino_models\Mistral-7B-Instruct-v0.2-int4-cw-ov`
- Venv: `C:\Users\josef\openvino_models\openvino_env`

If you do not have an NPU, you can still try this with CPU or GPU, but it will be slower.
You can also point the runner at your own model directory.

### Python packages
Create a venv and install the packages below (pinning matches the tested setup):

```sh
python -m venv C:\Users\josef\openvino_models\openvino_env
C:\Users\josef\openvino_models\openvino_env\Scripts\activate
pip install nncf==2.18.0 onnx==1.18.0 optimum-intel==1.25.2 transformers==4.51.3
pip install openvino==2025.4 openvino-tokenizers==2025.4 openvino-genai==2025.4
```

Note: For NPU use on OpenVINO 2025.4, transformers==4.51.3 is the tested version.

### Models
Put your OpenVINO model folder anywhere you like and point the runner at it.
The tested model is `OpenVINO/Mistral-7B-Instruct-v0.2-int4-cw-ov`.

### Runner usage
The runner is a long-lived local process that reads JSON from stdin and writes JSON to stdout.
It is configured for NPU-only operation (no CPU fallback).

```sh
C:\Users\josef\openvino_models\openvino_env\Scripts\python.exe tools\llm_runner\runner.py ^
  --model-dir C:\Users\josef\openvino_models\Mistral-7B-Instruct-v0.2-int4-cw-ov ^
  --device NPU ^
  --force-npu
```


## Compile

Please read [COMPILING.md](doc/c++/COMPILING.md) - it covers general information and more specific recipes for Linux, OS X, Windows and BSD. See [COMPILER_SUPPORT.md](doc/c++/COMPILER_SUPPORT.md) for details on which compilers we support. And you can always dig for more information in [doc/](https://github.com/CleverRaven/Cataclysm-DDA/tree/master/doc).

We also have the following build guides:
* Building on Windows with `MSYS2` at [COMPILING-MSYS.md](doc/c++/COMPILING-MSYS.md)
* Building on Windows with `vcpkg` at [COMPILING-VS-VCPKG.md](doc/c++/COMPILING-VS-VCPKG.md)
* Building with `cmake` at [COMPILING-CMAKE.md](doc/c++/COMPILING-CMAKE.md)  (*unofficial guide*)

## Contribute

Cataclysm: Dark Days Ahead is the result of contributions from over 1000 volunteers under the Creative Commons Attribution ShareAlike 3.0 license. The code and content of the game is free to use, modify, and redistribute for any purpose whatsoever. See https://creativecommons.org/licenses/by-sa/3.0/ for details.
Some code distributed with the project is not part of the project and is released under different software licenses; the files covered by different software licenses have their own license notices.

Please see [CONTRIBUTING.md](doc/CONTRIBUTING.md) for details.

Special thanks to the contributors, including but not limited to, people below:
<a href="https://github.com/cleverraven/cataclysm-dda/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=cleverraven/cataclysm-dda" />
</a>

Made with [contrib.rocks](https://contrib.rocks).

#### Is there a tutorial?

Yes, you can find the tutorial in the **Special** menu at the main menu (be aware that due to many code changes the tutorial may not function). You can also access documentation in-game via the `?` key.

#### How can I change the key bindings?

Press the `?` key, followed by the `1` key to see the full list of key commands. Press the `+` key to add a key binding, select which action with the corresponding letter key `a-w`, and then the key you wish to assign to that action.

#### How can I start a new world?

**World** on the main menu will generate a fresh world for you. Select **Create World**.

#### I've found a bug. What should I do?

Please submit an issue on [our GitHub page](https://github.com/CleverRaven/Cataclysm-DDA/issues/) using [bug report template](https://github.com/CleverRaven/Cataclysm-DDA/issues/new?template=bug_report.md). If you're not able to, send an email to `kevin.granade@gmail.com`.

#### I would like to make a suggestion. What should I do?

Please submit an issue on [our GitHub page](https://github.com/CleverRaven/Cataclysm-DDA/issues/) using [feature request template](https://github.com/CleverRaven/Cataclysm-DDA/issues/new?template=feature_request.md).
