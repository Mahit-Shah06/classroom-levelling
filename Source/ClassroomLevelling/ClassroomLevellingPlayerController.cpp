// Copyright Epic Games, Inc. All Rights Reserved.


#include "ClassroomLevellingPlayerController.h"
#include "StoryPlayerComponent.h"
#include "Narrative/NarrativeSubsystem.h"
#include "World/HubWorld.h"
#include "UI/SMainMenu.h"
#include "UI/GamePrefs.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AudioDevice.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "ClassroomLevelling.h"
#include "Widgets/Input/SVirtualJoystick.h"

AClassroomLevellingPlayerController::AClassroomLevellingPlayerController()
{
	Story = CreateDefaultSubobject<UStoryPlayerComponent>(TEXT("Story"));
}

void AClassroomLevellingPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		FGamePrefs& P = FGamePrefs::Get();
		if (UGameUserSettings* GS = GEngine->GetGameUserSettings(); GS && !P.bBenchmarked)
		{
			// First launch: pick graphics settings that fit this PC (keeps low-spec machines playable).
			GS->RunHardwareBenchmark();
			GS->ApplyHardwareBenchmarkResults();
			GS->SaveSettings();
			P.bBenchmarked = true; P.Save();
		}
		if (FAudioDevice* AD = GEngine->GetMainAudioDeviceRaw()) AD->SetTransientPrimaryVolume(P.Volume);
		Story->OnPauseRequested = [this]() { TogglePause(); };
		ShowMenu(false);

		// Dev: -autotest = start, advance the story and take screenshots (Saved/Screenshots) for visual checks.
		if (FParse::Param(FCommandLine::Get(), TEXT("autotest")))
		{
			FTimerHandle H, H2;
			GetWorldTimerManager().SetTimer(H, [this]() { FScreenshotRequest::RequestScreenshot(true); }, 3.f, false);
			GetWorldTimerManager().SetTimer(H2, [this]()
			{
				static int32 N = 0;
				if (N++ == 0) { NewGame(); return; }
				if (Story->IsAwaitingChoice()) Story->Choose(N % 3); else Story->Continue();
				FTimerHandle H3; // shoot mid-line so typed text is visible
				GetWorldTimerManager().SetTimer(H3, []() { FScreenshotRequest::RequestScreenshot(true); }, 3.f, false);
			}, 5.f, true, 5.f);
		}
	}

	// only spawn touch controls on local player controllers
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogClassroomLevelling, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AClassroomLevellingPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AClassroomLevellingPlayerController::TogglePause).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, &AClassroomLevellingPlayerController::TogglePause).bExecuteWhenPaused = true;

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool AClassroomLevellingPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AClassroomLevellingPlayerController::ShowMenu(bool bPause)
{
	if (!GEngine || !GEngine->GameViewport) return;
	HideMenu();
	SAssignNew(Menu, SMainMenu).bPause(bPause)
		.OnPlay_Lambda([this, bPause]() { if (bPause) TogglePause(); else NewGame(); })
		.OnTitle_Lambda([this]() { UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this))); })
		.OnQuit_Lambda([this]() { UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false); });
	GEngine->GameViewport->AddViewportWidgetContent(Menu.ToSharedRef(), 50);
	Story->bBlocked = true;
	Story->SetHidden(true);
	SetShowMouseCursor(true);
	SetIgnoreMoveInput(true); SetIgnoreLookInput(true);
	FInputModeUIOnly M; M.SetWidgetToFocus(Menu); SetInputMode(M);
	Menu->FocusFirst();
	if (!bPause) if (AHubWorld* Hub = AHubWorld::Get(this)) Hub->MenuOrbit(this, true);
	if (bPause) SetPause(true);
}

void AClassroomLevellingPlayerController::HideMenu()
{
	if (Menu.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
	Menu.Reset();
}

void AClassroomLevellingPlayerController::NewGame()
{
	HideMenu();
	bInGame = true;
	Story->bBlocked = false;
	if (AHubWorld* Hub = AHubWorld::Get(this))
	{
		Hub->MenuOrbit(this, false);
		if (GetPawn()) GetPawn()->SetActorLocation(Hub->PlayerSpawn(), false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (UNarrativeSubsystem* N = GetGameInstance()->GetSubsystem<UNarrativeSubsystem>()) N->NewRun(FMath::Rand());
	SetShowMouseCursor(false);
	SetIgnoreMoveInput(false); SetIgnoreLookInput(false);
	SetInputMode(FInputModeGameOnly());
	Story->StartNext(); // opening cutscene (entrance ceremony)
}

void AClassroomLevellingPlayerController::TogglePause()
{
	if (!bInGame) return;
	if (Menu.IsValid())
	{
		HideMenu();
		SetPause(false);
		Story->bBlocked = false;
		if (Story->IsBusy()) Story->SetHidden(false);
		else { SetShowMouseCursor(false); SetIgnoreMoveInput(false); SetIgnoreLookInput(false); SetInputMode(FInputModeGameOnly()); }
	}
	else ShowMenu(true);
}

void AClassroomLevellingPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	HideMenu();
	Super::EndPlay(Reason);
}
