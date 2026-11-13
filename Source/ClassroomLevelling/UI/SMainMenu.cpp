#include "SMainMenu.h"
#include "GamePrefs.h"
#include "Narrative/VoiceSubsystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "AudioDevice.h"

static FSlateFontInfo MenuFont(const char* S, int32 Sz) { return FCoreStyle::GetDefaultFontStyle(S, Sz); }

void SMainMenu::Construct(const FArguments& Args)
{
	bPause = Args._bPause; OnPlay = Args._OnPlay; OnTitle = Args._OnTitle; OnQuit = Args._OnQuit;
	ChildSlot
	[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0, 0, 0, bPause ? 0.75f : 0.35f))
		.Padding(FMargin(110, 0, 0, 0)).VAlign(VAlign_Center).HAlign(HAlign_Left)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[ SNew(STextBlock).Text(FText::FromString(TEXT("CLASSROOM"))).Font(MenuFont("Bold", 64)).ColorAndOpacity(FLinearColor::White) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, -18, 0, 0)
			[ SNew(STextBlock).Text(FText::FromString(TEXT("LEVELLING"))).Font(MenuFont("Light", 64)).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f)) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 40)
			[ SNew(STextBlock).Text(FText::FromString(TEXT("99% placement rate. Your evaluation is always on."))).Font(MenuFont("Italic", 15)).ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f)) ]
			+ SVerticalBox::Slot().AutoHeight()
			[ SNew(SBox).WidthOverride(460.f)[ SAssignNew(List, SVerticalBox) ] ]
		]
	];
	ShowMain();
}

TSharedRef<SWidget> SMainMenu::Item(const TFunction<FString()>& Label, const TFunction<void()>& OnClick)
{
	TSharedPtr<STextBlock> T;
	TSharedRef<SButton> B = SNew(SButton)
		.ButtonColorAndOpacity(FLinearColor(0.05f, 0.05f, 0.05f, 0.85f))
		.ContentPadding(FMargin(18, 10))
		.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
		[ SAssignNew(T, STextBlock).Font(MenuFont("Regular", 19)).ColorAndOpacity(FLinearColor::White).Text_Lambda([Label]() { return FText::FromString(Label()); }) ];
	if (!First.IsValid()) First = B;
	return SNew(SBox).Padding(FMargin(0, 4))[ B ];
}

void SMainMenu::FocusFirst()
{
	if (First.IsValid()) FSlateApplication::Get().SetAllUserFocus(First, EFocusCause::SetDirectly);
}

void SMainMenu::ShowMain()
{
	bInSettings = false; First.Reset(); List->ClearChildren();
	List->AddSlot().AutoHeight()[ Item([this]() { return FString(bPause ? TEXT("Resume") : TEXT("New Game")); }, [this]() { OnPlay.ExecuteIfBound(); }) ];
	List->AddSlot().AutoHeight()[ Item([]() { return FString(TEXT("Settings")); }, [this]() { ShowSettings(); }) ];
	if (bPause) List->AddSlot().AutoHeight()[ Item([]() { return FString(TEXT("Quit to Title")); }, [this]() { OnTitle.ExecuteIfBound(); }) ];
	List->AddSlot().AutoHeight()[ Item([]() { return FString(TEXT("Quit Game")); }, [this]() { OnQuit.ExecuteIfBound(); }) ];
	FocusFirst();
}

void SMainMenu::ShowSettings()
{
	bInSettings = true; First.Reset(); List->ClearChildren();
	UGameUserSettings* GS = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	FGamePrefs& P = FGamePrefs::Get();
	auto Apply = [GS]() { if (GS) { GS->ApplySettings(false); GS->SaveSettings(); } FGamePrefs::Get().Save(); };

	static const TCHAR* Q[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic") };
	List->AddSlot().AutoHeight()[ Item(
		[GS]() { return FString::Printf(TEXT("Graphics:  %s"), Q[FMath::Clamp(GS ? GS->GetOverallScalabilityLevel() : 1, 0, 3)]); },
		[GS, Apply]() { if (GS) { GS->SetOverallScalabilityLevel((FMath::Max(GS->GetOverallScalabilityLevel(), 0) + 1) % 4); Apply(); } }) ];

	static const float Scales[] = { 0.5f, 0.67f, 0.75f, 0.85f, 1.f };
	List->AddSlot().AutoHeight()[ Item(
		[GS]() { return FString::Printf(TEXT("Render scale:  %d%%"), FMath::RoundToInt((GS ? GS->GetResolutionScaleNormalized() : 1.f) * 100.f)); },
		[GS, Apply]()
		{
			if (!GS) return;
			const float Cur = GS->GetResolutionScaleNormalized(); int32 Idx = 0;
			for (int32 i = 0; i < 5; ++i) if (Cur >= Scales[i] - 0.01f) Idx = i;
			GS->SetResolutionScaleNormalized(Scales[(Idx + 1) % 5]); Apply();
		}) ];

	static const TCHAR* WM[] = { TEXT("Fullscreen"), TEXT("Borderless"), TEXT("Windowed") };
	List->AddSlot().AutoHeight()[ Item(
		[GS]() { return FString::Printf(TEXT("Display:  %s"), WM[GS ? (int32)GS->GetFullscreenMode() : 2]); },
		[GS, Apply]() { if (GS) { GS->SetFullscreenMode(EWindowMode::ConvertIntToWindowMode(((int32)GS->GetFullscreenMode() + 1) % 3)); Apply(); } }) ];

	List->AddSlot().AutoHeight()[ Item(
		[GS]() { return FString::Printf(TEXT("VSync:  %s"), GS && GS->IsVSyncEnabled() ? TEXT("On") : TEXT("Off")); },
		[GS, Apply]() { if (GS) { GS->SetVSyncEnabled(!GS->IsVSyncEnabled()); Apply(); } }) ];

	List->AddSlot().AutoHeight()[ Item(
		[&P]() { return FString::Printf(TEXT("Volume:  %d%%"), FMath::RoundToInt(P.Volume * 100.f)); },
		[&P, Apply]()
		{
			P.Volume = P.Volume >= 0.99f ? 0.f : FMath::Min(P.Volume + 0.1f, 1.f);
			if (FAudioDevice* AD = GEngine ? GEngine->GetMainAudioDeviceRaw() : nullptr) AD->SetTransientPrimaryVolume(P.Volume);
			Apply();
		}) ];

	List->AddSlot().AutoHeight()[ Item(
		[&P]() { return FString::Printf(TEXT("Voices:  %s"), P.bVoice ? TEXT("On") : TEXT("Off")); },
		[&P, Apply]() { P.bVoice = !P.bVoice; Apply(); }) ];

	static const float Speeds[] = { 25.f, 40.f, 70.f, 10000.f };
	static const TCHAR* SpeedN[] = { TEXT("Slow"), TEXT("Normal"), TEXT("Fast"), TEXT("Instant") };
	List->AddSlot().AutoHeight()[ Item(
		[&P]() { int32 i = 0; for (int32 k = 0; k < 4; ++k) if (P.TextSpeed >= Speeds[k]) i = k; return FString::Printf(TEXT("Text speed:  %s"), SpeedN[i]); },
		[&P, Apply]() { int32 i = 0; for (int32 k = 0; k < 4; ++k) if (P.TextSpeed >= Speeds[k]) i = k; P.TextSpeed = Speeds[(i + 1) % 4]; Apply(); }) ];

	List->AddSlot().AutoHeight()[ Item(
		[&P]() { return FString::Printf(TEXT("Hesitation timer:  %s"), P.bHesitation ? TEXT("On") : TEXT("Off (relaxed)")); },
		[&P, Apply]() { P.bHesitation = !P.bHesitation; Apply(); }) ];

	List->AddSlot().AutoHeight()[ Item([]() { return FString(TEXT("Back")); }, [this]() { ShowMain(); }) ];
	FocusFirst();
}

FReply SMainMenu::OnKeyDown(const FGeometry& G, const FKeyEvent& E)
{
	const FKey K = E.GetKey();
	if (K == EKeys::Escape || K == EKeys::Gamepad_FaceButton_Right || K == EKeys::Gamepad_Special_Right)
	{
		if (bInSettings) ShowMain();
		else if (bPause) OnPlay.ExecuteIfBound();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(G, E);
}
