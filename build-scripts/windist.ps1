if (Test-path bindist) {
  rm -Force -Recurse bindist
}

mkdir bindist
cp cataclysm-tlg-tiles.exe bindist/cataclysm-tlg-tiles.exe
if (Test-Path cataclysm-tlg-tiles.stripped.pdb) {
  cp cataclysm-tlg-tiles.stripped.pdb bindist/cataclysm-tlg-tiles.pdb
} elseif (Test-Path cataclysm-tlg-tiles.pdb) {
  cp cataclysm-tlg-tiles.pdb bindist/cataclysm-tlg-tiles.pdb
}
cp tools/format/json_formatter.exe bindist/json_formatter.exe
cp zzip.exe bindist/zzip.exe

mkdir bindist/lang
cp -r lang/mo bindist/lang

$extras = "data", "doc", "gfx", "LICENSE.txt", "LICENSE-OFL-Terminus-Font.txt", "README.md", "Plan.md", "TechnicalTome.md", "Agents.md", "VERSION.txt"
ForEach ($extra in $extras) {
	cp -r $extra bindist
}
mkdir bindist/tools
cp -r tools/llm_runner bindist/tools
Compress-Archive -Force -Path bindist/* -DestinationPath "cataclysm-tlg-1.0.zip"
