// Drives storylets on screen: "System window" UI (Slate, no binary assets), hesitation timer,
// NPC interpretation readout, screen shake + rumble on strong reactions.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NarrativeTypes.h"
#include "StoryPlayerComponent.generated.h"

class SStoryWindow;

UCLASS(ClassGroup = (Narrative), meta = (BlueprintSpawnableComponent))
class CLASSROOMLEVELLING_API UStoryPlayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStoryPlayerComponent();

	UFUNCTION(BlueprintCallable) void StartNext(FName Kind = NAME_None);
	UFUNCTION(BlueprintCallable) void Choose(int32 Index);
	UFUNCTION(BlueprintCallable) void Continue();
	UFUNCTION(BlueprintCallable) void Close();

	UPROPERTY(EditAnywhere) bool bAutoStart = true;
	bool IsAwaitingChoice() const { return bAwaitingChoice; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	class UNarrativeSubsystem* Narrative() const;
	void SetUIMode(bool bUI);
	void Shake(float Strength);
	TArray<FName> Witnesses() const;

	TSharedPtr<SStoryWindow> Window;
	FStorylet Current;
	TArray<int32> VisibleChoices; // indices into Current.Choices that pass Requires
	TArray<FName> CachedWitnesses;
	float CachedPatience = 10.f;
	float ChoiceTime = 0.f;
	float ShakeT = 0.f, ShakeAmp = 0.f;
	bool bAwaitingChoice = false;
	bool bWarnedHesitation = false;
};
