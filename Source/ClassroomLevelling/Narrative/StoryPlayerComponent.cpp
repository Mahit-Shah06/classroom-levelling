#include "StoryPlayerComponent.h"
#include "NarrativeSubsystem.h"
#include "VoiceSubsystem.h"
#include "UI/GamePrefs.h"
#include "World/HubWorld.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Styling/CoreStyle.h"

DEFINE_LOG_CATEGORY_STATIC(LogStory, Log, All);

// ---------------------------------------------------------------------------
// Dialogue box: speaker + lines on the left, choices on the right. Letterbox bars during scenes.
// Prompts follow the active device: 1/2/3/4 on keyboard+mouse, A/B/X/Y once a gamepad is used.
// ---------------------------------------------------------------------------
class SStoryWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SStoryWindow) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UStoryPlayerComponent>, Owner)
	SLATE_END_ARGS()

	enum class EStyle : uint8 { Dialogue, Narrator, System };

	static FSlateFontInfo Font(const char* Style, int32 Size) { return FCoreStyle::GetDefaultFontStyle(Style, Size); }
	static TSharedRef<SWidget> Bar(TSharedPtr<SBox>& Out) { return SAssignNew(Out, SBox).HeightOverride(0.f)[ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor::Black) ]; }

	void Construct(const FArguments& Args)
	{
		Owner = Args._Owner;
		bPad = FSlateApplication::IsInitialized() && FSlateApplication::Get().IsGamepadAttached();
		const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot().VAlign(VAlign_Top)[ Bar(TopBar) ]
			+ SOverlay::Slot().VAlign(VAlign_Bottom)[ Bar(BottomBar) ]
			// EVAL app notification (top-right)
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0, 90, 40, 0)
			[
				SAssignNew(ToastBox, SBorder).BorderImage(White).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.8f)).Padding(FMargin(16, 10)).Visibility(EVisibility::Collapsed)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Font(Font("Bold", 11)).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f)).Text(FText::FromString(TEXT("EVAL  //  STUDENT EVALUATION APP"))) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)[ SAssignNew(Toast, STextBlock).Font(Font("Regular", 13)).ColorAndOpacity(FLinearColor::White).WrapTextAt(380.f) ]
				]
			]
			// Dialogue box
			+ SOverlay::Slot().VAlign(VAlign_Bottom).HAlign(HAlign_Center).Padding(0, 0, 0, 34)
			[
				SNew(SBox).WidthOverride(1500.f).MinDesiredHeight(210.f)
				[
					SNew(SBorder).BorderImage(White).BorderBackgroundColor(FLinearColor(0.015f, 0.015f, 0.015f, 0.9f)).Padding(0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(34, 20, 30, 22))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
							[ SAssignNew(Name, STextBlock).Font(Font("Bold", 17)).ColorAndOpacity(FLinearColor::White) ]
							+ SVerticalBox::Slot().AutoHeight()
							[ SAssignNew(Body, STextBlock).Font(Font("Regular", 20)).AutoWrapText(true).LineHeightPercentage(1.15f).ColorAndOpacity(FLinearColor(0.93f, 0.93f, 0.93f)) ]
						]
						+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(1.f)[ SNew(SBorder).BorderImage(White).BorderBackgroundColor(FLinearColor(0.3f, 0.3f, 0.3f)) ] ]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).WidthOverride(470.f).Padding(FMargin(20, 18))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[ SAssignNew(Choices, SVerticalBox) ]
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
								[ SAssignNew(Hesitation, SProgressBar).Percent(0.f).Visibility(EVisibility::Hidden).FillColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f)) ]
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
								[ SAssignNew(HesText, STextBlock).Font(Font("Bold", 13)).ColorAndOpacity(FLinearColor::White).Text(FText::FromString(TEXT("HESITATION DETECTED"))).Visibility(EVisibility::Hidden) ]
							]
						]
					]
				]
			]
		];
	}

	void SetLetterbox(bool bOn) { LetterTarget = bOn ? 70.f : 0.f; }

	void ShowLine(const FString& Speaker, const FString& Text, EStyle Style)
	{
		Name->SetText(FText::FromString(Style == EStyle::System ? TEXT("EVAL // Notification") : Speaker));
		Name->SetVisibility(Speaker.IsEmpty() && Style != EStyle::System ? EVisibility::Collapsed : EVisibility::Visible);
		Body->SetFont(Font(Style == EStyle::Narrator ? "Italic" : "Regular", 20));
		Body->SetColorAndOpacity(Style == EStyle::Narrator ? FLinearColor(0.72f, 0.72f, 0.72f) : FLinearColor(0.93f, 0.93f, 0.93f));
		FullText = Text; Shown = 0; TypeAcc = 0.f;
		Body->SetText(FText::GetEmpty());
		SetHesitation(0.f, false, false);
	}

	void SetChoices(const TArray<FString>& In) { Labels = In; bContinue = false; Rebuild(); }
	void SetContinue(const FString& Hint) { Labels.Reset(); bContinue = true; ContinueHint = Hint; Rebuild(); }
	bool IsTyping() const { return Shown < FullText.Len(); }
	void SkipTyping() { Shown = FullText.Len(); Body->SetText(FText::FromString(FullText)); }
	int32 WordCount() const { TArray<FString> W; FullText.ParseIntoArrayWS(W); return W.Num(); }

	void SetHesitation(float P, bool bShow, bool bLate)
	{
		Hesitation->SetVisibility(bShow ? EVisibility::Visible : EVisibility::Hidden);
		Hesitation->SetPercent(P);
		Hesitation->SetFillColorAndOpacity(bLate ? FLinearColor::White : FLinearColor(0.45f, 0.45f, 0.45f));
		HesText->SetVisibility(bLate ? EVisibility::Visible : EVisibility::Hidden);
	}

	void ShowToast(const FString& S) { Toast->SetText(FText::FromString(S)); ToastBox->SetVisibility(EVisibility::HitTestInvisible); ToastT = 7.f; }

	virtual void Tick(const FGeometry& G, const double Time, const float Dt) override
	{
		if (Shown < FullText.Len())
		{
			TypeAcc += Dt * FGamePrefs::Get().TextSpeed;
			const int32 N = FMath::Min(FullText.Len(), Shown + (int32)TypeAcc);
			if (N != Shown) { TypeAcc -= N - Shown; Shown = N; Body->SetText(FText::FromString(FullText.Left(Shown))); }
		}
		Letter = FMath::FInterpTo(Letter, LetterTarget, Dt, 6.f);
		TopBar->SetHeightOverride(Letter); BottomBar->SetHeightOverride(Letter);
		if (ToastT > 0.f) { ToastT -= Dt; ToastBox->SetRenderOpacity(FMath::Clamp(ToastT, 0.f, 1.f)); if (ToastT <= 0.f) ToastBox->SetVisibility(EVisibility::Collapsed); }
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnMouseMove(const FGeometry& G, const FPointerEvent& E) override { SetPad(false); return FReply::Unhandled(); }
	virtual FReply OnAnalogValueChanged(const FGeometry&, const FAnalogInputEvent& E) override { if (FMath::Abs(E.GetAnalogValue()) > 0.5f) SetPad(true); return FReply::Unhandled(); }

	virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent& E) override
	{
		if (!Owner.IsValid()) return FReply::Unhandled();
		const FKey K = E.GetKey();
		SetPad(K.IsGamepadKey());
		if (K == EKeys::Escape || K == EKeys::Gamepad_Special_Right) { if (Owner->OnPauseRequested) Owner->OnPauseRequested(); return FReply::Handled(); }

		const bool bChoosing = Owner->IsAwaitingChoice() && !IsTyping();
		static const FKey Nums[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
		static const FKey Pad[] = { EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Left, EKeys::Gamepad_FaceButton_Top };
		for (int32 i = 0; i < 5; ++i) if (K == Nums[i] && bChoosing) { Owner->Choose(i); return FReply::Handled(); }
		for (int32 i = 0; i < 4; ++i) if (K == Pad[i] && bChoosing) { Owner->Choose(i); return FReply::Handled(); }
		if (K == EKeys::SpaceBar || K == EKeys::Enter || K == EKeys::Gamepad_FaceButton_Bottom || K == EKeys::LeftMouseButton)
		{
			if (IsTyping()) SkipTyping(); else Owner->Continue();
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}
	virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override
	{
		SetPad(false);
		if (!Owner.IsValid() || Owner->IsAwaitingChoice()) return FReply::Unhandled();
		if (IsTyping()) SkipTyping(); else Owner->Continue();
		return FReply::Handled();
	}

private:
	void SetPad(bool b) { if (b != bPad) { bPad = b; Rebuild(); } }
	FString Key(int32 i) const { static const TCHAR* P[] = { TEXT("A"), TEXT("B"), TEXT("X"), TEXT("Y"), TEXT("-") }; return bPad ? P[FMath::Min(i, 4)] : FString::FromInt(i + 1); }

	void Rebuild()
	{
		Choices->ClearChildren();
		const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
		auto Badge = [&](const FString& K) -> TSharedRef<SWidget>
		{
			return SNew(SBox).WidthOverride(K.Len() > 1 ? 64.f : 30.f).HeightOverride(30.f)
				[ SNew(SBorder).BorderImage(White).BorderBackgroundColor(FLinearColor(0.9f, 0.9f, 0.9f)).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0)
					[ SNew(STextBlock).Font(Font("Bold", 13)).ColorAndOpacity(FLinearColor::Black).Text(FText::FromString(K)) ] ];
		};
		if (bContinue)
		{
			Choices->AddSlot().AutoHeight().Padding(0, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Badge(bPad ? TEXT("A") : TEXT("Space")) ]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(12, 0, 0, 0)[ SNew(STextBlock).Font(Font("Regular", 15)).ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f)).Text(FText::FromString(ContinueHint)) ]
			];
			return;
		}
		for (int32 i = 0; i < Labels.Num(); ++i)
		{
			Choices->AddSlot().AutoHeight().Padding(0, 3)
			[
				SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.07f, 0.07f, 0.07f, 1.f)).ContentPadding(FMargin(8, 6)).IsFocusable(false)
				.OnClicked_Lambda([this, i]() { if (Owner.IsValid() && !IsTyping()) Owner->Choose(i); return FReply::Handled(); })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Badge(Key(i)) ]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(12, 0, 0, 0)
					[ SNew(STextBlock).Font(Font("Regular", 15)).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.93f, 0.93f, 0.93f)).Text(FText::FromString(Labels[i])) ]
				]
			];
		}
	}

	TWeakObjectPtr<UStoryPlayerComponent> Owner;
	TSharedPtr<STextBlock> Name, Body, HesText, Toast;
	TSharedPtr<SVerticalBox> Choices;
	TSharedPtr<SProgressBar> Hesitation;
	TSharedPtr<SBorder> ToastBox;
	TSharedPtr<SBox> TopBar, BottomBar;
	TArray<FString> Labels;
	FString FullText, ContinueHint;
	int32 Shown = 0;
	float TypeAcc = 0.f, Letter = 0.f, LetterTarget = 0.f, ToastT = 0.f;
	bool bPad = false, bContinue = false;
};

// ---------------------------------------------------------------------------
UStoryPlayerComponent::UStoryPlayerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only ticks while a scene is open
}

UNarrativeSubsystem* UStoryPlayerComponent::Narrative() const
{
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNarrativeSubsystem>() : nullptr;
}

void UStoryPlayerComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Close();
	Super::EndPlay(Reason);
}

void UStoryPlayerComponent::SetUIMode(bool bUI)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC) return;
	PC->SetShowMouseCursor(bUI);
	PC->SetIgnoreMoveInput(bUI);
	PC->SetIgnoreLookInput(bUI);
	if (bUI && Window.IsValid()) { FInputModeUIOnly M; M.SetWidgetToFocus(Window); PC->SetInputMode(M); }
	else PC->SetInputMode(FInputModeGameOnly());
}

void UStoryPlayerComponent::SetHidden(bool bHide)
{
	if (!Window.IsValid()) return;
	Window->SetVisibility(bHide ? EVisibility::Collapsed : EVisibility::Visible);
	if (!bHide) SetUIMode(true);
}

void UStoryPlayerComponent::StartNext(FName Kind)
{
	FStorylet S;
	if (UNarrativeSubsystem* N = Narrative()) if (N->PickStorylet(Kind, NAME_None, S)) Begin(S);
}

void UStoryPlayerComponent::StartAt(FName Location)
{
	FStorylet S;
	if (UNarrativeSubsystem* N = Narrative()) if (N->PickStorylet(NAME_None, Location, S)) Begin(S);
}

int32 UStoryPlayerComponent::VoiceId(FName NPC) const
{
	const UNarrativeSubsystem* N = Narrative();
	const FNPCProfile* P = N ? N->NPCs.Find(NPC) : nullptr;
	if (P && !P->Voice.IsEmpty()) return FCString::Atoi(*P->Voice) % 904;
	return 92; // narrator
}

bool UStoryPlayerComponent::Begin(const FStorylet& S)
{
	UNarrativeSubsystem* N = Narrative();
	if (!N || !GEngine || !GEngine->GameViewport) return false;
	Current = S;
	UE_LOG(LogStory, Log, TEXT("Begin %s (%d lines) at %s"), *S.Id.ToString(), S.Lines.Num(), *S.Location.ToString());
	if (!Window.IsValid())
	{
		SAssignNew(Window, SStoryWindow).Owner(this);
		GEngine->GameViewport->AddViewportWidgetContent(Window.ToSharedRef(), 10);
	}

	// Resolve speakers per step; queue all voice lines now (synthesis runs while the player reads).
	UVoiceSubsystem* V = GetWorld()->GetGameInstance()->GetSubsystem<UVoiceSubsystem>();
	StepSpeakers.Reset(); VoicePaths.Reset();
	TArray<FName> Cast_;
	auto AddStep = [&](FName Speaker, const FText& Text)
	{
		const FName Npc = N->SpeakerNPC(Speaker);
		StepSpeakers.Add(Npc);
		if (!Npc.IsNone()) Cast_.AddUnique(Npc);
		const bool bVoiced = V && Speaker != TEXT("system");
		VoicePaths.Add(bVoiced ? V->Request(N->ResolveText(Text).ToString(), VoiceId(Npc)) : FString());
	};
	for (const FDialogueLine& L : Current.Lines) AddStep(L.Speaker, L.Text);
	AddStep(Current.PromptSpeaker, Current.Text);

	if (AHubWorld* Hub = AHubWorld::Get(this))
	{
		APlayerController* PC = Cast<APlayerController>(GetOwner());
		Hub->Stage(Current.Location, Cast_, PC ? PC->GetPawn() : nullptr);
	}
	Window->SetLetterbox(true);
	Step = 0;
	bResolved = false;
	ShowStep();
	SetComponentTickEnabled(true);
	SetUIMode(true);
	return true;
}

void UStoryPlayerComponent::ShowStep()
{
	UNarrativeSubsystem* N = Narrative();
	if (!N || !Window.IsValid()) return;
	StopVoice();
	UE_LOG(LogStory, Log, TEXT("Step %d/%d"), Step, Current.Lines.Num());
	const bool bPrompt = Step >= Current.Lines.Num();
	const FName Speaker = bPrompt ? Current.PromptSpeaker : Current.Lines[Step].Speaker;
	const FText Text = bPrompt ? Current.Text : Current.Lines[Step].Text;
	const SStoryWindow::EStyle Style = Speaker == TEXT("system") ? SStoryWindow::EStyle::System :
		N->SpeakerName(Speaker).IsEmpty() ? SStoryWindow::EStyle::Narrator : SStoryWindow::EStyle::Dialogue;
	Window->ShowLine(N->SpeakerName(Speaker), N->ResolveText(Text).ToString(), Style);

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (AHubWorld* Hub = AHubWorld::Get(this))
		Hub->Shot(bPrompt ? FName("over_shoulder") : Current.Lines[Step].Shot, StepSpeakers.IsValidIndex(Step) ? StepSpeakers[Step] : NAME_None, PC ? PC->GetPawn() : nullptr);
	bVoicePending = VoicePaths.IsValidIndex(Step) && !VoicePaths[Step].IsEmpty();
	VoiceWait = 0.f;

	if (!bPrompt) { bAwaitingChoice = false; Window->SetContinue(TEXT("Continue")); return; }

	VisibleChoices.Reset();
	TArray<FString> Labels;
	for (int32 i = 0; i < Current.Choices.Num(); ++i)
	{
		if (!N->CheckAll(Current.Choices[i].Requires)) continue;
		VisibleChoices.Add(i);
		Labels.Add(N->ResolveText(Current.Choices[i].Text).ToString());
	}
	bAwaitingChoice = Labels.Num() > 0;
	if (bAwaitingChoice) Window->SetChoices(Labels); else Window->SetContinue(TEXT("Continue"));

	CachedWitnesses = Witnesses();
	CachedPatience = 10.f;
	for (const FName& W : CachedWitnesses) if (const FNPCProfile* P = N->NPCs.Find(W)) CachedPatience = FMath::Min(CachedPatience, P->Patience);
	bWarnedHesitation = false;
	ChoiceTime = 0.f;
	Grace = -1.f; // set once typing finishes
}

TArray<FName> UStoryPlayerComponent::Witnesses() const
{
	// Everyone on stage or referenced in the scene witnesses the choice.
	TArray<FName> Out;
	const UNarrativeSubsystem* N = Narrative();
	if (!N) return Out;
	for (const FName& S : StepSpeakers) if (!S.IsNone()) Out.AddUnique(S);
	const FString Txt = Current.Text.ToString();
	auto Consider = [&](FName Ref) { const FName Id = N->Resolve(FName(*(TEXT("@") + Ref.ToString()))); if (!Id.IsNone()) Out.AddUnique(Id); };
	for (const auto& KV : N->RoleBindings) if (Txt.Contains(FString::Printf(TEXT("{%s}"), *KV.Key.ToString()))) Consider(KV.Key);
	for (const auto& KV : N->SlotBindings) Consider(KV.Key);
	return Out;
}

void UStoryPlayerComponent::Choose(int32 Index)
{
	UNarrativeSubsystem* N = Narrative();
	if (!bAwaitingChoice || !N || !VisibleChoices.IsValidIndex(Index) || !Window.IsValid()) return;
	bAwaitingChoice = false;
	bResolved = true;

	const FNarrativeChoice& C = Current.Choices[VisibleChoices[Index]];
	const float Hes = FGamePrefs::Get().bHesitation ? ChoiceTime : 0.f;
	const TArray<FNPCReaction> Reactions = N->ApplyChoice(C, CachedWitnesses, Hes);

	FString Out = FString::Printf(TEXT("Decision logged (%.1fs)."), Hes);
	float Worst = 0.f;
	for (const FNPCReaction& R : Reactions)
	{
		const FNPCProfile* P = N->NPCs.Find(R.NPC);
		if (FMath::Abs(R.Delta) < 1.f) continue;
		Out += FString::Printf(TEXT("\n%s read it as %s%s  %+.0f"), P ? *P->Name.ToString() : *R.NPC.ToString(),
			*R.PerceivedTag.ToString().ToUpper(), R.bMisread ? TEXT(" (misread)") : TEXT(""), R.Delta);
		Worst = FMath::Min(Worst, R.Delta);
	}
	Window->ShowToast(Out);
	Window->SetHesitation(0.f, false, false);
	Window->SetContinue(TEXT("Continue"));
	if (Worst <= -5.f) Shake(FMath::Clamp(-Worst / 15.f, 0.3f, 1.f));

	PendingNext = (C.Next.IsNone() || C.Next == TEXT("None")) ? NAME_None : C.Next; // choice-chained scene
}

void UStoryPlayerComponent::Continue()
{
	if (bAwaitingChoice || !Window.IsValid()) return;
	if (Step < Current.Lines.Num()) { ++Step; ShowStep(); return; }
	if (!PendingNext.IsNone())
	{
		const FName Id = PendingNext; PendingNext = NAME_None;
		if (const UNarrativeSubsystem* N = Narrative())
			for (const FStorylet& S : N->Storylets) if (S.Id == Id) { const FStorylet Copy = S; StopVoice(); Begin(Copy); return; }
	}
	Close();
	// Mandatory follow-ups (e.g. homeroom right after the ceremony) play immediately.
	if (const UNarrativeSubsystem* N = Narrative())
		if (const FStorylet* Next = N->FindStorylet(NAME_None, NAME_None)) if (Next->Priority >= 95.f) StartNext();
}

void UStoryPlayerComponent::StopVoice()
{
	if (Voice) { Voice->Stop(); Voice = nullptr; }
	if (AHubWorld* Hub = AHubWorld::Get(this)) for (const FName& S : StepSpeakers) Hub->SetTalking(S, false);
}

void UStoryPlayerComponent::Close()
{
	if (Window.IsValid()) UE_LOG(LogStory, Log, TEXT("Close %s at step %d"), *Current.Id.ToString(), Step);
	StopVoice();
	if (Window.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(Window.ToSharedRef());
	Window.Reset();
	bAwaitingChoice = false;
	SetComponentTickEnabled(false);
	if (GetWorld() && !GetWorld()->bIsTearingDown)
	{
		if (AHubWorld* Hub = AHubWorld::Get(this)) Hub->EndCutscene(Cast<APlayerController>(GetOwner()));
		SetUIMode(false);
	}
}

void UStoryPlayerComponent::Shake(float Strength)
{
	ShakeT = 0.35f; ShakeAmp = 14.f * Strength;
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		FLatentActionInfo Info;
		PC->PlayDynamicForceFeedback(Strength, 0.3f, true, true, true, true, EDynamicForceFeedbackAction::Start, Info);
	}
}

void UStoryPlayerComponent::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* Fn)
{
	Super::TickComponent(Dt, TickType, Fn);
	if (!Window.IsValid() || bBlocked) return;

	// Voice: start when synthesized (give up after 2s so text never waits on audio).
	if (bVoicePending)
	{
		VoiceWait += Dt;
		UVoiceSubsystem* V = GetWorld()->GetGameInstance()->GetSubsystem<UVoiceSubsystem>();
		if (V && V->IsReady(VoicePaths[Step]))
		{
			bVoicePending = false;
			V->bEnabled = FGamePrefs::Get().bVoice;
			Voice = V->Play(this, VoicePaths[Step]);
			if (Voice && StepSpeakers.IsValidIndex(Step))
				if (AHubWorld* Hub = AHubWorld::Get(this)) Hub->SetTalking(StepSpeakers[Step], true);
		}
		else if (VoiceWait > 2.f) bVoicePending = false;
	}
	if (Voice && !Voice->IsPlaying())
	{
		Voice = nullptr;
		if (AHubWorld* Hub = AHubWorld::Get(this)) if (StepSpeakers.IsValidIndex(Step)) Hub->SetTalking(StepSpeakers[Step], false);
	}

	// Hesitation: only after the prompt is fully on screen + time to read it.
	if (bAwaitingChoice && !Window->IsTyping() && FGamePrefs::Get().bHesitation)
	{
		if (Grace < 0.f) Grace = 1.5f + Window->WordCount() * 0.3f;
		if (Grace > 0.f) { Grace -= Dt; return; }
		ChoiceTime += Dt;
		const float Patience = FMath::Max(CachedPatience, 3.f);
		const bool bLate = ChoiceTime > Patience;
		Window->SetHesitation(FMath::Clamp(ChoiceTime / Patience, 0.f, 1.f), true, bLate);
		if (bLate && !bWarnedHesitation) { bWarnedHesitation = true; Shake(0.25f); }
	}
	if (ShakeT > 0.f)
	{
		ShakeT -= Dt;
		const float A = ShakeAmp * FMath::Max(ShakeT, 0.f) / 0.35f;
		Window->SetRenderTransform(FSlateRenderTransform(FVector2D(FMath::FRandRange(-A, A), FMath::FRandRange(-A, A))));
	}
}
