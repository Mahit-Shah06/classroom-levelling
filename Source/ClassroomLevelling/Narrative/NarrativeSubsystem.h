// Local "director": loads JSON content, evaluates conditions, NPC interpretation, slot filling.
// Zero runtime AI cost: all scoring is cheap arithmetic.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NarrativeTypes.h"
#include "NarrativeSubsystem.generated.h"

UCLASS()
class CLASSROOMLEVELLING_API UNarrativeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// --- State ---
	UPROPERTY(BlueprintReadOnly) FStatBlock Player;
	UPROPERTY(BlueprintReadOnly) TMap<FName, float> Relationship; // npc -> -100..100
	UPROPERTY(BlueprintReadOnly) TSet<FName> Flags;
	UPROPERTY(BlueprintReadOnly) TMap<FName, float> TagHistory;   // accumulated player tags (behaviour profile)
	UPROPERTY(BlueprintReadOnly) TMap<FName, FName> SlotBindings; // current storylet slot -> npc
	UPROPERTY(BlueprintReadOnly) TMap<FName, FName> RoleBindings; // rolled per run: role -> npc

	// "@x" -> slot binding, else role binding, else the name itself.
	UFUNCTION(BlueprintCallable) FName Resolve(FName IdOrRef) const;

	// --- Content ---
	UPROPERTY(BlueprintReadOnly) TMap<FName, FNPCProfile> NPCs;
	UPROPERTY(BlueprintReadOnly) TArray<FStorylet> Storylets;

	UFUNCTION(BlueprintCallable) void LoadContent();
	UFUNCTION(BlueprintCallable) void NewRun(int32 Seed);

	UFUNCTION(BlueprintCallable) bool CheckCondition(const FString& Cond) const;
	UFUNCTION(BlueprintCallable) bool CheckAll(const TArray<FString>& Conds) const;

	// Best eligible storylet of a kind/location (None = any). Pick binds its slots and marks it played.
	const FStorylet* FindStorylet(FName Kind, FName Location = NAME_None) const;
	UFUNCTION(BlueprintCallable) bool PickStorylet(FName Kind, FName Location, FStorylet& Out);
	FName SpeakerNPC(FName Speaker) const;   // "@rival" -> npc id (None for narrator/system)
	FString SpeakerName(FName Speaker) const; // display name ("" = narrator)
	UFUNCTION(BlueprintCallable) FText ResolveText(const FText& In) const;

	// Apply a choice; Witnesses interpret it. Hesitation = seconds the player took.
	UFUNCTION(BlueprintCallable) TArray<FNPCReaction> ApplyChoice(const FNarrativeChoice& Choice, const TArray<FName>& Witnesses, float Hesitation, bool bRushed = false);

	UFUNCTION(BlueprintCallable) FNPCReaction Interpret(FName NPC, const TMap<FName, float>& Tags, float Hesitation);

	// Distance of player behaviour from the built-in average profile (0 = typical).
	UFUNCTION(BlueprintCallable) float Uniqueness() const;

	// --- Levelling ---
	// Origin = why you were admitted: base stats + a skill flag. Rolled per run.
	UPROPERTY(BlueprintReadOnly) FName Origin;
	UPROPERTY(BlueprintReadOnly) FName Homeroom = "A";
	UPROPERTY(BlueprintReadOnly) int32 Level = 1;
	UPROPERTY(BlueprintReadOnly) float Xp = 0.f;
	int32 PredHits = 0, PredTotal = 0;   // how often the System called your choice in advance
	int32 ChoicesMade = 0, Rushed = 0;   // Rushed = answered before the options could have been read
	float TagShare(FName Tag) const;     // this tag's share of everything you have done (0..1)
	int32 PendingLevelUps = 0;           // consumed by the UI (level-up toast)
	float XpForNext() const { return 60.f + Level * 40.f; }
	float Score() const;                 // evaluation score used for rank
	int32 Rank() const;                  // 1 = top of the cohort
	int32 CohortSize() const;
	FString OriginTitle() const; FString OriginBlurb() const; FString OriginSkill() const;
	// The System's forecast: index into Visible of the choice it expects (-1 = no forecast).
	int32 Predict(const FStorylet& S, const TArray<int32>& Visible) const;
	void RecordPrediction(bool bHit) { ++PredTotal; if (bHit) ++PredHits; }
	float Predictability() const { return PredTotal ? float(PredHits) / PredTotal : 1.f; }
	FName ForecastEnding() const;        // the ending the System currently expects for you
	FName EndingReached() const;         // "puppet", "ghost"... (None while the run is live)

	// --- Run / save ---
	UPROPERTY(BlueprintReadOnly) FName PlayerGender = "m";
	UPROPERTY(BlueprintReadOnly) int32 RunSeed = 0;
	float PlayTime = 0.f;
	FName TalkTarget;
	void MarkPlayed(FName Id) { Played.Add(Id); } // NPC the player walked up to (fills "talk_target" slots)
	bool SaveSlot(const FString& Slot, const FVector& Loc, const FRotator& Rot, FName Area);
	class UCLSaveGame* LoadSlot(const FString& Slot); // restores narrative state; caller places the player
	static class UCLSaveGame* PeekSlot(const FString& Slot);

private:
	FName SelectNPC(FName Selector);
	void RollRoles(TArray<FName>& Pool, const FName* Roles, int32 Count);
	FRandomStream Rng;
	TSet<FName> Played;
	TMap<FName, float> AverageProfile;
};
