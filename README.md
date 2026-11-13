# Classroom Levelling (Epic MegaJam: "Director's Cut")

Unreal Engine project (engine version, concept: TBD).

## Setup notes
- Engine is installed outside this repo.
- Git LFS required: `git lfs install`
- For vim/clangd: generate `compile_commands.json` via
  `UnrealBuildTool -mode=GenerateClangDatabase -project=<uproject> -game -engine <Target>Editor Linux Development`
