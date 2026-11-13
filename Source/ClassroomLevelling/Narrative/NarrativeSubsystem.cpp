#include "NarrativeSubsystem.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "CLSaveGame.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogNarrative, Log, All);

// ---------- FStatBlock ----------
static const TCHAR* GStatNames[] = { TEXT("Intellect"), TEXT("Charisma"), TEXT("Physique"), TEXT("Sanity"),
	TEXT("Cunning"), TEXT("Reputation"), TEXT("Funds"), TEXT("Luck") };

float FStatBlock::General(EGeneralStat S) const
{
	const FString Prefix = FString(GStatNames[(int32)S]) + TEXT(".");
	float Sum = 0.f; int32 N = 0;
	for (const auto& KV : Sub)
	{
		if (KV.Key.ToString().StartsWith(Prefix)) { Sum += KV.Value; ++N; }
	}
	return N ? Sum / N : 0.f;
}

float FStatBlock::Get(FName Key) const
{
	if (const float* V = Sub.Find(Key)) return *V;
	for (int32 i = 0; i < (int32)EGeneralStat::Count; ++i)
	{
		if (Key == GStatNames[i]) return General((EGeneralStat)i);
	}
	return 0.f;
}

void FStatBlock::Add(FName SubKey, float Delta)
{
	float* V = Sub.Find(SubKey);
	if (!V)
	{
		// A new sub-stat starts at its parent's level, so discovering one never drags the general down.
		FString Parent, Leaf;
		const float Base = SubKey.ToString().Split(TEXT("."), &Parent, &Leaf) ? Get(FName(*Parent)) : 0.f;
		V = &Sub.Add(SubKey, Base);
	}
	*V = FMath::Clamp(*V + Delta, 0.f, 100.f);
}

ESecretClass FStatBlock::SecretClass() const
{
	// Pair sums: Onyx=Int+Cun, Slate=Phy+San, Ash=Cha+Rep, Ivory=Fun+Luck
	const float S[4] = {
		General(EGeneralStat::Intellect) + General(EGeneralStat::Cunning),
		General(EGeneralStat::Physique) + General(EGeneralStat::Sanity),
		General(EGeneralStat::Charisma) + General(EGeneralStat::Reputation),
		General(EGeneralStat::Funds) + General(EGeneralStat::Luck) };
	int32 Best = 0;
	for (int32 i = 1; i < 4; ++i) if (S[i] > S[Best]) Best = i;
	return (ESecretClass)Best;
}

// ---------- Origins ----------
struct FOriginDef { const TCHAR* Id; const TCHAR* Title; const TCHAR* Blurb; const TCHAR* Skill; float Base[8]; };
// Base order: Intellect, Charisma, Physique, Sanity, Cunning, Reputation, Funds, Luck
static const FOriginDef GOrigins[] = {
	{ TEXT("scholar"),  TEXT("Scholarship Admit"),    TEXT("Top of a school nobody has heard of. Here, that makes you average."), TEXT("Cram: you think fastest when cornered."),      { 46, 30, 26, 36, 30, 24, 14, 30 } },
	{ TEXT("legacy"),   TEXT("Legacy Heir"),          TEXT("Your surname is on a building. The building is being audited."),      TEXT("Allowance: money appears when you need it."),  { 30, 34, 26, 28, 28, 38, 50, 34 } },
	{ TEXT("athlete"),  TEXT("Sports Transfer"),      TEXT("Recruited for a team that was dissolved the week you arrived."),      TEXT("Second Wind: you outlast everyone."),          { 24, 32, 50, 38, 24, 32, 22, 30 } },
	{ TEXT("expelled"), TEXT("Second Chance"),        TEXT("Expelled elsewhere. The file is sealed. Everyone has read it."),      TEXT("Read the Room: you notice when people lie."),  { 32, 30, 32, 30, 48, 16, 22, 32 } },
	{ TEXT("lottery"),  TEXT("Lottery Admit"),        TEXT("One seat a year is drawn at random. Nobody will tell you why."),      TEXT("Fluke: the dice like you."),                   { 28, 32, 28, 34, 26, 26, 24, 52 } },
	{ TEXT("nobody"),   TEXT("Nobody In Particular"), TEXT("No story. No talent. The evaluation files you under 'baseline'."),   TEXT("Forgettable: people overlook you."),           { 32, 32, 32, 36, 32, 30, 28, 32 } },
};
static const TCHAR* GBaseSubs[8][3] = {
	{ TEXT("Logic"), TEXT("Memory"), TEXT("Analysis") }, { TEXT("Presence"), TEXT("Persuasion"), TEXT("Charm") },
	{ TEXT("Strength"), TEXT("Endurance"), TEXT("Agility") }, { TEXT("Focus"), TEXT("Resilience"), TEXT("Clarity") },
	{ TEXT("Deception"), TEXT("Scheming"), TEXT("Insight") }, { TEXT("Prestige"), TEXT("Trust"), TEXT("Influence") },
	{ TEXT("Liquidity"), TEXT("Credit"), TEXT("Assets") }, { TEXT("Timing"), TEXT("Serendipity"), TEXT("Odds") } };
static const FOriginDef* FindOrigin(FName Id) { for (const FOriginDef& O : GOrigins) if (Id == O.Id) return &O; return nullptr; }

FString UNarrativeSubsystem::OriginTitle() const { const FOriginDef* O = FindOrigin(Origin); return O ? O->Title : TEXT(""); }
FString UNarrativeSubsystem::OriginBlurb() const { const FOriginDef* O = FindOrigin(Origin); return O ? O->Blurb : TEXT(""); }
FString UNarrativeSubsystem::OriginSkill() const { const FOriginDef* O = FindOrigin(Origin); return O ? O->Skill : TEXT(""); }

static float StatTotal(const FStatBlock& B) { float T = 0.f; for (int32 i = 0; i < (int32)EGeneralStat::Count; ++i) T += B.General((EGeneralStat)i); return T; }

// Levels weigh heavily: the school ranks growth it caused, not talent it was handed.
float UNarrativeSubsystem::Score() const { return StatTotal(Player) + (Level - 1) * 35.f; }

int32 UNarrativeSubsystem::CohortSize() const
{
	int32 N = 1;
	for (const auto& KV : NPCs) if (KV.Value.Kind == TEXT("student")) ++N;
	return N;
}

int32 UNarrativeSubsystem::Rank() const
{
	const float Mine = Score();
	int32 R = 1;
	for (const auto& KV : NPCs) if (KV.Value.Kind == TEXT("student") && StatTotal(KV.Value.Stats) > Mine) ++R;
	return R;
}

// The System's model of you: the built-in average student, replaced by your own record as it grows.
int32 UNarrativeSubsystem::Predict(const FStorylet& S, const TArray<int32>& Visible) const
{
	if (Visible.Num() < 2) return -1;
	float Total = 0.f;
	for (const auto& KV : TagHistory) if (KV.Key != TEXT("hesitation")) Total += FMath::Max(0.f, KV.Value);
	const float W = Total / (Total + 4.f);
	int32 Best = -1; float BestScore = -MAX_flt;
	for (int32 i = 0; i < Visible.Num(); ++i)
	{
		if (!S.Choices.IsValidIndex(Visible[i])) continue;
		float Sc = 0.f;
		for (const auto& T : S.Choices[Visible[i]].Tags)
		{
			const float Mine = Total > 0.f ? FMath::Max(0.f, TagHistory.FindRef(T.Key)) / Total : 0.f;
			Sc += T.Value * (W * Mine + (1.f - W) * AverageProfile.FindRef(T.Key));
		}
		if (Sc > BestScore) { BestScore = Sc; Best = i; }
	}
	return Best;
}

float UNarrativeSubsystem::TagShare(FName Tag) const
{
	float Total = 0.f;
	for (const auto& KV : TagHistory) if (KV.Key != TEXT("hesitation")) Total += FMath::Max(0.f, KV.Value);
	return Total > 0.f ? FMath::Max(0.f, TagHistory.FindRef(Tag)) / Total : 0.f;
}

FName UNarrativeSubsystem::EndingReached() const
{
	for (const FName& F : Flags) { FString S = F.ToString(); if (S.RemoveFromStart(TEXT("ending_"))) return FName(*S); }
	return NAME_None;
}

// What the System expects you to become: the best ending whose non-progress conditions already hold.
FName UNarrativeSubsystem::ForecastEnding() const
{
	FName Best = "puppet"; float BestP = -MAX_flt;
	for (const FStorylet& S : Storylets)
	{
		if (!S.Id.ToString().StartsWith(TEXT("end_y1_")) || !S.Choices.Num()) continue;
		FString Id;
		for (const FName& F : S.Choices[0].SetFlags) { FString T = F.ToString(); if (T.RemoveFromStart(TEXT("ending_"))) Id = T; }
		if (Id.IsEmpty()) continue;
		bool bOk = true;
		for (const FString& C : S.Conditions) if (!C.Contains(TEXT("flag:")) && !CheckCondition(C)) { bOk = false; break; }
		const float P = S.Priority + S.Conditions.Num() * 0.5f;
		if (bOk && P > BestP) { BestP = P; Best = FName(*Id); }
	}
	return Best;
}

// ---------- Subsystem ----------
void UNarrativeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Built-in "average player" tag profile (normalized weights). Tuned later with real data.
	AverageProfile = { {"obedience", 0.30f}, {"empathy", 0.25f}, {"ambition", 0.20f}, {"cunning", 0.10f},
		{"betrayal", 0.05f}, {"chaos", 0.05f}, {"withdrawal", 0.05f} };
	LoadContent();
	NewRun(FMath::Rand());
}

void UNarrativeSubsystem::LoadContent()
{
	NPCs.Reset(); Storylets.Reset();
	const FString Dir = FPaths::ProjectContentDir() / TEXT("Data");
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Dir, TEXT("*.json"), true, false);
	for (const FString& F : Files)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *F)) continue;
		if (FPaths::GetCleanFilename(F).StartsWith(TEXT("npc")))
		{
			TArray<FNPCProfile> Arr;
			if (FJsonObjectConverter::JsonArrayStringToUStruct(Json, &Arr))
				for (FNPCProfile& N : Arr) NPCs.Add(N.Id, N);
			else UE_LOG(LogNarrative, Warning, TEXT("Bad NPC json: %s"), *F);
		}
		else
		{
			TArray<FStorylet> Arr;
			if (FJsonObjectConverter::JsonArrayStringToUStruct(Json, &Arr)) Storylets.Append(Arr);
			else UE_LOG(LogNarrative, Warning, TEXT("Bad storylet json: %s"), *F);
		}
	}
	UE_LOG(LogNarrative, Log, TEXT("Loaded %d NPCs, %d storylets"), NPCs.Num(), Storylets.Num());
}

void UNarrativeSubsystem::NewRun(int32 Seed)
{
	Rng.Initialize(Seed);
	RunSeed = Seed; PlayTime = 0.f;
	PlayerGender = Rng.FRand() < 0.5f ? FName("f") : FName("m"); // random body each run
	Player = FStatBlock(); Relationship.Reset(); Flags.Reset(); TagHistory.Reset(); Played.Reset(); SlotBindings.Reset();
	RoleBindings.Reset();
	for (auto& KV : NPCs) { Relationship.Add(KV.Key, 0.f); KV.Value.Role = TEXT("bg"); }

	// Roll the cast: every run, any student can fill any key role (personality fit preferred).
	TArray<FName> Pool;
	for (const auto& KV : NPCs) if (KV.Value.Kind == TEXT("student")) Pool.Add(KV.Key);
	for (int32 i = Pool.Num() - 1; i > 0; --i) Pool.Swap(i, Rng.RandRange(0, i));
	static const FName Roles[] = { "rival", "ally", "gremlin", "president", "ex1", "broker" };
	RollRoles(Pool, Roles, UE_ARRAY_COUNT(Roles));

	// Staff roles come from teachers/staff (fixed set, assignment shuffled per run).
	Pool.Reset();
	for (const auto& KV : NPCs) if (KV.Value.Kind != TEXT("student")) Pool.Add(KV.Key);
	for (int32 i = Pool.Num() - 1; i > 0; --i) Pool.Swap(i, Rng.RandRange(0, i));
	static const FName Staff[] = { "head", "homeroom", "proctor", "nurse", "coach", "counsellor" };
	RollRoles(Pool, Staff, UE_ARRAY_COUNT(Staff));

	// Origin: why you were admitted. Sets base stats (general +/- a little per sub-stat) and a skill flag.
	Level = 1; Xp = 0.f; PredHits = PredTotal = 0; PendingLevelUps = 0; ChoicesMade = Rushed = 0;
	const FOriginDef& O = GOrigins[Rng.RandRange(0, UE_ARRAY_COUNT(GOrigins) - 1)];
	Origin = O.Id;
	Homeroom = FName(*FString::Chr(TCHAR('A' + Rng.RandRange(0, 3))));
	for (int32 s = 0; s < 8; ++s) for (int32 k = 0; k < 3; ++k)
		Player.Sub.Add(FName(*FString::Printf(TEXT("%s.%s"), GStatNames[s], GBaseSubs[s][k])), FMath::Clamp(O.Base[s] + Rng.FRandRange(-6.f, 6.f), 1.f, 100.f));
	Flags.Add(FName(*FString::Printf(TEXT("origin_%s"), O.Id)));
}

void UNarrativeSubsystem::RollRoles(TArray<FName>& Pool, const FName* Roles, int32 Count)
{
	for (int32 r = 0; r < Count; ++r)
	{
		const FName Role = Roles[r];
		int32 Pick = Pool.IndexOfByPredicate([&](FName Id) { const auto& F = NPCs[Id].RoleFit; return F.Num() == 0 || F.Contains(Role); });
		if (Pick == INDEX_NONE) Pick = Pool.Num() ? 0 : INDEX_NONE;
		if (Pick == INDEX_NONE) break;
		RoleBindings.Add(Role, Pool[Pick]);
		NPCs[Pool[Pick]].Role = Role;
		Pool.RemoveAt(Pick);
	}
}

FName UNarrativeSubsystem::Resolve(FName IdOrRef) const
{
	const FString S = IdOrRef.ToString();
	if (!S.StartsWith(TEXT("@"))) return IdOrRef;
	const FName Ref(S.Mid(1));
	if (const FName* B = SlotBindings.Find(Ref)) return *B;
	if (const FName* R = RoleBindings.Find(Ref)) return *R;
	return NAME_None;
}

// Grammar: [!]flag:name | stat:Key OP N | rel:npc OP N | tag:name OP N | uniq OP N   (OP: >= <= > < ==)
bool UNarrativeSubsystem::CheckCondition(const FString& InCond) const
{
	FString Cond = InCond.TrimStartAndEnd();
	const bool bNot = Cond.RemoveFromStart(TEXT("!"));
	bool Result = false;

	if (Cond.StartsWith(TEXT("flag:")))
	{
		Result = Flags.Contains(FName(Cond.Mid(5)));
	}
	else
	{
		static const TCHAR* Ops[] = { TEXT(">="), TEXT("<="), TEXT("=="), TEXT(">"), TEXT("<") };
		int32 OpIdx = -1, Pos = INDEX_NONE;
		for (int32 i = 0; i < 5 && Pos == INDEX_NONE; ++i) { Pos = Cond.Find(Ops[i]); if (Pos != INDEX_NONE) OpIdx = i; }
		if (Pos == INDEX_NONE) { UE_LOG(LogNarrative, Warning, TEXT("Bad condition: %s"), *InCond); return false; }

		const FString Lhs = Cond.Left(Pos).TrimEnd();
		const float Rhs = FCString::Atof(*Cond.Mid(Pos + FCString::Strlen(Ops[OpIdx])));
		FString Kind, Key;
		if (!Lhs.Split(TEXT(":"), &Kind, &Key)) Kind = Lhs;

		float L = 0.f;
		if (Kind == TEXT("stat")) L = Player.Get(FName(Key));
		else if (Kind == TEXT("rel"))
		{
			const FName Npc = Resolve(FName(Key));
			const float* R = Relationship.Find(Npc); L = R ? *R : 0.f;
		}
		else if (Kind == TEXT("tag")) { const float* T = TagHistory.Find(FName(Key)); L = T ? *T : 0.f; }
		else if (Kind == TEXT("uniq")) L = Uniqueness();
		else if (Kind == TEXT("pred")) L = Predictability();
		else if (Kind == TEXT("share")) L = TagShare(FName(Key));
		else if (Kind == TEXT("rush")) L = ChoicesMade ? float(Rushed) / ChoicesMade : 0.f;
		else if (Kind == TEXT("level")) L = Level;
		else if (Kind == TEXT("rank")) L = Rank();

		switch (OpIdx)
		{
		case 0: Result = L >= Rhs; break;
		case 1: Result = L <= Rhs; break;
		case 2: Result = FMath::IsNearlyEqual(L, Rhs); break;
		case 3: Result = L > Rhs; break;
		default: Result = L < Rhs; break;
		}
	}
	return bNot ? !Result : Result;
}

bool UNarrativeSubsystem::CheckAll(const TArray<FString>& Conds) const
{
	for (const FString& C : Conds) if (!CheckCondition(C)) return false;
	return true;
}

FName UNarrativeSubsystem::SelectNPC(FName Selector)
{
	const FString S = Selector.ToString();
	FName Best = NAME_None;
	if (S == TEXT("best_friend") || S == TEXT("worst_enemy"))
	{
		const float Sign = S == TEXT("best_friend") ? 1.f : -1.f;
		float BestV = -MAX_flt;
		for (const auto& KV : Relationship) if (KV.Value * Sign > BestV) { BestV = KV.Value * Sign; Best = KV.Key; }
	}
	else if (S == TEXT("talk_target")) Best = TalkTarget;
	else if (S.StartsWith(TEXT("role:")))
	{
		Best = RoleBindings.FindRef(FName(S.Mid(5)));
	}
	if (Best.IsNone() && NPCs.Num()) // random_npc or fallback
	{
		TArray<FName> Keys; NPCs.GetKeys(Keys);
		Best = Keys[Rng.RandRange(0, Keys.Num() - 1)];
	}
	return Best;
}

const FStorylet* UNarrativeSubsystem::FindStorylet(FName Kind, FName Location) const
{
	const FStorylet* Best = nullptr;
	float BestScore = -MAX_flt;
	for (const FStorylet& S : Storylets)
	{
		if (!Kind.IsNone() && S.Kind != Kind) continue;
		if (!Location.IsNone() && S.Location != Location) continue;
		if (Kind.IsNone() && S.Kind == TEXT("talk")) continue; // talk only on demand
		if (S.bOnce && Played.Contains(S.Id)) continue;
		if (!CheckAll(S.Conditions)) continue;
		float Score = S.Priority + S.Conditions.Num() * 0.5f; // specific > generic
		if (S.Kind == TEXT("talk")) Score += Rng.FRand() * 15.f; // variety in small talk
		if (Score > BestScore) { BestScore = Score; Best = &S; }
	}
	return Best;
}

bool UNarrativeSubsystem::PickStorylet(FName Kind, FName Location, FStorylet& Out)
{
	const FStorylet* Best = FindStorylet(Kind, Location);
	if (!Best) return false;
	SlotBindings.Reset();
	for (const auto& KV : Best->Slots) SlotBindings.Add(KV.Key, SelectNPC(KV.Value));
	Played.Add(Best->Id);
	Out = *Best;
	return true;
}

FName UNarrativeSubsystem::SpeakerNPC(FName Speaker) const
{
	const FName Id = Resolve(Speaker);
	return NPCs.Contains(Id) ? Id : NAME_None;
}

FString UNarrativeSubsystem::SpeakerName(FName Speaker) const
{
	if (Speaker.IsNone() || Speaker == TEXT("narrator")) return FString();
	if (Speaker == TEXT("system")) return TEXT("EVAL // Notification");
	const FNPCProfile* P = NPCs.Find(SpeakerNPC(Speaker));
	return P ? P->Name.ToString() : TEXT("???");
}

FText UNarrativeSubsystem::ResolveText(const FText& In) const
{
	FString S = In.ToString();
	TMap<FName, FName> All = RoleBindings;
	All.Append(SlotBindings);
	for (const auto& KV : All)
	{
		const FNPCProfile* N = NPCs.Find(KV.Value);
		S.ReplaceInline(*FString::Printf(TEXT("{%s}"), *KV.Key.ToString()), N ? *N->Name.ToString() : TEXT("someone"));
	}
	return FText::FromString(S);
}

FNPCReaction UNarrativeSubsystem::Interpret(FName NPC, const TMap<FName, float>& Tags, float Hesitation)
{
	FNPCReaction R; R.NPC = NPC;
	const FNPCProfile* P = NPCs.Find(NPC);
	if (!P) return R;

	TArray<FName> Known; P->Values.GetKeys(Known);
	const float Rel = Relationship.FindRef(NPC);
	// Misread chance: low perception -> more; player luck reduces it.
	const float MisreadP = FMath::Clamp((1.f - P->Perception) * 0.5f - Player.General(EGeneralStat::Luck) * 0.002f, 0.f, 0.6f);

	float Strongest = 0.f;
	for (const auto& T : Tags)
	{
		FName Tag = T.Key;
		if (Known.Num() > 1 && Rng.FRand() < MisreadP)
		{
			Tag = Known[Rng.RandRange(0, Known.Num() - 1)];
			R.bMisread |= Tag != T.Key;
		}
		const float D = P->Values.FindRef(Tag) * T.Value * 10.f;
		R.Delta += D;
		if (FMath::Abs(D) > Strongest) { Strongest = FMath::Abs(D); R.PerceivedTag = Tag; }
	}
	// Existing feelings colour the reading: friends forgive, enemies hold grudges.
	R.Delta *= (R.Delta >= 0.f) ? (1.f + Rel * 0.005f) : (1.f - Rel * 0.005f);
	if (Hesitation > P->Patience) R.Delta -= (Hesitation - P->Patience) * 0.5f;
	return R;
}

TArray<FNPCReaction> UNarrativeSubsystem::ApplyChoice(const FNarrativeChoice& Choice, const TArray<FName>& Witnesses, float Hesitation, bool bRushed)
{
	++ChoicesMade; if (bRushed) ++Rushed;
	// Authored deltas are small (0.5-3); scale them so a year of choices visibly moves the bars.
	const float GainScale = 3.f;
	float Gain = 6.f;
	for (const auto& KV : Choice.StatDeltas) { Player.Add(KV.Key, KV.Value * GainScale); Gain += FMath::Abs(KV.Value) * GainScale * 4.f; }
	Xp += Gain;
	while (Xp >= XpForNext())
	{
		Xp -= XpForNext(); ++Level; ++PendingLevelUps;
		Player.Add(TEXT("Sanity.Resilience"), -2.5f); // every level costs something
	}
	for (const auto& KV : Choice.Tags) TagHistory.FindOrAdd(KV.Key) += KV.Value;
	TagHistory.FindOrAdd(TEXT("hesitation")) += Hesitation;
	for (const FName& F : Choice.SetFlags) Flags.Add(F);

	TArray<FNPCReaction> Out;
	for (FName W : Witnesses)
	{
		W = Resolve(W);
		if (W.IsNone()) continue;
		FNPCReaction R = Interpret(W, Choice.Tags, Hesitation);
		float& Rel = Relationship.FindOrAdd(W);
		Rel = FMath::Clamp(Rel + R.Delta, -100.f, 100.f);
		Out.Add(R);
	}
	return Out;
}

float UNarrativeSubsystem::Uniqueness() const
{
	float Total = 0.f;
	for (const auto& KV : TagHistory) if (KV.Key != TEXT("hesitation")) Total += FMath::Max(0.f, KV.Value);
	if (Total <= 0.f) return 0.f;
	float D2 = 0.f;
	TSet<FName> Keys;
	for (const auto& KV : AverageProfile) Keys.Add(KV.Key);
	for (const auto& KV : TagHistory) if (KV.Key != TEXT("hesitation")) Keys.Add(KV.Key);
	for (const FName& K : Keys)
	{
		const float P = FMath::Max(0.f, TagHistory.FindRef(K)) / Total;
		const float A = AverageProfile.FindRef(K);
		D2 += FMath::Square(P - A);
	}
	return FMath::Sqrt(D2); // 0..~1.4
}

bool UNarrativeSubsystem::SaveSlot(const FString& Slot, const FVector& Loc, const FRotator& Rot, FName Area)
{
	UCLSaveGame* G = Cast<UCLSaveGame>(UGameplayStatics::CreateSaveGameObject(UCLSaveGame::StaticClass()));
	G->Saved = FDateTime::Now(); G->PlayTime = PlayTime; G->Area = Area;
	G->PlayerLocation = Loc; G->PlayerRotation = Rot; G->PlayerGender = PlayerGender; G->Seed = RunSeed;
	G->Player = Player; G->Relationship = Relationship; G->Flags = Flags; G->TagHistory = TagHistory;
	G->RoleBindings = RoleBindings; G->Played = Played;
	G->Origin = Origin; G->Homeroom = Homeroom; G->Level = Level; G->Xp = Xp; G->PredHits = PredHits; G->PredTotal = PredTotal; G->ChoicesMade = ChoicesMade; G->Rushed = Rushed;
	return UGameplayStatics::SaveGameToSlot(G, Slot, 0);
}

UCLSaveGame* UNarrativeSubsystem::PeekSlot(const FString& Slot)
{
	return UGameplayStatics::DoesSaveGameExist(Slot, 0) ? Cast<UCLSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)) : nullptr;
}

UCLSaveGame* UNarrativeSubsystem::LoadSlot(const FString& Slot)
{
	UCLSaveGame* G = PeekSlot(Slot);
	if (!G) return nullptr;
	NewRun(G->Seed);
	PlayTime = G->PlayTime; PlayerGender = G->PlayerGender;
	if (!G->Origin.IsNone()) { Origin = G->Origin; Homeroom = G->Homeroom; }
	Level = FMath::Max(1, G->Level); Xp = G->Xp; PredHits = G->PredHits; PredTotal = G->PredTotal; ChoicesMade = G->ChoicesMade; Rushed = G->Rushed;
	Player = G->Player; Flags = G->Flags; TagHistory = G->TagHistory; Played = G->Played;
	for (const auto& KV : G->Relationship) Relationship.Add(KV.Key, KV.Value);
	RoleBindings.Reset();
	for (auto& KV : NPCs) KV.Value.Role = TEXT("bg");
	for (const auto& KV : G->RoleBindings) if (FNPCProfile* P = NPCs.Find(KV.Value)) { RoleBindings.Add(KV.Key, KV.Value); P->Role = KV.Key; }
	return G;
}
