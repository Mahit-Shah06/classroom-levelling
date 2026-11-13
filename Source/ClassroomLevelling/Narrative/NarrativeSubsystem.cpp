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
	float& V = Sub.FindOrAdd(SubKey);
	V = FMath::Clamp(V + Delta, 0.f, 100.f);
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

TArray<FNPCReaction> UNarrativeSubsystem::ApplyChoice(const FNarrativeChoice& Choice, const TArray<FName>& Witnesses, float Hesitation)
{
	for (const auto& KV : Choice.StatDeltas) Player.Add(KV.Key, KV.Value);
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
	Player = G->Player; Flags = G->Flags; TagHistory = G->TagHistory; Played = G->Played;
	for (const auto& KV : G->Relationship) Relationship.Add(KV.Key, KV.Value);
	RoleBindings.Reset();
	for (auto& KV : NPCs) KV.Value.Role = TEXT("bg");
	for (const auto& KV : G->RoleBindings) if (FNPCProfile* P = NPCs.Find(KV.Value)) { RoleBindings.Add(KV.Key, KV.Value); P->Role = KV.Key; }
	return G;
}
