// Plays storylets: cutscene lines (camera shots + Piper voice + mouth flaps) in a dialogue box,
// then the prompt with choices on the box's right side (1/2/3 or A/B/X/Y depending on the active device).
// Hesitation timer starts only after the text is fully shown plus a reading grace period.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NarrativeTypes.h"
#include "StoryPlayerComponent.generated.h"

class SStoryWindow;
class UAudioComponent;

UCLASS(ClassGroup = (Narrative), meta = (BlueprintSpawnableComponent))
class CLASSROOMLEVELLING_API UStoryPlayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStoryPlayerComponent();

	UFUNCTION(BlueprintCallable) void StartNext(FName Kind = NAME_None);
	UFUNCTION(BlueprintCallable) void StartAt(FName Location);
	UFUNCTION(BlueprintCallable) void Choose(int32 Index);
	UFUNCTION(BlueprintCallable) void Continue();
	UFUNCTION(BlueprintCallable) void Close();

	bool IsAwaitingChoice() const { return bAwaitingChoice; }
	bool IsBusy() const { return Window.IsValid() || bBlocked; }
	bool bBlocked = false;              // set while menus are open
	TFunction<void()> OnPauseRequested; // Esc / Start inside a scene
	void SetHidden(bool bHide);

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	class UNarrativeSubsystem* Narrative() const;
	bool Begin(const FStorylet& S);
	void ShowStep();      // current line or the prompt
	void SetUIMode(bool bUI);
	void Shake(float Strength);
	void StopVoice();
	TArray<FName> Witnesses() const;
	int32 VoiceId(FName NPC) const;

	TSharedPtr<SStoryWindow> Window;
	FStorylet Current;
	FName PendingNext;
	int32 Step = 0;                 // 0..Lines.Num()-1 = lines, Lines.Num() = prompt, > = done
	TArray<FString> VoicePaths;     // per step
	TArray<FName> StepSpeakers;     // resolved npc per step (None = narrator/system)
	TArray<int32> VisibleChoices;   // indices into Current.Choices that pass Requires
	TArray<FName> CachedWitnesses;
	UPROPERTY() TObjectPtr<UAudioComponent> Voice;
	float CachedPatience = 10.f, ChoiceTime = 0.f, Grace = 0.f, VoiceWait = 0.f;
	float ShakeT = 0.f, ShakeAmp = 0.f;
	bool bAwaitingChoice = false, bWarnedHesitation = false, bResolved = false, bVoicePending = false;
};
