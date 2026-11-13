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
#include "Narrative/GameAudio.h"
#include "Narrative/CLSaveGame.h"
#include "World/NPCStandIn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"

// Minimal in-game HUD: area title (fades), interact prompt, optional FPS.
class SGameHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGameHud) {}
	SLATE_END_ARGS()
	void Construct(const FArguments&)
	{
		auto F = [](const char* S, int32 Z) { return FCoreStyle::GetDefaultFontStyle(S, Z); };
		ChildSlot
		[
			SNew(SOverlay).Visibility(EVisibility::HitTestInvisible)
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(48, 40)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ SAssignNew(Area, STextBlock).Font(F("Bold", 30)).ColorAndOpacity(FLinearColor::White).ShadowOffset(FVector2D(1, 1)) ]
				+ SVerticalBox::Slot().AutoHeight()[ SAssignNew(Sub, STextBlock).Font(F("Italic", 14)).ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f)).ShadowOffset(FVector2D(1, 1)) ]
			]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(24, 16)
			[ SAssignNew(Fps, STextBlock).Font(F("Mono", 12)).ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f)) ]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0, 0, 0, 120)
			[
				SAssignNew(PromptBox, SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0, 0, 0, 0.7f)).Padding(FMargin(14, 8)).Visibility(EVisibility::Collapsed)
				[ SAssignNew(Prompt, STextBlock).Font(F("Regular", 16)).ColorAndOpacity(FLinearColor::White) ]
			]
		];
	}
	void ShowArea(const FString& Title, const FString& Subtitle) { Area->SetText(FText::FromString(Title.ToUpper())); Sub->SetText(FText::FromString(Subtitle)); AreaT = 4.f; }
	void SetPrompt(const FString& S) { PromptBox->SetVisibility(S.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible); Prompt->SetText(FText::FromString(S)); }
	void SetFps(const FString& S) { Fps->SetText(FText::FromString(S)); }
	virtual void Tick(const FGeometry& G, const double T, const float Dt) override
	{
		AreaT = FMath::Max(AreaT - Dt, 0.f);
		const float A = FMath::Clamp(AreaT, 0.f, 1.f);
		Area->SetRenderOpacity(A); Sub->SetRenderOpacity(A);
	}
private:
	TSharedPtr<STextBlock> Area, Sub, Prompt, Fps;
	TSharedPtr<SBorder> PromptBox;
	float AreaT = 0.f;
};

static const TMap<FName, FString>& AreaSubtitles()
{
	static const TMap<FName, FString> M = {
		{ "station", TEXT("Bus stop. The only road out.") }, { "gate", TEXT("Main gate. Cameras watch both ways.") },
		{ "courtyard", TEXT("Where rankings are whispered.") }, { "corridor", TEXT("Main building, ground floor.") },
		{ "hall", TEXT("Assembly hall.") }, { "classroom", TEXT("Homeroom. Seats ordered by rank.") },
		{ "office", TEXT("Staff office. Knock first.") }, { "cafeteria", TEXT("Meals priced in points.") },
		{ "rooftop", TEXT("Officially off-limits.") }, { "store", TEXT("Convenience store. Accepts points, sells secrets.") },
		{ "library", TEXT("Quiet. Mostly.") }, { "gym", TEXT("Physique is a stat too.") }, { "field", TEXT("Sports field and track.") },
		{ "dorm", TEXT("Dormitory. Walls are thin.") }, { "garden", TEXT("The pond remembers everything.") } };
	return M;
}
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
		// First launch: start at High; PlayerTick auto-tunes down while the title screen runs (low-spec safe).
		// (RunHardwareBenchmark hangs at frame 0 on some GPUs, so it is not used.)
		if (UGameUserSettings* GS = GEngine->GetGameUserSettings(); GS && !P.bBenchmarked) { GS->SetOverallScalabilityLevel(2); GS->ApplySettings(false); bAutoTune = true; }
		Story->OnPauseRequested = [this]() { TogglePause(); };
		Story->OnSceneEnded = [this]() { Autosave(); };
		ApplyPrefs();
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
				static const FName Tour[] = { "gym", "field", "library", "dorm", "garden", "station", "store" };
				UE_LOG(LogTemp, Log, TEXT("Autotest tick %d busy=%d awaiting=%d pawn=%s menu=%d moveIgnored=%d lookIgnored=%d"), N, Story->IsBusy(), Story->IsAwaitingChoice(), GetPawn() ? *GetPawn()->GetActorLocation().ToCompactString() : TEXT("none"), Menu.IsValid(), IsMoveInputIgnored(), IsLookInputIgnored());
				if (N >= 12 && !Story->IsBusy())
				{
					const int32 T = N - 12;
					if (T == 0) { SaveGame(TEXT("slot0")); UE_LOG(LogTemp, Log, TEXT("Autotest: saved slot0 -> %s"), UNarrativeSubsystem::PeekSlot(TEXT("slot0")) ? TEXT("ok") : TEXT("FAILED")); }
					else if (T == 1) { if (AHubWorld* Hub = AHubWorld::Get(this)) if (const ANPCStandIn* Np = Hub->NearestNamed(GetPawn()->GetActorLocation(), 1e6f)) { GetPawn()->SetActorLocation(Np->GetActorLocation() + FVector(-150, 0, 20)); Story->StartTalk(Np->Id); } }
					else if (T - 2 < (int32)UE_ARRAY_COUNT(Tour)) { if (AHubWorld* Hub = AHubWorld::Get(this)) if (const FHubArea* A = Hub->Area(Tour[T - 2])) { GetPawn()->SetActorLocation(A->Center + FVector(-600, 0, 120)); SetControlRotation(FRotator(-10, 0, 0)); } }
				}
				else if (Story->IsAwaitingChoice()) Story->Choose(N % 3); else Story->Continue();
				FTimerHandle H3; // shoot mid-line so typed text is visible
				GetWorldTimerManager().SetTimer(H3, []() { FScreenshotRequest::RequestScreenshot(true); }, 3.f, false);
			}, 3.f, true, 5.f);
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
	InputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AClassroomLevellingPlayerController::SprintDown);
	InputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &AClassroomLevellingPlayerController::SprintUp);
	InputComponent->BindKey(EKeys::Gamepad_LeftThumbstick, IE_Pressed, this, &AClassroomLevellingPlayerController::SprintDown);
	InputComponent->BindKey(EKeys::Gamepad_LeftThumbstick, IE_Released, this, &AClassroomLevellingPlayerController::SprintUp);
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AClassroomLevellingPlayerController::Interact);
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, &AClassroomLevellingPlayerController::Interact);

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
	FMenuActions A;
	A.NewGame = [this]() { NewGame(); };
	A.Resume = [this]() { TogglePause(); };
	A.Title = [this]() { Autosave(); UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this))); };
	A.Quit = [this]() { if (bInGame) Autosave(); UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false); };
	A.ApplyPrefs = [this]() { ApplyPrefs(); };
	A.Load = [this](const FString& S) { LoadGame(S); };
	A.Save = [this](const FString& S) { SaveGame(S); };
	Menu = SNew(SMainMenu, A).bPause(bPause);
	GEngine->GameViewport->AddViewportWidgetContent(Menu.ToSharedRef(), 50);
	Story->bBlocked = true;
	Story->SetHidden(true);
	if (AHubWorld* Hub = AHubWorld::Get(this)) Hub->SetGrayscale(false, 0.8f); // menus are in full colour
	if (Hud.IsValid()) Hud->SetVisibility(EVisibility::Collapsed);
	SetShowMouseCursor(true);
	if (!IsMoveInputIgnored()) SetIgnoreMoveInput(true); if (!IsLookInputIgnored()) SetIgnoreLookInput(true);
	FInputModeUIOnly M; M.SetWidgetToFocus(Menu); SetInputMode(M);
	Menu->FocusFirst();
	if (!bPause)
	{
		if (AHubWorld* Hub = AHubWorld::Get(this)) Hub->MenuOrbit(this, true);
		if (UGameAudio* Au = UGameAudio::Get(this)) { Au->PlayMusic(this, "music_menu"); Au->PlayAmbient(this, "amb_campus"); }
	}
	if (bPause) SetPause(true);
}

void AClassroomLevellingPlayerController::HideMenu()
{
	if (Menu.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
	Menu.Reset();
}

void AClassroomLevellingPlayerController::ApplyPlayerBody()
{
	const UNarrativeSubsystem* N = GetGameInstance()->GetSubsystem<UNarrativeSubsystem>();
	if (ACharacter* C = Cast<ACharacter>(GetPawn())) ANPCStandIn::ApplyBody(C->GetMesh(), N && N->PlayerGender == TEXT("f"), FLinearColor(0.95f, 0.72f, 0.30f)); // gold: the player stands out
}

// Shared by New Game and Load: close menus, hand control to the player.
void AClassroomLevellingPlayerController::EnterPlay()
{
	if (IsPaused()) SetPause(false);
	HideMenu();
	bInGame = true;
	Story->bBlocked = false;
	if (AHubWorld* Hub = AHubWorld::Get(this)) Hub->MenuOrbit(this, false);
	ApplyPlayerBody();
	ApplyPrefs();
	if (!Hud.IsValid() && GEngine && GEngine->GameViewport) { SAssignNew(Hud, SGameHud); GEngine->GameViewport->AddViewportWidgetContent(Hud.ToSharedRef(), 5); }
	if (Hud.IsValid()) Hud->SetVisibility(EVisibility::HitTestInvisible);
	if (UGameAudio* Au = UGameAudio::Get(this)) Au->PlayMusic(this, "music_hub");
	if (AHubWorld* Hub = AHubWorld::Get(this)) Hub->SetGrayscale(true, 3.f); // slow fade into the B/W world
	SetShowMouseCursor(false);
	ResetIgnoreInputFlags(); // counters: fully unlock
	SetInputMode(FInputModeGameOnly());
	LastArea = NAME_None;
}

void AClassroomLevellingPlayerController::NewGame()
{
	if (UNarrativeSubsystem* N = GetGameInstance()->GetSubsystem<UNarrativeSubsystem>()) N->NewRun(FMath::Rand());
	if (AHubWorld* Hub = AHubWorld::Get(this))
		if (GetPawn()) GetPawn()->SetActorLocation(Hub->PlayerSpawn(), false, nullptr, ETeleportType::TeleportPhysics);
	EnterPlay();
	Story->StartNext(); // opening cutscene (entrance ceremony)
}

void AClassroomLevellingPlayerController::LoadGame(const FString& Slot)
{
	UNarrativeSubsystem* N = GetGameInstance()->GetSubsystem<UNarrativeSubsystem>();
	UCLSaveGame* G = N ? N->LoadSlot(Slot) : nullptr;
	if (!G) return;
	if (Story->IsBusy()) Story->Close();
	EnterPlay();
	if (APawn* P = GetPawn()) { P->SetActorLocation(G->PlayerLocation + FVector(0, 0, 20), false, nullptr, ETeleportType::TeleportPhysics); SetControlRotation(G->PlayerRotation); }
}

void AClassroomLevellingPlayerController::SaveGame(const FString& Slot)
{
	UNarrativeSubsystem* N = GetGameInstance()->GetSubsystem<UNarrativeSubsystem>();
	const APawn* P = GetPawn();
	const AHubWorld* Hub = AHubWorld::Get(this);
	if (N && P) N->SaveSlot(Slot, P->GetActorLocation(), GetControlRotation(), Hub ? Hub->AreaAt(P->GetActorLocation()) : NAME_None);
}

void AClassroomLevellingPlayerController::Autosave() { if (bInGame) SaveGame(TEXT("auto")); }

void AClassroomLevellingPlayerController::TogglePause()
{
	if (!bInGame) return;
	if (Menu.IsValid())
	{
		HideMenu();
		SetPause(false);
		Story->bBlocked = false;
		if (AHubWorld* Hub = AHubWorld::Get(this)) Hub->SetGrayscale(true, 2.f);
		if (Hud.IsValid()) Hud->SetVisibility(EVisibility::HitTestInvisible);
		if (Story->IsBusy()) Story->SetHidden(false);
		else { SetShowMouseCursor(false); ResetIgnoreInputFlags(); SetInputMode(FInputModeGameOnly()); }
	}
	else ShowMenu(true);
}

void AClassroomLevellingPlayerController::ApplyPrefs()
{
	const FGamePrefs& P = FGamePrefs::Get();
	if (GEngine)
	{
		GEngine->DisplayGamma = P.Brightness;
		if (UGameUserSettings* GS = GEngine->GetGameUserSettings()) { GS->SetFrameRateLimit(P.FrameLimit); GS->ApplyNonResolutionSettings(); }
		if (FAudioDevice* AD = GEngine->GetMainAudioDeviceRaw()) AD->SetTransientPrimaryVolume(1.f); // per-channel volumes handled in UGameAudio/voice
	}
	if (UGameAudio* Au = UGameAudio::Get(this)) Au->ApplyVolumes();
	if (APawn* Pawn = GetPawn()) if (UCameraComponent* Cam = Pawn->FindComponentByClass<UCameraComponent>()) Cam->SetFieldOfView(P.Fov);
	for (TActorIterator<APostProcessVolume> It(GetWorld()); It; ++It)
		if (It->bUnbound) { It->Settings.bOverride_MotionBlurAmount = true; It->Settings.MotionBlurAmount = P.bMotionBlur ? 0.5f : 0.f; }
	if (Hud.IsValid() && !P.bShowFps) Hud->SetFps(FString());
}

void AClassroomLevellingPlayerController::SprintDown()
{
	bSprinting = FGamePrefs::Get().bSprintToggle ? !bSprinting : true;
	if (ACharacter* C = Cast<ACharacter>(GetPawn())) C->GetCharacterMovement()->MaxWalkSpeed = bSprinting ? 800.f : 500.f;
}

void AClassroomLevellingPlayerController::SprintUp()
{
	if (FGamePrefs::Get().bSprintToggle) return;
	bSprinting = false;
	if (ACharacter* C = Cast<ACharacter>(GetPawn())) C->GetCharacterMovement()->MaxWalkSpeed = 500.f;
}

void AClassroomLevellingPlayerController::Interact()
{
	if (!bInGame || Menu.IsValid() || Story->IsBusy() || !GetPawn()) return;
	AHubWorld* Hub = AHubWorld::Get(this);
	ANPCStandIn* T = Hub ? Hub->NearestNamed(GetPawn()->GetActorLocation(), 260.f) : nullptr;
	if (T) Story->StartTalk(T->Id);
}

void AClassroomLevellingPlayerController::PlayerTick(float Dt)
{
	Super::PlayerTick(Dt);
	if (bAutoTune && IsLocalController()) AutoTuneTick();
	if (!bInGame || !IsLocalController()) return;
	const FGamePrefs& Prefs = FGamePrefs::Get();
	if (!IsPaused()) if (UNarrativeSubsystem* N = GetGameInstance()->GetSubsystem<UNarrativeSubsystem>()) N->PlayTime += Dt;

	// FPS readout (averaged over 0.5 s).
	FpsAcc += FApp::GetDeltaTime(); ++FpsFrames;
	if (FpsAcc >= 0.5f && Hud.IsValid()) { if (Prefs.bShowFps) Hud->SetFps(FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(FpsFrames / FpsAcc))); FpsAcc = 0.f; FpsFrames = 0; }

	ACharacter* C = Cast<ACharacter>(GetPawn());
	if (!C || Story->IsBusy()) { if (Hud.IsValid()) Hud->SetPrompt(FString()); return; }

	// Footsteps by distance travelled on the ground.
	if (C->GetCharacterMovement()->IsMovingOnGround())
	{
		StepDist += C->GetVelocity().Size2D() * Dt;
		if (StepDist > (bSprinting ? 190.f : 150.f))
		{
			StepDist = 0.f;
			if (UGameAudio* Au = UGameAudio::Get(this)) Au->PlaySfx(this, FName(*FString::Printf(TEXT("footstep_%d"), FMath::RandRange(1, 4))), 0.45f);
		}
	}

	AreaT += Dt;
	if (AreaT < 0.25f) return;
	AreaT = 0.f;
	AHubWorld* Hub = AHubWorld::Get(this);
	if (!Hub) return;
	const FName Area = Hub->AreaAt(C->GetActorLocation());
	if (Area != LastArea && !Area.IsNone() && Hud.IsValid())
	{
		const FString* Sub = AreaSubtitles().Find(Area);
		Hud->ShowArea(Area.ToString(), Sub ? *Sub : FString());
	}
	if (!Area.IsNone()) LastArea = Area;
	const ANPCStandIn* T = Hub->NearestNamed(C->GetActorLocation(), 260.f);
	const bool bPad = FSlateApplication::IsInitialized() && FSlateApplication::Get().IsGamepadAttached();
	if (Hud.IsValid()) Hud->SetPrompt(T ? FString::Printf(TEXT("[%s]  Talk"), bPad ? TEXT("X") : TEXT("E")) : FString());
}

void AClassroomLevellingPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	HideMenu();
	Super::EndPlay(Reason);
}

// Measures real FPS for a few seconds; steps the preset down until it holds ~50 FPS (or reaches Low).
void AClassroomLevellingPlayerController::AutoTuneTick()
{
	TuneT += FApp::GetDeltaTime(); ++TuneFrames;
	if (TuneT < 2.f) { TuneFrames = 0; return; }        // let the scene settle / shaders stream
	if (TuneT < 6.f) return;
	const float Fps = TuneFrames / (TuneT - 2.f);
	TuneT = 0.f; TuneFrames = 0;
	UGameUserSettings* GS = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const int32 L = GS ? GS->GetOverallScalabilityLevel() : 0;
	UE_LOG(LogTemp, Log, TEXT("AutoTune: %.0f fps at level %d"), Fps, L);
	if (GS && Fps < 50.f && L > 0) { GS->SetOverallScalabilityLevel(L - 1); if (L - 1 == 0) GS->SetResolutionScaleNormalized(0.75f); GS->ApplySettings(false); return; }
	if (GS) GS->SaveSettings();
	bAutoTune = false;
	FGamePrefs::Get().bBenchmarked = true; FGamePrefs::Get().Save();
}
