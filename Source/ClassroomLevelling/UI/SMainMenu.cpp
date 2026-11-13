#include "SMainMenu.h"
#include "GamePrefs.h"
#include "Narrative/NarrativeSubsystem.h"
#include "Narrative/CLSaveGame.h"
#include "Narrative/GameAudio.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

static FSlateFontInfo MenuFont(const char* S, int32 Sz) { return FCoreStyle::GetDefaultFontStyle(S, Sz); }
static const TCHAR* GQ[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") };
static UGameUserSettings* GS() { return GEngine ? GEngine->GetGameUserSettings() : nullptr; }
static void Sfx(FName N)
{
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWorld())
		if (UGameAudio* A = UGameAudio::Get(GEngine->GameViewport->GetWorld())) A->PlaySfx(GEngine->GameViewport->GetWorld(), N);
}

const TArray<FString>& SMainMenu::SlotNames() { static TArray<FString> S = { TEXT("auto"), TEXT("slot0"), TEXT("slot1"), TEXT("slot2") }; return S; }

FString SMainMenu::SlotLabel(const FString& Slot)
{
	const FString Name = Slot == TEXT("auto") ? TEXT("Autosave") : FString::Printf(TEXT("Slot %d"), FCString::Atoi(*Slot.Right(1)) + 1);
	const UCLSaveGame* G = UNarrativeSubsystem::PeekSlot(Slot);
	if (!G) return Name + TEXT("   -   Empty");
	FString Area = G->Area.IsNone() ? FString(TEXT("Campus")) : G->Area.ToString();
	if (Area.Len()) Area[0] = FChar::ToUpper(Area[0]);
	return FString::Printf(TEXT("%s   -   %s   -   %dm played   -   %s"), *Name, *Area, FMath::RoundToInt(G->PlayTime / 60.f), *G->Saved.ToString(TEXT("%Y-%m-%d %H:%M")));
}

void SMainMenu::Construct(const FArguments& Args, const FMenuActions& InActions)
{
	bPause = Args._bPause; Actions = InActions;
	ChildSlot.HAlign(HAlign_Left)
	[
		SNew(SBox).WidthOverride(940.f)[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.02f, 0.03f, 0.08f, bPause ? 0.6f : 0.45f))
		.Padding(FMargin(110, 60, 0, 60)).VAlign(VAlign_Center).HAlign(HAlign_Left)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[ SNew(STextBlock).Text(FText::FromString(TEXT("CLASSROOM"))).Font(MenuFont("Bold", 58)).ColorAndOpacity(FLinearColor::White).ShadowOffset(FVector2D(2, 2)) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, -16, 0, 0)
			[ SNew(STextBlock).Text(FText::FromString(TEXT("LEVELLING"))).Font(MenuFont("Light", 58)).ColorAndOpacity(FLinearColor(1.f, 0.72f, 0.18f)).ShadowOffset(FVector2D(2, 2)) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 28)
			[ SNew(STextBlock).Text(FText::FromString(TEXT("99% placement rate. Your evaluation is always on."))).Font(MenuFont("Italic", 14)).ColorAndOpacity(FLinearColor(0.55f, 0.85f, 1.f)).ShadowOffset(FVector2D(1, 1)) ]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[ SNew(SBox).WidthOverride(720.f).MaxDesiredHeight(620.f)[ SNew(SScrollBox) + SScrollBox::Slot()[ SAssignNew(List, SVerticalBox) ] ] ]
		]]
	];
	ShowMain();
}

void SMainMenu::Header(const FString& Title)
{
	List->AddSlot().AutoHeight().Padding(0, 10, 0, 6)[ SNew(STextBlock).Font(MenuFont("Bold", 14)).ColorAndOpacity(FLinearColor(1.f, 0.72f, 0.18f)).Text(FText::FromString(Title.ToUpper())) ];
}

void SMainMenu::Note(const FString& Text)
{
	List->AddSlot().AutoHeight().Padding(4, 6)[ SNew(STextBlock).Font(MenuFont("Regular", 13)).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f)).Text(FText::FromString(Text)) ];
}

void SMainMenu::Item(const TFunction<FString()>& Label, const TFunction<void(int32)>& OnAdjust, bool bEnabled)
{
	TSharedRef<SButton> B = SNew(SButton)
		.ButtonColorAndOpacity(FLinearColor(0.02f, 0.30f, 0.42f, 0.92f))
		.ContentPadding(FMargin(18, 9))
		.IsEnabled(bEnabled)
		.OnClicked_Lambda([OnAdjust]() { Sfx("ui_select"); OnAdjust(1); return FReply::Handled(); })
		[ SNew(STextBlock).Font(MenuFont("Regular", 18)).ColorAndOpacity(bEnabled ? FLinearColor::White : FLinearColor(0.4f, 0.4f, 0.4f)).Text_Lambda([Label]() { return FText::FromString(Label()); }) ];
	Buttons.Add(B);
	Adjusters.Add(&B.Get(), OnAdjust);
	List->AddSlot().AutoHeight().Padding(FMargin(0, 3))[ B ];
}

void SMainMenu::FocusFirst() { FocusIndex = 0; Refocus(); }

void SMainMenu::Refocus()
{
	for (int32 i = FMath::Clamp(FocusIndex, 0, Buttons.Num() - 1); i < Buttons.Num() && i >= 0; ++i)
		if (Buttons[i]->IsEnabled()) { FSlateApplication::Get().SetAllUserFocus(Buttons[i], EFocusCause::Navigation); return; }
}

static void Reset(TSharedPtr<SVerticalBox>& L, TArray<TSharedPtr<SButton>>& B, TMap<SWidget*, TFunction<void(int32)>>& A) { L->ClearChildren(); B.Reset(); A.Reset(); }

void SMainMenu::ShowMain()
{
	Page = EPage::Main; Reset(List, Buttons, Adjusters);
	bool bAnySave = false;
	for (const FString& S : SlotNames()) bAnySave |= UNarrativeSubsystem::PeekSlot(S) != nullptr;
	if (bPause) Item([]() { return FString(TEXT("Resume")); }, [this](int32 D) { if (D > 0) Actions.Resume(); });
	else
	{
		if (bAnySave) Item([]() { return FString(TEXT("Continue")); }, [this](int32 D)
		{
			if (D <= 0) return;
			FString Best; FDateTime BestT = FDateTime::MinValue();
			for (const FString& S : SlotNames()) if (const UCLSaveGame* G = UNarrativeSubsystem::PeekSlot(S)) if (G->Saved > BestT) { BestT = G->Saved; Best = S; }
			if (!Best.IsEmpty()) Actions.Load(Best);
		});
		Item([]() { return FString(TEXT("New Game")); }, [this](int32 D) { if (D > 0) Actions.NewGame(); });
	}
	if (bPause) Item([]() { return FString(TEXT("Save Game")); }, [this](int32 D) { if (D > 0) ShowSlots(true); });
	Item([]() { return FString(TEXT("Load Game")); }, [this](int32 D) { if (D > 0) ShowSlots(false); }, bAnySave);
	Item([]() { return FString(TEXT("Settings")); }, [this](int32 D) { if (D > 0) ShowSettings(0); });
	if (bPause) Item([]() { return FString(TEXT("Quit to Title")); }, [this](int32 D) { if (D > 0) Actions.Title(); });
	Item([]() { return FString(TEXT("Quit Game")); }, [this](int32 D) { if (D > 0) Actions.Quit(); });
	Note(TEXT("Mouse / keyboard or controller.  Enter / A select,  Esc / B back,  Left / Right adjust."));
	FocusFirst();
}

void SMainMenu::ShowSlots(bool bSaving)
{
	Page = EPage::Slots; Reset(List, Buttons, Adjusters);
	Header(bSaving ? TEXT("Save game") : TEXT("Load game"));
	for (const FString& S : SlotNames())
	{
		if (bSaving && S == TEXT("auto")) continue; // autosave is written automatically after each scene
		const bool bExists = UNarrativeSubsystem::PeekSlot(S) != nullptr;
		Item([S]() { return SlotLabel(S); }, [this, S, bSaving](int32 D)
		{
			if (D <= 0) return;
			if (bSaving) { Actions.Save(S); Sfx("notify"); } // label refreshes via lambda
			else Actions.Load(S);
		}, bSaving || bExists);
	}
	Item([]() { return FString(TEXT("Back")); }, [this](int32 D) { if (D > 0) ShowMain(); });
	FocusFirst();
}

// ------------------------------------------------------------------ settings
void SMainMenu::ShowSettings(int32 Tab)
{
	Page = EPage::Settings; SettingsTab = Tab; Reset(List, Buttons, Adjusters);
	FGamePrefs& P = FGamePrefs::Get();
	UGameUserSettings* G = GS();
	auto Apply = [this]() { if (UGameUserSettings* S = GS()) { S->ApplySettings(false); S->SaveSettings(); } FGamePrefs::Get().Save(); if (Actions.ApplyPrefs) Actions.ApplyPrefs(); };
	auto Cycle = [](int32 V, int32 D, int32 N) { return (V + D + N) % N; };
	auto Pct = [](float V) { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V * 100.f)); };
	auto Step = [](float& V, int32 D, float S, float Lo, float Hi) { V = FMath::Clamp(FMath::RoundToFloat((V + D * S) / S) * S, Lo, Hi); };
	auto OnOff = [](bool b) { return b ? FString(TEXT("On")) : FString(TEXT("Off")); };

	static const TCHAR* Tabs[] = { TEXT("Graphics"), TEXT("Display"), TEXT("Audio"), TEXT("Controls"), TEXT("Gameplay") };
	Item([this]() { return FString::Printf(TEXT("<   %s   >"), Tabs[SettingsTab]); }, [this, Cycle](int32 D) { ShowSettings(Cycle(SettingsTab, D, 5)); });

	if (Tab == 0 && G)
	{
		Item([G]() { const int32 L = G->GetOverallScalabilityLevel(); return FString::Printf(TEXT("Preset:  %s"), L < 0 ? TEXT("Custom") : GQ[FMath::Clamp(L, 0, 4)]); },
			[G, Apply, Cycle](int32 D) { G->SetOverallScalabilityLevel(Cycle(FMath::Max(G->GetOverallScalabilityLevel(), 0), D, 4)); Apply(); });
		Item([]() { return FString(TEXT("Auto-tune graphics (next launch)")); }, [G, Apply](int32 D) { if (D > 0) { FGamePrefs::Get().bBenchmarked = false; G->SetOverallScalabilityLevel(2); Apply(); } }); // re-tunes on next launch
		struct FQ { const TCHAR* N; int32 (UGameUserSettings::*Get)() const; void (UGameUserSettings::*Set)(int32); };
		static const FQ Qs[] = {
			{ TEXT("View distance"), &UGameUserSettings::GetViewDistanceQuality, &UGameUserSettings::SetViewDistanceQuality },
			{ TEXT("Shadows"), &UGameUserSettings::GetShadowQuality, &UGameUserSettings::SetShadowQuality },
			{ TEXT("Global illumination"), &UGameUserSettings::GetGlobalIlluminationQuality, &UGameUserSettings::SetGlobalIlluminationQuality },
			{ TEXT("Reflections"), &UGameUserSettings::GetReflectionQuality, &UGameUserSettings::SetReflectionQuality },
			{ TEXT("Anti-aliasing"), &UGameUserSettings::GetAntiAliasingQuality, &UGameUserSettings::SetAntiAliasingQuality },
			{ TEXT("Textures"), &UGameUserSettings::GetTextureQuality, &UGameUserSettings::SetTextureQuality },
			{ TEXT("Effects"), &UGameUserSettings::GetVisualEffectQuality, &UGameUserSettings::SetVisualEffectQuality },
			{ TEXT("Post-processing"), &UGameUserSettings::GetPostProcessingQuality, &UGameUserSettings::SetPostProcessingQuality },
			{ TEXT("Foliage"), &UGameUserSettings::GetFoliageQuality, &UGameUserSettings::SetFoliageQuality },
			{ TEXT("Shading"), &UGameUserSettings::GetShadingQuality, &UGameUserSettings::SetShadingQuality } };
		for (const FQ& Q : Qs)
			Item([G, Q]() { return FString::Printf(TEXT("%s:  %s"), Q.N, GQ[FMath::Clamp((G->*Q.Get)(), 0, 4)]); },
				[G, Q, Apply, Cycle](int32 D) { (G->*Q.Set)(Cycle(FMath::Clamp((G->*Q.Get)(), 0, 3), D, 4)); Apply(); });
		Item([G, Pct]() { return FString::Printf(TEXT("Render scale:  %s"), *Pct(G->GetResolutionScaleNormalized())); },
			[G, Apply](int32 D) { G->SetResolutionScaleNormalized(FMath::Clamp(G->GetResolutionScaleNormalized() + D * 0.05f, 0.5f, 1.f)); Apply(); });
		Item([&P, OnOff]() { return FString::Printf(TEXT("Motion blur:  %s"), *OnOff(P.bMotionBlur)); }, [&P, Apply](int32) { P.bMotionBlur = !P.bMotionBlur; Apply(); });
	}
	else if (Tab == 1 && G)
	{
		static const TCHAR* WM[] = { TEXT("Fullscreen"), TEXT("Borderless"), TEXT("Windowed") };
		Item([G]() { return FString::Printf(TEXT("Display mode:  %s"), WM[FMath::Clamp((int32)G->GetFullscreenMode(), 0, 2)]); },
			[G, Apply, Cycle](int32 D) { G->SetFullscreenMode(EWindowMode::ConvertIntToWindowMode(Cycle((int32)G->GetFullscreenMode(), D, 3))); Apply(); });
		Item([G]() { const FIntPoint R = G->GetScreenResolution(); return FString::Printf(TEXT("Resolution:  %d x %d"), R.X, R.Y); },
			[G, Apply](int32 D)
			{
				TArray<FIntPoint> Rs; UKismetSystemLibrary::GetSupportedFullscreenResolutions(Rs);
				if (Rs.Num() == 0) return;
				int32 Idx = Rs.IndexOfByKey(G->GetScreenResolution()); if (Idx == INDEX_NONE) Idx = Rs.Num() - 1;
				G->SetScreenResolution(Rs[(Idx + D + Rs.Num()) % Rs.Num()]); Apply();
			});
		Item([G, OnOff]() { return FString::Printf(TEXT("VSync:  %s"), *OnOff(G->IsVSyncEnabled())); }, [G, Apply](int32) { G->SetVSyncEnabled(!G->IsVSyncEnabled()); Apply(); });
		static const float Limits[] = { 0.f, 30.f, 60.f, 90.f, 120.f, 144.f, 165.f, 240.f };
		Item([&P]() { return P.FrameLimit <= 0.f ? FString(TEXT("Frame limit:  Unlimited")) : FString::Printf(TEXT("Frame limit:  %d"), (int32)P.FrameLimit); },
			[&P, Apply, Cycle](int32 D) { int32 i = 0; for (int32 k = 0; k < 8; ++k) if (P.FrameLimit >= Limits[k]) i = k; P.FrameLimit = Limits[Cycle(i, D, 8)]; Apply(); });
		Item([&P]() { return FString::Printf(TEXT("Field of view:  %d"), (int32)P.Fov); }, [&P, Apply, Step](int32 D) { Step(P.Fov, D, 5.f, 70.f, 110.f); Apply(); });
		Item([&P]() { return FString::Printf(TEXT("Brightness (gamma):  %.1f"), P.Brightness); }, [&P, Apply, Step](int32 D) { Step(P.Brightness, D, 0.1f, 1.5f, 3.f); Apply(); });
		Item([&P, OnOff]() { return FString::Printf(TEXT("Show FPS:  %s"), *OnOff(P.bShowFps)); }, [&P, Apply](int32) { P.bShowFps = !P.bShowFps; Apply(); });
	}
	else if (Tab == 2)
	{
		struct FV { const TCHAR* N; float FGamePrefs::* V; };
		static const FV Vs[] = { { TEXT("Master"), &FGamePrefs::MasterVol }, { TEXT("Music"), &FGamePrefs::MusicVol }, { TEXT("Ambience"), &FGamePrefs::AmbientVol },
			{ TEXT("Effects / UI"), &FGamePrefs::SfxVol }, { TEXT("Voices"), &FGamePrefs::VoiceVol } };
		for (const FV& V : Vs)
			Item([&P, V, Pct]() { return FString::Printf(TEXT("%s volume:  %s"), V.N, *Pct(P.*V.V)); }, [&P, V, Apply, Step](int32 D) { Step(P.*V.V, D == 0 ? 1 : D, 0.1f, 0.f, 1.f); Apply(); });
		Item([&P, OnOff]() { return FString::Printf(TEXT("Voice acting (Piper TTS):  %s"), *OnOff(P.bVoice)); }, [&P, Apply](int32) { P.bVoice = !P.bVoice; Apply(); });
		Note(TEXT("Subtitles are always shown. Voices are generated on your CPU while you read (very light)."));
	}
	else if (Tab == 3)
	{
		Item([&P]() { return FString::Printf(TEXT("Mouse sensitivity:  %.2f"), P.LookSens); }, [&P, Apply, Step](int32 D) { Step(P.LookSens, D, 0.05f, 0.1f, 3.f); Apply(); });
		Item([&P]() { return FString::Printf(TEXT("Controller sensitivity:  %.2f"), P.PadSens); }, [&P, Apply, Step](int32 D) { Step(P.PadSens, D, 0.05f, 0.1f, 3.f); Apply(); });
		Item([&P, OnOff]() { return FString::Printf(TEXT("Invert Y:  %s"), *OnOff(P.bInvertY)); }, [&P, Apply](int32) { P.bInvertY = !P.bInvertY; Apply(); });
		Item([&P, OnOff]() { return FString::Printf(TEXT("Vibration:  %s"), *OnOff(P.bVibration)); }, [&P, Apply](int32) { P.bVibration = !P.bVibration; Apply(); });
		Item([&P]() { return FString::Printf(TEXT("Sprint:  %s"), P.bSprintToggle ? TEXT("Toggle") : TEXT("Hold")); }, [&P, Apply](int32) { P.bSprintToggle = !P.bSprintToggle; Apply(); });
		Header(TEXT("Keyboard / mouse"));
		Note(TEXT("Move WASD   Look Mouse   Sprint Shift   Jump Space   Talk E   Pause Esc\nDialogue: 1-4 choose,  Space / Click continue"));
		Header(TEXT("Controller"));
		Note(TEXT("Move Left stick   Look Right stick   Sprint L3   Jump A   Talk X   Pause Start\nDialogue: A / B / X / Y choose,  A continue"));
	}
	else if (Tab == 4)
	{
		static const float Speeds[] = { 25.f, 40.f, 70.f, 10000.f };
		static const TCHAR* SpeedN[] = { TEXT("Slow"), TEXT("Normal"), TEXT("Fast"), TEXT("Instant") };
		Item([&P]() { int32 i = 0; for (int32 k = 0; k < 4; ++k) if (P.TextSpeed >= Speeds[k]) i = k; return FString::Printf(TEXT("Text speed:  %s"), SpeedN[i]); },
			[&P, Apply, Cycle](int32 D) { int32 i = 0; for (int32 k = 0; k < 4; ++k) if (P.TextSpeed >= Speeds[k]) i = k; P.TextSpeed = Speeds[Cycle(i, D, 4)]; Apply(); });
		static const TCHAR* Sizes[] = { TEXT("Small"), TEXT("Normal"), TEXT("Large") };
		Item([&P]() { return FString::Printf(TEXT("Text size:  %s"), Sizes[FMath::Clamp((int32)P.TextSize, 0, 2)]); }, [&P, Apply, Cycle](int32 D) { P.TextSize = (float)Cycle((int32)P.TextSize, D, 3); Apply(); });
		Item([&P, OnOff]() { return FString::Printf(TEXT("Auto-advance dialogue:  %s"), *OnOff(P.bAutoAdvance)); }, [&P, Apply](int32) { P.bAutoAdvance = !P.bAutoAdvance; Apply(); });
		Item([&P]() { return FString::Printf(TEXT("Hesitation timer:  %s"), P.bHesitation ? TEXT("On") : TEXT("Off (relaxed)")); }, [&P, Apply](int32) { P.bHesitation = !P.bHesitation; Apply(); });
		Item([&P, OnOff]() { return FString::Printf(TEXT("Name tags:  %s"), *OnOff(P.bNameTags)); }, [&P, Apply](int32) { P.bNameTags = !P.bNameTags; Apply(); });
		Item([&P, OnOff]() { return FString::Printf(TEXT("Story markers (!):  %s"), *OnOff(P.bMarkers)); }, [&P, Apply](int32) { P.bMarkers = !P.bMarkers; Apply(); });
	}

	Item([]() { return FString(TEXT("Reset all to defaults")); }, [this, G, Apply](int32 D)
	{
		if (D <= 0) return;
		FGamePrefs::Get().ResetToDefaults();
		if (G) { G->SetToDefaults(); G->SetOverallScalabilityLevel(2); }
		Apply(); ShowSettings(SettingsTab);
	});
	Item([]() { return FString(TEXT("Back")); }, [this](int32 D) { if (D > 0) ShowMain(); });
	FocusFirst();
}

FReply SMainMenu::OnKeyDown(const FGeometry& G, const FKeyEvent& E)
{
	const FKey K = E.GetKey();
	if (K == EKeys::Escape || K == EKeys::Gamepad_FaceButton_Right || K == EKeys::BackSpace)
	{
		Sfx("ui_back");
		if (Page != EPage::Main) ShowMain();
		else if (bPause) Actions.Resume();
		return FReply::Handled();
	}
	if (K == EKeys::Gamepad_Special_Right && bPause) { Actions.Resume(); return FReply::Handled(); }
	const int32 Dir = (K == EKeys::Left || K == EKeys::Gamepad_DPad_Left || K == EKeys::A) ? -1 :
		(K == EKeys::Right || K == EKeys::Gamepad_DPad_Right || K == EKeys::D) ? 1 : 0;
	if (Dir != 0)
	{
		TSharedPtr<SWidget> F = FSlateApplication::Get().GetUserFocusedWidget(0);
		if (const TFunction<void(int32)>* Adj = F.IsValid() ? Adjusters.Find(F.Get()) : nullptr)
		{
			// Only value rows react to left/right (action rows ignore -1 and would misfire on +1).
			const int32 Idx = Buttons.IndexOfByPredicate([&](const TSharedPtr<SButton>& B) { return B.Get() == F.Get(); });
			if (Page == EPage::Settings && Idx >= 0 && Idx < Buttons.Num() - 2) { Sfx("ui_move"); FocusIndex = Idx; const TFunction<void(int32)> Copy = *Adj; Copy(Dir); if (Idx == 0) Refocus(); }
			return FReply::Handled();
		}
	}
	if (K == EKeys::Up || K == EKeys::Down || K == EKeys::Gamepad_DPad_Up || K == EKeys::Gamepad_DPad_Down || K == EKeys::Gamepad_LeftStick_Up || K == EKeys::Gamepad_LeftStick_Down) Sfx("ui_move");
	return SCompoundWidget::OnKeyDown(G, E);
}
