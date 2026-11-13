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

	// Best eligible storylet of a kind (None = any). Binds its slots.
	UFUNCTION(BlueprintCallable) bool PickStorylet(FName Kind, FStorylet& Out);
	UFUNCTION(BlueprintCallable) FText ResolveText(const FText& In) const;

	// Apply a choice; Witnesses interpret it. Hesitation = seconds the player took.
	UFUNCTION(BlueprintCallable) TArray<FNPCReaction> ApplyChoice(const FNarrativeChoice& Choice, const TArray<FName>& Witnesses, float Hesitation);

	UFUNCTION(BlueprintCallable) FNPCReaction Interpret(FName NPC, const TMap<FName, float>& Tags, float Hesitation);

	// Distance of player behaviour from the built-in average profile (0 = typical).
	UFUNCTION(BlueprintCallable) float Uniqueness() const;

private:
	FName SelectNPC(FName Selector);
	FRandomStream Rng;
	TSet<FName> Played;
	TMap<FName, float> AverageProfile;
};
