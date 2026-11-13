# Design Notes (living doc)

## Concept (rough, user-originated, NOT final)
- "Solo Leveling" style progression (stats, ranks, quests, leveling up) set in a school with a class/point-ranking
  social system (inspired by the anime Classroom of the Elite).
- Chaotic, branching, decision-driven; weird/different endings.
- MUST NOT copy real characters, names, or plot from either source. Original cast, original world.
- Must tie to jam theme "Director's Cut" (undecided how).
- Possible OpenAI use: LLM-driven NPC dialogue/scheming, with RAG over a world/character bible. Needs API key (not yet).

## Constraints
- ~2 days remaining as of 2026-11-13. Solo. Scope: one small loop, 3-5 min playthrough.
- Pocket Powerhouse implies mobile build (Android) + touch controls.
- Key must never be committed.

## Open questions
- How "Director's Cut" is expressed in mechanics.
- 2D/2.5D/3D; mobile vs desktop target.
- Engine version.

## Decisions 2026-11-13
- Core hook (user): an AI "director" tracks/tests the player and works to steer EVERY player to the same (canon) ending;
  players who outsmart the AI's pattern (or the AI itself) earn their own distinct, satisfying ending.
  Fits theme: AI = enforces the theatrical cut; player earns their own Director's Cut.
- Platform: web not viable (UE5 has no HTML5 export); target PC with controller support.
- Art style (user, 2026-11-13): 3D, black & white plus ONE accent colour, chess motif (B/W because chess). Assets: AI-generated (Meshy/Tripo) or self-made.
- Engine setup via scripts/setup_ue.sh <zip>. Download needs user's Epic login (can't be scripted).

## Graphics / engine (2026-11-13)
- User wants NVIDIA award features (DLSS 4.5 / NvRTX / ACE) + great graphics + runs on low-spec. Dev GPU is AMD (cannot run DLSS/NvRTX) -> open question.
- DLSS 4.5 plugin (NVIDIA archive) supports UE 5.4-5.7 per search; 5.9 preview is unsupported/unstable. Leaning UE 5.7 if listed as stable.
- UE 5.7.4 chosen (2026-11-13). Zip must go to the data drive (root only has 44 GB free).

## Ending design (user, 2026-11-13)
- Every playthrough should feel different, yet almost all of them converge on the same canon ending.
- The hidden/own ending needs a genuinely different playstyle. It should be hard but not ultra-rare: target 5-10% of players (user-confirmed).
- Proposed mechanic: the AI director predicts the player's choices; staying unpredictable at key "tests" (low prediction hit-rate) unlocks the distinct endings.
- Title: "Classroom Levelling" (user, 2026-11-13).
- Platform: PC only for now (Pocket Powerhouse skipped). NVIDIA award dropped (no RTX hardware).
- Clarified (user): chess is NOT the theme. B/W palette is only a nod (the COTE protagonist plays chess). Core: the player enrols in an elite merit-ranked school, levels up, and makes insane decisions to win.
