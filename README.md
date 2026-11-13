# Classroom Levelling

Epic AI MegaJam 2026 entry (theme: "Director's Cut"). Solo student project, Unreal Engine 5.7.4, C++ only.

You enrol at the most elite school in the world. Its evaluation System watches every choice, ranks you
against 24 classmates, and has already written your ending: the same one it writes for everyone. Before each
decision it commits to a forecast of what you will do. Stay predictable and you get the Theatrical Cut.
Become someone it cannot model and you earn your own Director's Cut.

- Third-person free-roam campus, 15 areas, talk to anyone.
- 85 storylets, 3 multi-stage exams, 9 endings. The cast is re-rolled every run from a pool of generated students.
- NPCs read the *meaning* of your choices through their own values, and sometimes misread you.
- Levels, rank, 8 stats, 6 random origins. Every level costs sanity.
- Runtime text-to-speech with a distinct voice per character. Keyboard/mouse and gamepad.

## Controls
| | Keyboard | Gamepad |
|---|---|---|
| Move / look | WASD / mouse | sticks |
| Sprint | Shift | L3 |
| Talk | E | X |
| EVAL app (stats, rank, forecast) | Tab | Back |
| Choose | 1-4 | A B X Y |
| Continue / skip text | Space | A |
| Pause | Esc | Start |

## How it was made
One human directing AI agents; nothing in the repo predates the jam. The full story is in
[docs/DEVLOG.md](docs/DEVLOG.md) and the design decisions in [docs/DESIGN.md](docs/DESIGN.md).

| Tool | Used for |
|---|---|
| Claude Code (Opus 5.5, Fable 5.2) | all C++, tools, build and packaging scripts |
| Grok via Cursor CLI (`tools/gen.sh`) | storylets, exams, endings, the student and staff pool |
| Hyper3D Rodin (`tools/assets.py`) | 18 prop models |
| Piper TTS | runtime character voices |
| `tools/gen_audio.py` | procedurally synthesised music, ambience and sound effects |
| `tools/sim.py` | simulated players used to tune how often each ending fires |
| Unreal Engine 5.7.4 | engine, mannequins and animations from the Third Person template |

## Build
- Linux dev: `tools/setup_content.sh && tools/setup_piper.sh`, then `./play.sh`.
- Windows package: install UE 5.7.4 and Visual Studio 2022 (Game development with C++), then `tools\package_win.bat`.
- Git LFS is required (`git lfs install`).
