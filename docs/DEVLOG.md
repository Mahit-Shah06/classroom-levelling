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
