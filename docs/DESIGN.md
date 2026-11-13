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

## World bible Q&A round 1 (user, 2026-11-13)
- Setting: modern Japan-like city.
- Protagonist: blank silent player.
- Tone: dark psychological scheming (COTE-like) + unhinged absurd comedy (One Piece-like); the chaos shows in the art, models and environments too.
- Levelling: Solo Leveling-style system and access to new areas/abilities, but NOT overpowered. Levelling costs a heavy psychological toll.
- The System = the AI. It evaluates the player constantly and issues quests based on their decisions. Quests get harder over time. Its goal is to force everyone into the same canon ending by putting the player in ridiculous situations.
- Distinct ending: earned by taking stray, ridiculous decisions entirely off the expected path. No "right" answer; the ending can be very bad or very good (mostly good). It describes the player's thinking style and is based on how different they are from other players.
- Average/low-risk players -> canon ending.
- Button-mashers / rushers -> dead end: relationships ruined, under-levelled, stuck on a quest they can't beat (effectively softlocked).

## World bible Q&A round 2 (user, 2026-11-13)
- Uniqueness is measured against BOTH a built-in "average player" profile and real aggregate player data (needs a server: open question on cost).
- Dead end: the player is mocked at a dead end (no one helps them because they ignored everyone) and must restart from the beginning.
- AI runtime: user wants max capability with zero CPU/GPU/money cost; any AI provider is fine (OpenAI no longer required). Proposed: AI writes the content during dev, plus a lightweight local director (scoring/prediction logic) at runtime.
- Length: as long as possible. Build the core loop first, then add content until the deadline.
- User approved: offline AI-authored content + local director; real-player comparison server last (free tier) if time; core loop first, then chapters.

## World bible Q&A round 3 (user, 2026-11-13)
- Ranking: close to COTE. Classes compete; the player levels both solo AND with their class. Premise: "how would an average student survive in a COTE-like school?" Guardrail: original names, cast and point system; nothing lifted from COTE's terms, characters or plot.
- Stats: Intellect, Charisma, Physique, Sanity (+ more TBD).
- Cast: as many as possible (start with a core set, add per chapter).
- Story branches on decisions; skeleton TBD.
- Stats: Intellect, Charisma, Physique, Sanity, Cunning, Reputation, Funds, Luck. Each is a GENERAL stat aggregating several sub-stats; sub-stat changes roll up into it. Stats drive how NPCs act toward the player.
- Canon ending: "Puppet at the top". Player reaches #1 by betrayal or obedience to the System; wins alone/hated. The same ending gets slight twists based on behaviour.
- Structure: all three. Chapters = special exams, inside a day-by-day calendar, with a free-roam school hub + quest board.
- Secret endings (all approved): The Director (seize the AI, rewrite the school), The Ghost (invisible puppeteer, forgotten), The Revolutionary (abolish the ranking), The Dropout (walk away), The Clown (pure chaos, the school becomes a circus), The Kingmaker (puts an NPC at #1), The Mirror (the AI learned from you; you become the System).

## World bible Q&A round 5 (user, 2026-11-13)
- Core cast roles: Genius rival, Loyal-ish ally, Chaos gremlin (+ more TBD).
- Teachers: minimal role, mainly relay information. But one secret ending could involve winning a teacher over or manipulating one.
- The System appears as UI/voice only (quest windows, Solo Leveling-like).
- Starting class: random each run. Each class comes with a backstory reason ("why you're here") that grants base stats + a unique skill.
- More cast: Council president, Fallen ex-#1, and a shady random NPC (e.g. a store worker) who's secretly the best info broker. Random NPC interactions can hold key info.
- Rich-kid route: manipulate school points <-> in-game real money. One of the hardest ways to graduate #1 (needs high Luck/Funds at birth + acing exams).
- Teacher route: both a contributing key to other endings AND its own ending variant.
- Classes: 4-5. Placement = the student's 2 highest base general stats, so the classes are balanced overall. Which class wins depends on its leader and their style (democratic, communal, dictatorial, from the shadows...).
- TWO LAYERS of class (user-confirmed):
  - Public homeroom classes A, B, C, D: balanced, so each has the same average stats.
  - Hidden "secret class" (archetype) from each student's 2 highest general stats: Onyx (Intellect+Cunning), Slate (Physique+Sanity), Ash (Charisma+Reputation), Ivory (Funds+Luck).
  - Every student (NPCs included) has their own 8 stats + secret class; each homeroom mixes the archetypes evenly.
- Timeline: 3 school years to graduation (scope for the jam build TBD).
- Scope (user): all 3 years compressed. Each year = 2-3 exam chapters, with time-skips + stat buffs between. Build as much as possible.
- Gameplay (user): hybrid. Third-person walkable campus hub with visible NPCs to approach and talk to (feel reference: Pokemon S/V school/overworld, NOT its content). Exams + story = cinematic 3D scenes with choice menus + System UI windows.

## Exams + depth (user, 2026-11-13)
- Draft exam list (9, 3 per year): Hot Seat, Group Project From Hell, Cafeteria Stock Market | Mock Trial, Chained Sports Festival, Council Election | Rumour Exam, Debt Exam, The Final Cut. Approved as a base.
- Exams must be LONG. Off-script stunts (e.g. Kingmaker) need prerequisites: stat levels, decisions earlier in the chapter, allies in other classes, things acquired earlier. Going against the System needs something set up from the beginning.
- Year 1 = existential arc: new students questioning life/purpose. Ignore it and bond with friends, build your own purpose, go nihilist, go absurdist, etc. This sets the trajectory: shadow, dropout (system too hard), too smart for the system, average rat-racer, or the best = System's puppet.
- Hidden behaviour tracking: sympathy/empathy, how they react to each situation, how long they think. If they take too long, the System/NPCs get bored, smack them, or wonder what's up.
- NPCs interpret the player through their own communication style -> misunderstandings.
- Villainization: track who the player turns into the villain (the System, specific students, a whole class, their own class, teachers). Anything can happen.

## Exams + depth (user, 2026-11-13)
- Draft exam list (9, 3 per year): Hot Seat, Group Project From Hell, Cafeteria Stock Market | Mock Trial, Chained Sports Festival, Council Election | Rumour Exam, Debt Exam, The Final Cut. Approved as a base.
- Exams must be LONG. Off-script stunts (e.g. Kingmaker) need prerequisites: stat levels, decisions earlier in the chapter, allies in other classes, things acquired earlier. Going against the System needs something set up from the beginning.
- Exams adapt per player: eligibility can differ; the exam can change entirely because of past decisions; or the same exam is populated by past choices (e.g. Mock Trial accused = your best friend, your rival, or a random NPC who later proves useless, a plot-twist saviour, or the biggest enemy even after you saved them).
- Outcomes depend on how each NPC INTERPRETS the player's decision: player personality + NPC personality + luck.
- Year 1 = existential arc: new students questioning life/purpose. Ignore it and bond with friends, build your own purpose, go nihilist, go absurdist, etc. This sets the trajectory: shadow, dropout (system too hard), too smart for the system, average rat-racer, or the best = System's puppet.
- Hidden behaviour tracking: sympathy/empathy, how they react, how long they think. If they take too long, the System/NPCs get bored, smack them, or wonder what's up.
- NPCs interpret the player through their own communication style -> misunderstandings.
- Villainization: track who the player turns into the villain (the System, specific students, a whole class, their own class, teachers).
