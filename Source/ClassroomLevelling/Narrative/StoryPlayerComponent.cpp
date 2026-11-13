#include "StoryPlayerComponent.h"
#include "NarrativeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"

// ---------------------------------------------------------------------------
// System window: monochrome, cold, evaluator-style. Text types out; the System
// annotates how each NPC *read* your choice (including misreads).
// ---------------------------------------------------------------------------
class SStoryWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SStoryWindow) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UStoryPlayerComponent>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args)
	{
		Owner = Args._Owner;
		Panel.TintColor = FSlateColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.88f));
		Panel.DrawAs = ESlateBrushDrawType::Box;
		Line.TintColor = FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.f));

		ChildSlot
		.VAlign(VAlign_Bottom).HAlign(HAlign_Center).Padding(FMargin(0, 0, 0, 40))
		[
			SNew(SBox).WidthOverride(1100.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[ SNew(SBox).HeightOverride(2.f)[ SNew(SBorder).BorderImage(&Line) ] ]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBorder).BorderImage(&Panel).Padding(FMargin(28, 18))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
						[ SAssignNew(Header, STextBlock).Font(Font("Bold", 13)).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f)) ]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
						[ SAssignNew(Body, STextBlock).Font(Font("Regular", 19)).AutoWrapText(true).ColorAndOpacity(FLinearColor::White) ]
						+ SVerticalBox::Slot().AutoHeight()
						[ SAssignNew(Choices, SVerticalBox) ]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
						[ SAssignNew(Hesitation, SProgressBar).Percent(0.f).FillColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f)).BarFillType(EProgressBarFillType::LeftToRight) ]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
						[ SAssignNew(Readout, STextBlock).Font(Font("Italic", 14)).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f)) ]
					]
				]
			]
		];
	}

	static const TCHAR* PadLabel(int32 i) { static const TCHAR* L[] = { TEXT("A"), TEXT("B"), TEXT("X"), TEXT("Y"), TEXT("-") }; return L[FMath::Clamp(i, 0, 4)]; }
	static FSlateFontInfo Font(const char* Style, int32 Size) { return FCoreStyle::GetDefaultFontStyle(Style, Size); }

	void Show(const FString& InHeader, const FString& InBody, const TArray<FString>& InChoices)
	{
		Header->SetText(FText::FromString(InHeader.ToUpper()));
		FullText = InBody; Shown = 0; TypeAcc = 0.f;
		Body->SetText(FText::GetEmpty());
		Readout->SetText(FText::GetEmpty());
		Hesitation->SetPercent(0.f);
		Choices->ClearChildren();
		for (int32 i = 0; i < InChoices.Num(); ++i)
		{
			Choices->AddSlot().AutoHeight().Padding(0, 3)
			[
				SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.08f, 0.08f, 0.08f, 1.f))
				.OnClicked_Lambda([this, i]() { if (Owner.IsValid()) Owner->Choose(i); return FReply::Handled(); })
				[ SNew(STextBlock).Font(Font("Regular", 16)).ColorAndOpacity(FLinearColor(0.92f, 0.92f, 0.92f))
					.Text(FText::FromString(FString::Printf(TEXT("[%d / %s]  %s"), i + 1, PadLabel(i), *InChoices[i]))) ]
			];
		}
	}

	void SetReadout(const FString& S) { Readout->SetText(FText::FromString(S)); }
	void SetHesitation(float P, bool bLate) { Hesitation->SetPercent(P); Hesitation->SetFillColorAndOpacity(bLate ? FLinearColor(1.f, 1.f, 1.f) : FLinearColor(0.45f, 0.45f, 0.45f)); }
	void ClearChoices(const FString& Hint) { Choices->ClearChildren(); Choices->AddSlot().AutoHeight()[ SNew(STextBlock).Font(Font("Regular", 13)).ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f)).Text(FText::FromString(Hint)) ]; }
	void SkipTyping() { Shown = FullText.Len(); Body->SetText(FText::FromString(FullText)); }

	virtual void Tick(const FGeometry& G, const double Time, const float Dt) override
	{
		if (Shown < FullText.Len())
		{
			TypeAcc += Dt * 60.f; // chars/sec
			const int32 N = FMath::Min(FullText.Len(), Shown + (int32)TypeAcc);
			if (N != Shown) { TypeAcc -= N - Shown; Shown = N; Body->SetText(FText::FromString(FullText.Left(Shown))); }
		}
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent& E) override
	{
		if (!Owner.IsValid()) return FReply::Unhandled();
		const FKey K = E.GetKey();
		static const FKey Nums[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
		for (int32 i = 0; i < 5; ++i) if (K == Nums[i]) { Owner->Choose(i); return FReply::Handled(); }
		// Gamepad: A/B/X/Y = choices 1-4 (A also continues), Start = close.
		static const FKey Pad[] = { EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Left, EKeys::Gamepad_FaceButton_Top };
		for (int32 i = 0; i < 4; ++i) if (K == Pad[i] && Shown >= FullText.Len() && Owner->IsAwaitingChoice()) { Owner->Choose(i); return FReply::Handled(); }
		if (K == EKeys::Gamepad_Special_Right) { Owner->Close(); return FReply::Handled(); }
		if (K == EKeys::SpaceBar || K == EKeys::Enter || K == EKeys::Gamepad_FaceButton_Bottom)
		{
			if (Shown < FullText.Len()) SkipTyping(); else Owner->Continue();
			return FReply::Handled();
		}
		if (K == EKeys::Escape) { Owner->Close(); return FReply::Handled(); }
		return FReply::Unhandled();
	}

private:
	TWeakObjectPtr<UStoryPlayerComponent> Owner;
	TSharedPtr<STextBlock> Header, Body, Readout;
	TSharedPtr<SVerticalBox> Choices;
	TSharedPtr<SProgressBar> Hesitation;
	FSlateBrush Panel, Line;
	FString FullText;
	int32 Shown = 0;
	float TypeAcc = 0.f;
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

void UStoryPlayerComponent::BeginPlay()
{
	Super::BeginPlay();
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (bAutoStart && PC && PC->IsLocalController()) StartNext();
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

void UStoryPlayerComponent::StartNext(FName Kind)
{
	UNarrativeSubsystem* N = Narrative();
	if (!N) return;

	if (!Window.IsValid() && GEngine && GEngine->GameViewport)
	{
		SAssignNew(Window, SStoryWindow).Owner(this);
		GEngine->GameViewport->AddViewportWidgetContent(Window.ToSharedRef(), 10);
	}
	if (!Window.IsValid()) return;

	if (!N->PickStorylet(Kind, Current))
	{
		Window->Show(TEXT("System"), TEXT("No further directives. Go live your life. The System is watching anyway."), {});
		Window->ClearChoices(TEXT("[Space] close"));
		bAwaitingChoice = false;
		SetUIMode(true);
		return;
	}

	VisibleChoices.Reset();
	TArray<FString> Labels;
	for (int32 i = 0; i < Current.Choices.Num(); ++i)
	{
		if (!N->CheckAll(Current.Choices[i].Requires)) continue;
		VisibleChoices.Add(i);
		Labels.Add(N->ResolveText(Current.Choices[i].Text).ToString());
	}
	const FString Header = Current.Kind == TEXT("exam") ? TEXT("System // Special Exam") :
		Current.Kind == TEXT("system") ? TEXT("System") : TEXT("Scene");
	Window->Show(Header, N->ResolveText(Current.Text).ToString(), Labels);
	if (Labels.Num() == 0) Window->ClearChoices(TEXT("[Space] continue"));

	bAwaitingChoice = Labels.Num() > 0;
	CachedWitnesses = Witnesses();
	CachedPatience = 10.f;
	for (const FName& W : CachedWitnesses) if (const FNPCProfile* P = N->NPCs.Find(W)) CachedPatience = FMath::Min(CachedPatience, P->Patience);
	bWarnedHesitation = false;
	ChoiceTime = 0.f;
	SetComponentTickEnabled(true);
	SetUIMode(true);
}

TArray<FName> UStoryPlayerComponent::Witnesses() const
{
	// Anyone referenced in the scene text ({role}/{slot}) or slots witnesses the choice.
	TArray<FName> Out;
	const UNarrativeSubsystem* N = Narrative();
	if (!N) return Out;
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

	const FNarrativeChoice& C = Current.Choices[VisibleChoices[Index]];
	const TArray<FNPCReaction> Reactions = N->ApplyChoice(C, CachedWitnesses, ChoiceTime);

	FString Out = FString::Printf(TEXT("[SYSTEM] Decision logged in %.1fs."), ChoiceTime);
	float Worst = 0.f;
	for (const FNPCReaction& R : Reactions)
	{
		const FNPCProfile* P = N->NPCs.Find(R.NPC);
		const FString Name = P ? P->Name.ToString() : R.NPC.ToString();
		Out += FString::Printf(TEXT("\n%s read this as %s%s.  %+.0f"), *Name,
			*R.PerceivedTag.ToString().ToUpper(), R.bMisread ? TEXT(" (misread)") : TEXT(""), R.Delta);
		Worst = FMath::Min(Worst, R.Delta);
	}
	Window->SetReadout(Out);
	Window->ClearChoices(TEXT("[Space] continue"));
	if (Worst <= -5.f) Shake(FMath::Clamp(-Worst / 15.f, 0.3f, 1.f));
}

void UStoryPlayerComponent::Continue()
{
	if (bAwaitingChoice) return;
	if (Current.Id.IsNone()) { Close(); return; }
	FName NextKind = NAME_None;
	Current = FStorylet();
	StartNext(NextKind);
}

void UStoryPlayerComponent::Close()
{
	if (Window.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(Window.ToSharedRef());
	Window.Reset();
	bAwaitingChoice = false;
	SetComponentTickEnabled(false);
	if (GetWorld() && !GetWorld()->bIsTearingDown) SetUIMode(false);
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
	if (bAwaitingChoice && Window.IsValid())
	{
		ChoiceTime += Dt;
		const float Patience = FMath::Max(CachedPatience, 1.f);
		const bool bLate = ChoiceTime > Patience;
		Window->SetHesitation(FMath::Clamp(ChoiceTime / Patience, 0.f, 1.f), bLate);
		if (bLate && !bWarnedHesitation)
		{
			bWarnedHesitation = true;
			Window->SetReadout(TEXT("[SYSTEM] Hesitation detected. They are getting bored of you."));
			Shake(0.25f);
		}
	}
	if (ShakeT > 0.f && Window.IsValid())
	{
		ShakeT -= Dt;
		const float A = ShakeAmp * FMath::Max(ShakeT, 0.f) / 0.35f;
		Window->SetRenderTransform(FSlateRenderTransform(FVector2D(FMath::FRandRange(-A, A), FMath::FRandRange(-A, A))));
	}
}
