# Dev Log (also feeds the Glass Box award: keep it honest and detailed)

Newest last. Every session: append what was decided/done/blocked.

## 2026-11-12 (jam day 1)
- Jam: Epic AI MegaJam, Nov 12-15 2026, itch.io. Theme: "Director's Cut". Solo dev.
- Machine: Linux, RX 6650 XT, 30GB RAM, 16 cores, 312GB free. No UE installed yet.
- Tools present: git, git-lfs, gh (logged in as Mahit-Shah06, ssh), clang, cmake, dotnet, mono.
- Created local git repo (main), .gitignore, .gitattributes (LFS), .clangd, README, CLAUDE.md.

## 2026-11-13 (jam day 2)
- Chose award targets (see CLAUDE.md). Pocket Powerhouse = best MOBILE game; Render Royalty = best visuals.
- Concept brainstorm started: see docs/DESIGN.md.
- Engine download instructions given to user (zip from unrealengine.com -> extract to /media/itzzsuperrr/846e0635-f4f1-4d51-b053-9c416566a01f/UE). Awaiting confirmation.

## Open blockers
- UE not installed / version unknown.
- No OpenAI API key (user can arrange a temporary one).
- No GitHub repo connected yet.
- Official rules/deliverables from itch.io page not read yet (user said to get from website; search only found secondary sources).
- Decisions logged in docs/DESIGN.md; scripts/setup_ue.sh created.
- Engine version discussion: leaning 5.7; awaiting user to confirm availability + RTX hardware access.

## 2026-11-13 (session 3, Opus 5.5)
- Resumed from prior transcripts. UE still not installed (no zip in ~/Downloads, no UE dir).
- Auto-download blocked: no browser tool (claude-in-chrome) in this session; GitHub account not linked to Epic (EpicGames/UnrealEngine -> 404), so no source build either.
- Still awaiting: 5.7 availability, RTX access / drop NVIDIA award.
- User pasted the download list: 5.7.0-5.7.4, 5.8.0-5.8.4 and the 5.9 preview are all listed. Decision: UE 5.7.4 (newest that the DLSS plugin reportedly supports).
- Specs confirmed: RX 6650 XT + Radeon 680M iGPU, no NVIDIA GPU. Root has 44 GB free, data drive 312 GB -> save the zip to the data drive.
- Engine goes on the external SSD (the data drive). Added Rule 3 (build -> polish -> optimize). Logged the ending-convergence design and title candidates.
- Flagged: Pocket Powerhouse = mobile, which conflicts with the PC-only decision.
- User answers: PC only, hidden-ending rate 5-10%, title "Classroom Levelling". User downloading UE 5.7.4; delete the zip from internal storage once it's extracted.
- Art: 3D B/W + one accent colour, chess theme. Asset gen: Meshy/Tripo (accounts TBD).
- Awards research: the OpenAI awards are sponsor awards attached to categories (Pocket Powerhouse = mobile, so skipped; Render Royalty+OpenAI still viable on PC). Main categories: Best Overall UE / UEFN / Best Student Project (must be enrolled at a university). Source: dev.to, tweaktown, beebom.
- Official jam email (partner perks) received. Key facts:
  - DEADLINE: Nov 15, 5:59 PM PST.
  - OpenAI awards ($10k in API credits): $5k Best Overall UE, $2.5k Render Royalty, $2.5k Pocket Powerhouse. OpenAI use in dev OR gameplay counts; answer the optional question at submission.
  - One main + one modifier category per entry.
  - Hyper3D Rodin: $10k cash pool for entries that used Rodin (code MEGAJAM2026, redeem by Nov 16). List all tools in the "Tools you used" field.
  - User is redeeming Tripo + Cursor. User is a college student (Best Student Project is eligible).
- Open: pick main (Overall UE w/ OpenAI vs Student) + one modifier; accent colour.
- DECISION (user): main category = Best Student Project, modifier = Glass Box. Solo/Render Royalty/OpenAI are not formally targeted (only one modifier allowed), but still use OpenAI in dev.
- Open: pure grayscale vs grayscale + 1 accent; Windows 10/11 build (UE can't package Windows from a Linux host -> need a Windows PC).
- Colour: grayscale for now; the accent becomes a tweakable material/post-process parameter (decide later). User has a Windows PC for Windows packaging.
- UE download in progress (5.5/30 GB). Git: first local commit made.
- Windows packaging: needs Unreal on Windows (dual boot, 162 GB BitLocker partition with Valorant). The external SSD is ext4, which Windows can't read. Plan: build on Linux first, decide on Windows near the end. Repo: public (user).
- GitHub repo created: public, classroom-levelling (title stays "Classroom Levelling").
- World bible round 1 answers logged in DESIGN.md.
- Round 2 answers logged.
- User approved: offline AI-authored content + local director; real-player comparison server last (free tier) if time; core loop first, then chapters.
- UE 5.7.4 zip downloaded. Deps were already installed. Extracting to /media/.../UE/5.7.4 (background); zip auto-deleted from ~/Downloads on success.
- Exam/depth design logged (adaptive exams, NPC interpretation, philosophy arc, hesitation tracking, villain target).
- UE 5.7.4 extracted to /media/.../UE/5.7.4 (zip deleted). Created C++ project ClassroomLevelling from the TP_ThirdPerson template (renamed module + class redirects). The template files were read-only -> chmod u+w.
- Build: `Engine/Build/BatchFiles/Linux/Build.sh ClassroomLevellingEditor Linux Development -Project=$PWD/ClassroomLevelling.uproject` -> Succeeded (2 min).
- 2026-11-13: The user confirmed the session runs on the jam's $25 Claude API credits (about $21 left), so I'll keep token use low. Plan: use Groq (shared $50 credit with Cursor) through GROQ_API_KEY in .env for bulk content generation. Cursor credits only work inside the Cursor IDE.
- Glass Box: the repo is already public (Mahit-Shah06/classroom-levelling). Keep small, descriptive commits plus this log as the evidence trail.
- 2026-11-13: The user redeemed all three perks: Tripo, Rodin and Cursor. The $50 credit is Cursor/xAI Grok (not Groq), and the dashboard says it "applies to Cursor usage".
- Keys go in .env (gitignored). Template in .env.example. The user writes the keys themselves, so they never go through chat.
- Crash safety (16 threads, 30 GB RAM, 8 GB swap, systemd-oomd active): shader compiling now leaves 4 threads free (6 while in-game), and UBT runs at most 10 parallel compiles (~/.config/Unreal Engine/UnrealBuildTool/BuildConfiguration.xml). Temps are fine (GPU 54C, CPU 60C).
- The $50 is Cursor-only credit, with no separate xAI API balance. There's no OpenAI key with credits either, so OpenAI-in-dev is dropped unless one turns up.
- Plan: install the Cursor CLI (`agent -p`, headless) and offload bulk content writing to it (NPCs, storylines, exam text) so the Cursor credit gets used instead of the Claude credit. Tripo and Rodin APIs: script batch 3D asset generation. Rodin use also counts toward the $10k pool.
- Cursor CLI installed (~/.local/bin/agent) and the user logged in. Headless test: `agent -p --trust --model auto "..."` returns output. On the Free plan, GPT models fail with "Model unavailable on Free"; only Grok 4.7, Grok 4.6 and Auto are included. It's still unclear whether the $50 credit unlocks GPT models.
- The editor closed with a clean exit (CloseEditor), not a crash.
- Core narrative system built (C++, compiles):
  - Source/ClassroomLevelling/Narrative/ holds NarrativeTypes (stats/sub-stats, secret class, tagged choices, NPC profiles, storylets with slots) and UNarrativeSubsystem, the local director.
  - The director loads Content/Data/*.json, parses condition strings, picks storylets, fills slots (best_friend/worst_enemy/role:x/random), interprets choices per NPC (perception + luck misreads, relationship bias, hesitation penalty) and computes uniqueness against the average profile.
  - Added the Json and JsonUtilities modules.
- tools/gen.sh: Cursor CLI (Grok/auto) writes JSON content from the C++ schema plus DESIGN.md, which saves Claude credits. Generated npcs_core.json (6 core NPCs) and storylets_y1_intro.json (8 storylets, including exam_hot_seat with an {accused}=best_friend slot). Schema check passes.
- Renamed one generated NPC (Morishita -> Tsukishiro) to avoid a COTE character name. Rule going forward: no names copied from COTE or other existing IP.
- User corrected the theme meaning (developer directing agents, not film), logged in DESIGN. User answers: Piper voices with a distinct voice and accent per NPC; teachers "both, depending on player actions"; cast must be random each run.
- Implemented the random cast roll (NewRun), @role/@slot refs (Resolve), and Kind/Voice/RoleFit fields. Grok generated 24 students (2 files) plus 8 role-based Y1 storylets. Fixed duplicate ids and renamed Okabe (Steins;Gate) to avoid IP.
- UStoryPlayerComponent (on the PlayerController): Slate System window with typewriter text, choices, hesitation bar, interpretation readout, UI shake and gamepad rumble. Pure C++, no binary UI assets.
- Headless test (`UnrealEditor <uproject> -game -nullrhi`): "Loaded 24 NPCs, 8 storylets", map loads, no errors.
- Gamepad: A/B/X/Y pick choices 1-4 (A also continues), Start closes. Added play.sh (standalone windowed launch).

## 2026-11-13 (session 4, Opus 5.5): playtest round 1 fixes
- Playtest feedback (user): the System showed up from the very first frame; wanted English/diverse names, real environment/graphics, a plain "Hesitation detected", more reading time, and a dialogue box with the choices on its right (1/2/3 on keyboard, A/B/X/Y on gamepad). Also asked for a main menu, settings, cutscenes, more story talk, and shell commands.
- Story frame (user): the System is the school's real evaluation system, run by the heads (who boast a 99% placement rate). It relays directives through the professors. Survivors become puppets; #1 is the most perfect puppet. The player's stance emerges from play (puppet, pet, rebel, manipulator). Logged in DESIGN.md.
- Content via Grok (tools/gen.sh, free Cursor credit, 3 jobs in parallel): 7 staff (npcs_staff.json); a rewritten Y1 opening (entrance ceremony cutscene -> homeroom app -> 6 role meetings -> Hot Seat exam); 12 new y1_day_ storylets. A validator confirmed every required flag is set somewhere and all speakers are valid.
- Renamed all 24 students to diverse international names (staff/student first-name clashes fixed).
- Schema: FDialogueLine (Speaker/Text/Shot), plus storylet Location, Lines and PromptSpeaker. Staff roles (@head, @homeroom, @proctor, ...) are rolled from the staff pool.
- Voices: runtime Piper. Names are rolled per run, so lines can't all be pre-generated. One persistent standalone piper process (C++ release, works on Linux and Windows) takes JSON lines over stdin. WAVs are cached in Saved/VoiceCache and played as USoundWaveProcedural. RTF 0.06 on CPU. Setup: tools/setup_piper.sh [linux|windows] -> ThirdParty/Piper (gitignored, 78 MB model).
- UI rewrite: letterboxed cutscenes; dialogue box (name plate + typed text) with the choices on the right; badges switch between 1-4 and A/B/X/Y based on the last input device. Hesitation starts only after the text finishes plus a reading grace (1.5 s + 0.3 s/word) and shows just "HESITATION DETECTED". NPC readings appear as an EVAL app toast.
- Main menu + pause (Esc/Start) + settings: graphics preset, render scale, display mode, VSync, volume, voices, text speed, hesitation timer on/off. A first launch auto-runs the hardware benchmark to pick graphics settings (low-spec).
- Hub: procedural B/W school (gate, courtyard, corridor, hall, classroom, office, cafeteria, rooftop) built from instanced basic shapes (~12 draw calls). Unbound PP: desaturate, contrast, vignette, no motion blur. NPC stand-ins wander, have name tags and mouth flaps. "!" markers mark areas with eligible storylets; walking in starts one. Cutscene camera shots: wide/close/over_shoulder/pan/low. ~120 fps on dev PC.
- Verified with the new `-autotest` flag (auto-plays + screenshots): ceremony -> homeroom chain, toast, choices on the right.
- Assets: Cursor agent wrote tools/assets.py (Tripo + Rodin). Rodin: 4/4 OK (bookshelf, school_gate, podium, vending_machine -> Assets/Generated). Tripo: all failed with Cloudflare 403 / error 1010 (it blocks the default Python user-agent). Changing the UA was blocked by the permission classifier; left for the user.
- Shell: ~/.local/bin/ue and unreal-engine (open the editor with the project), classroom-levelling (play.sh).
- Gotcha: `pkill -f piper/piper` matched its own shell; use `pkill -x piper`.

## 2026-11-13 (session 5, Opus 5.5): models, huge campus, menus, saves, audio
- Feedback (user): wants proper models for players/teachers, random player gender, everything done in parallel, a free-roam huge campus, a proper menu, saves, and all audio/graphics/controller settings.
- Parallel work: Grok (gen.sh) wrote 30 talk storylets + 14 campus storylets while the Cursor agent wrote tools/gen_audio.py (stdlib synth: 3 music loops, ambience, UI, footsteps). I wrote the C++. 65 storylets validate (flags, speakers, ids).
- Characters: the project was missing the template Characters/Input packs. tools/setup_content.sh copies them from the engine (gitignored, 126 MB). NPCs are now ACharacters with Manny/Quinn by Gender (new NPC field; staff rebalanced to 4m/3f) and the template locomotion ABP, wandering via CharacterMovement. Staff wear dark tints, students mid-grey, plus 36 unnamed background students. Low-spec: URO, anim ticks only when rendered, 70 m draw distance, 20 Hz actor tick. Player body: random Manny/Quinn per run (saved).
- Campus: ~24000 x 24000 cm. Added a street, bus stop, city skyline, perimeter walls, paths, avenues with trees/lamps, a store, library, gym, sports field + track, dorm and garden. Props are culled at 60-120 m. Area titles fade in on entry.
- Gameplay: sprint (Shift/L3, hold or toggle), talk to the nearest named NPC (E/X) with an in-place staged conversation, footsteps, music per context (menu/hub/exam tension) + ambience.
- Saves: UCLSaveGame (stats, relationships, flags, tags, played, role bindings, seed, gender, position). 3 slots + autosave after every scene, on Quit and on Quit to Title. Title menu: Continue / New / Load / Settings / Quit. Pause: Resume / Save / Load / Settings / Title / Quit.
- Settings: Graphics (preset, auto-tune, 10 individual qualities, render scale, motion blur), Display (mode, resolution, VSync, frame limit, FOV, gamma, FPS counter), Audio (master/music/ambience/effects/voice + voice on/off), Controls (mouse/pad sensitivity, invert Y, vibration, sprint mode, binding reference), Gameplay (text speed/size, auto-advance, hesitation timer, name tags, markers), and reset to defaults.
- BUG found + fixed: UGameUserSettings::RunHardwareBenchmark() deadlocks at frame 0 with a real GPU (main thread stuck on a futex; diagnosed via /proc wchan since ptrace is blocked). This was also the "stuck at frame 1" from session 4. Replaced it with an FPS auto-tune: start at High, measure on the title screen, step down until ~50 fps.
- Packaging: Content/Data + Content/Audio are staged as NonUFS (DefaultGame.ini).
- Fixed: scenes chained via a choice's Next weren't marked played (homeroom replayed). Markers now re-arm only after you leave the area, so the next scene doesn't fire instantly where the last one ended.
- Verified (-autotest, logs + screenshots): ceremony -> homeroom, autosave written, slot0 save OK, talk scene with a nearby NPC, teleport tour (garden, station, store) with area titles, animated mannequins. Auto-tune measured 86 fps at High on the dev PC.

## 2026-11-13 (session 5b): colour menus, B/W gameplay, Rodin props, Y1 exams + endings
- Feedback (user): not enough time to read the dialogue and options; menus should be colourful while gameplay is grayscale, fading slowly into B/W on start/resume; "start creating all the models"; then "can't move anything".
- Reading time: the hesitation grace is now 3 s + 0.3 s/word of the prompt AND all options, and patience never drops below 8 s.
- Colour: the world has real palettes (cream walls, grass, brick, navy, leaf green; coloured uniforms; gold player). The unbound post-process animates saturation/contrast/vignette: menus (title + pause) are full colour, gameplay fades to B/W over 3 s (2 s on resume). The hub ticks while paused. Chessboard plaza under the fountain as the B/W nod. The menu restyle (gold title, teal buttons) sits on a left-side panel only.
- Models: all props via Rodin now (Tripo still Cloudflare-blocked). 18/18 generated (desk, chair, lockers, chalkboard, podium, bench, tree, vending machine, cafeteria table, gate, bookshelf, plant, basketball hoop, dorm bed, bus stop, street lamp, fountain, trophy case).
- Import: tools/import_assets.sh runs the editor with -ExecutePythonScript (PythonScriptPlugin, editor-only). The commandlet path asserts (the import syncs the Content Browser, which needs Slate), and renaming meshes mid-import crashed the render thread, so meshes stay at /Game/Generated/<id>/<id>/StaticMeshes/<id>. The editor segfaults on quit after saving; the assets are fine (90 files).
- Hub::Prop(): instanced generated meshes scaled to a target height, base on the ground, BlockAll collision, distance-culled; falls back to the blockout shapes if a model is missing.
- Grok: Y1 exam 2 (Group Project From Hell, 5 chained) and exam 3 (Cafeteria Stock Market, 5 chained) + 9 Y1 endings (canon Puppet as the fallback, plus Director, Ghost, Revolutionary, Dropout, Clown, Kingmaker, Mirror, and the Mocked dead end). 84 storylets validate.
- BUG (user: "can't move"): APlayerController::SetIgnoreMoveInput/LookInput are reference counted. Menus and scenes locked several times but unlocked once, so input stayed locked. Now we lock only if not already locked and unlock with ResetIgnoreInputFlags(). Verified in autotest: moveIgnored=0 after scenes. (The window open at the time was also the self-driving autotest.)

## 2026-11-13 (session 6, Fable 5.2): takeover, levelling core, ending tuning by simulation
- New agent took over from the docs + code alone (no chat history). Read DESIGN/DEVLOG, all narrative/UI source, and the content index; confirmed the project builds.
- Read the official jam page. Facts that change the plan: only packaged Windows/macOS/Android/iOS builds are accepted (no Linux); zip under 1 GB, named TeamName_GameName; a 30-60 s gameplay video is required; the dev log link is required for Glass Box; list every tool used. Judged 1-5 on ambition, theme, fun, gameplay, visuals, level design/progression, audio.
- Decisions (user): Windows build comes from the user's Windows dual-boot (UE 5.7.4 + VS 2022 to be installed there). Priority is finishing Year 1 as a complete, polished game; Y2/Y3 only if time remains.
- XAI_API_KEY in .env is empty, so Grok still runs through the Cursor CLI (tools/gen.sh), which works.
- Gaps found on takeover: player stats started at 0 and nothing showed them (the "levelling" was invisible); the game had no ending screen; ending conditions were untested (e.g. Sanity<=5 was always true at 0 stats).
- BUG fixed: a general stat is the mean of its sub-stats, so the first point in a brand-new sub-stat dragged the general DOWN. New sub-stats now start at their parent's value.
- Levelling: 6 origins rolled per run (Scholarship Admit, Legacy Heir, Sports Transfer, Second Chance, Lottery Admit, Nobody In Particular) set base stats; XP from every choice; each level costs Sanity; rank is computed against the 24 generated students; homeroom A-D.
- The System's forecast: before every choice the director commits to the option it expects (built-in average-student profile, replaced by the player's own record as it grows). The toast says MATCHED/MISSED and tracks how predictable you are. This is the core hook made visible: the System writes one ending for everyone, and beating its forecast is how you earn your own.
- EVAL app (Tab / Back): level, rank, XP, origin, 8 stat bars, forecast accuracy, projected ending, relationships with the cast you have met. HUD shows the next objective, level and rank.
- Ending screen: canon = "Theatrical Cut", hidden endings = "Director's Cut" (colour returns), dead end = "Cut from the film". Shows run stats, endings found N/9 (kept across runs), credits/tools, new run / title.
- tools/sim.py: a Python model of the director that plays thousands of synthetic students (average, masher, random, schemer, loner, rebel, clown, friend, traitor, contrarian) through the real content. First run showed the old conditions gave hidden endings to 56% of players. tools/tune_endings.py now owns the ending rules (new condition kinds: share:<tag>, pred, rush, level, rank). After tuning: average players get the canon ending 99% of the time, rushers hit the dead end, and each distinct style reliably earns its own ending (contrarian -> Director 100%, clown -> Clown 99%, loner -> Ghost 76%, rebel -> Revolutionary 66%).
- Rushing is measured for real: a choice made before the options could have been read counts as rushed; over 60% rushed -> dead end.
- Verified in-engine with -autotest -autoend (screenshots): EVAL app, HUD objective, ending screen.
