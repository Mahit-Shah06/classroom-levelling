// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ClassroomLevellingPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class AClassroomLevellingPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AClassroomLevellingPlayerController();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Narrative")
	TObjectPtr<class UStoryPlayerComponent> Story;

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	// Title / pause menus, saves, HUD, sprint, talk
public:
	void ShowMenu(bool bPause);
	void HideMenu();
	void NewGame();
	void TogglePause();
	void LoadGame(const FString& Slot);
	void SaveGame(const FString& Slot);
	void Autosave();
	void ApplyPrefs();
protected:
	void EnterPlay();
	void SprintDown(); void SprintUp();
	void Interact();
	void ToggleApp();
	void ShowEnding();
	TSharedPtr<class SEvalApp> App;
	TSharedPtr<class SEndScreen> EndScreen;
	void ApplyPlayerBody();
	virtual void PlayerTick(float DeltaTime) override;
	TSharedPtr<class SMainMenu> Menu;
	TSharedPtr<class SGameHud> Hud;
	bool bInGame = false, bSprinting = false;
	float StepDist = 0.f, AreaT = 0.f, FpsAcc = 0.f; int32 FpsFrames = 0;
	FName LastArea;
	int32 LastRank = 0;
	bool bAutoTune = false; float TuneT = 0.f; int32 TuneFrames = 0;
public:
	void AutoTuneTick();
protected:
	FVector PendingLoc; FRotator PendingRot; bool bPendingPlace = false;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

};
