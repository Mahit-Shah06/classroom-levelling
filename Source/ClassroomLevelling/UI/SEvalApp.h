// EVAL app (the school's evaluation app: level, rank, stats, the System's forecast of you) and the ending screen.
// Pure Slate, header-only. The app is a non-interactive overlay, so the player can keep walking with it open.
#pragma once

#include "CoreMinimal.h"
#include "Narrative/NarrativeSubsystem.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Styling/CoreStyle.h"
#include "Misc/ConfigCacheIni.h"

struct FEndingInfo { const TCHAR* Id; const TCHAR* Title; const TCHAR* Line; };
inline const TArray<FEndingInfo>& Endings()
{
	static const TArray<FEndingInfo> E = {
		{ TEXT("puppet"), TEXT("The Puppet at the Top"), TEXT("You did everything right. That was the plan.") },
		{ TEXT("director"), TEXT("The Director"), TEXT("You took the pen from the System and kept writing.") },
		{ TEXT("ghost"), TEXT("The Ghost"), TEXT("Nobody remembers who moved the pieces.") },
		{ TEXT("revolutionary"), TEXT("The Revolutionary"), TEXT("The ranking board is blank. So is the future.") },
		{ TEXT("dropout"), TEXT("The Dropout"), TEXT("You walked out of the gate. It was never locked.") },
		{ TEXT("clown"), TEXT("The Clown"), TEXT("The school is a circus now. Attendance is up.") },
		{ TEXT("kingmaker"), TEXT("The Kingmaker"), TEXT("Someone else wears the crown you built.") },
		{ TEXT("mirror"), TEXT("The Mirror"), TEXT("The System studied you until it became you.") },
		{ TEXT("mocked"), TEXT("Dead End"), TEXT("You ignored everyone. Everyone returned the favour.") } };
	return E;
}
inline const FEndingInfo& EndingInfo(FName Id) { for (const FEndingInfo& E : Endings()) if (Id == E.Id) return E; return Endings()[0]; }

// Endings found across runs live in the user settings ini (not in a save slot).
inline void MarkEndingFound(FName Id) { GConfig->SetBool(TEXT("ClassroomLevelling.Endings"), *Id.ToString(), true, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); }
inline int32 EndingsFound() { int32 N = 0; for (const FEndingInfo& E : Endings()) { bool b = false; GConfig->GetBool(TEXT("ClassroomLevelling.Endings"), E.Id, b, GGameUserSettingsIni); N += b; } return N; }

namespace EvalUI
{
	inline FSlateFontInfo Font(const char* S, int32 Z) { return FCoreStyle::GetDefaultFontStyle(S, Z); }
	inline const FLinearColor Gold(1.f, 0.72f, 0.18f), Dim(0.6f, 0.6f, 0.6f), Soft(0.85f, 0.85f, 0.85f);
	inline TSharedRef<SWidget> Label(const FString& S, const char* Style, int32 Size, const FLinearColor& C)
	{
		return SNew(STextBlock).Font(Font(Style, Size)).ColorAndOpacity(C).AutoWrapText(true).Text(FText::FromString(S));
	}
	inline TSharedRef<SWidget> BarRow(const FString& Name, float Pct, const FString& Value, const FLinearColor& Fill)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(SBox).WidthOverride(130.f)[ Label(Name, "Regular", 13, Soft) ] ]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[ SNew(SBox).HeightOverride(10.f)[ SNew(SProgressBar).Percent(FMath::Clamp(Pct, 0.f, 1.f)).FillColorAndOpacity(Fill) ] ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0, 0, 0)[ SNew(SBox).WidthOverride(46.f)[ Label(Value, "Mono", 12, FLinearColor::White) ] ];
	}
}

class SEvalApp : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEvalApp) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&)
	{
		ChildSlot.HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0, 0, 40, 0)
		[
			SNew(SBox).WidthOverride(520.f)
			[ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.88f)).Padding(FMargin(26, 22))[ SAssignNew(List, SVerticalBox) ] ]
		];
		SetVisibility(EVisibility::HitTestInvisible);
	}

	void Refresh(const UNarrativeSubsystem* N, const FString& CloseKey)
	{
		using namespace EvalUI;
		List->ClearChildren();
		if (!N) return;
		auto Add = [&](TSharedRef<SWidget> W, float Top = 0.f) { List->AddSlot().AutoHeight().Padding(0, Top, 0, 0)[ W ]; };
		static const TCHAR* Stats[] = { TEXT("Intellect"), TEXT("Charisma"), TEXT("Physique"), TEXT("Sanity"), TEXT("Cunning"), TEXT("Reputation"), TEXT("Funds"), TEXT("Luck") };

		Add(Label(TEXT("EVAL  //  STUDENT EVALUATION APP"), "Bold", 11, Dim));
		Add(Label(FString::Printf(TEXT("LV %d    RANK %d / %d    CLASS 1-%s"), N->Level, N->Rank(), N->CohortSize(), *N->Homeroom.ToString()), "Bold", 22, FLinearColor::White), 6.f);
		Add(BarRow(TEXT("Next level"), N->Xp / N->XpForNext(), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(100.f * N->Xp / N->XpForNext())), Gold), 8.f);

		Add(Label(N->OriginTitle().ToUpper(), "Bold", 13, Gold), 16.f);
		Add(Label(N->OriginBlurb(), "Italic", 12, Soft), 2.f);
		Add(Label(N->OriginSkill(), "Regular", 12, Dim), 2.f);

		Add(Label(TEXT("STATS"), "Bold", 13, Gold), 16.f);
		for (int32 i = 0; i < 8; ++i)
		{
			const float V = N->Player.General((EGeneralStat)i);
			Add(BarRow(Stats[i], V / 100.f, FString::FromInt(FMath::RoundToInt(V)), i == 3 && V < 20.f ? FLinearColor(0.9f, 0.2f, 0.2f) : FLinearColor(0.8f, 0.8f, 0.8f)), 4.f);
		}

		Add(Label(TEXT("SYSTEM FORECAST"), "Bold", 13, Gold), 16.f);
		Add(Label(N->PredTotal == 0 ? FString(TEXT("Insufficient data. Keep making choices."))
			: FString::Printf(TEXT("It called %d of your %d decisions in advance (%d%% predictable)."), N->PredHits, N->PredTotal, FMath::RoundToInt(N->Predictability() * 100.f)), "Regular", 13, Soft), 2.f);
		Add(Label(FString::Printf(TEXT("Projected outcome: %s"), *FString(EndingInfo(N->ForecastEnding()).Title).ToUpper()), "Regular", 13, Soft), 2.f);

		Add(Label(TEXT("PEOPLE"), "Bold", 13, Gold), 16.f);
		static const TPair<const TCHAR*, const TCHAR*> Roles[] = { { TEXT("rival"), TEXT("Rival") }, { TEXT("ally"), TEXT("Ally") }, { TEXT("gremlin"), TEXT("Wildcard") },
			{ TEXT("president"), TEXT("President") }, { TEXT("ex1"), TEXT("Former #1") }, { TEXT("broker"), TEXT("Broker") } };
		int32 Met = 0;
		for (const auto& R : Roles)
		{
			if (!N->Flags.Contains(FName(*FString::Printf(TEXT("met_%s"), R.Key)))) continue;
			const FName Id = N->RoleBindings.FindRef(FName(R.Key));
			const FNPCProfile* P = N->NPCs.Find(Id);
			if (!P) continue;
			const float Rel = N->Relationship.FindRef(Id);
			Add(BarRow(P->Name.ToString(), (Rel + 100.f) / 200.f, FString::Printf(TEXT("%+d"), FMath::RoundToInt(Rel)), Rel < 0.f ? FLinearColor(0.9f, 0.3f, 0.3f) : FLinearColor(0.4f, 0.8f, 0.6f)), 4.f);
			++Met;
		}
		if (!Met) Add(Label(TEXT("You have not met anyone who matters yet."), "Regular", 13, Dim), 2.f);

		Add(Label(FString::Printf(TEXT("[%s]  Close"), *CloseKey), "Regular", 12, Dim), 18.f);
	}

private:
	TSharedPtr<SVerticalBox> List;
};

class SEndScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEndScreen) {}
		SLATE_ARGUMENT(const UNarrativeSubsystem*, Narrative)
		SLATE_ARGUMENT(TFunction<void()>, OnNewRun)
		SLATE_ARGUMENT(TFunction<void()>, OnTitle)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args)
	{
		using namespace EvalUI;
		OnNewRun = Args._OnNewRun; OnTitle = Args._OnTitle;
		const UNarrativeSubsystem* N = Args._Narrative;
		const FName Id = N ? N->EndingReached() : NAME_None;
		const FEndingInfo& E = EndingInfo(Id);
		const bool bCanon = Id == TEXT("puppet"), bDead = Id == TEXT("mocked");
		const FString Cut = bDead ? TEXT("CUT FROM THE FILM") : bCanon ? TEXT("THEATRICAL CUT") : TEXT("DIRECTOR'S CUT");
		const FString Verdict = bDead ? TEXT("The System did not need to steer you. You did it yourself. Start again.")
			: bCanon ? TEXT("This is the ending the System wrote for everyone. Most students get it. Be harder to predict.")
			: TEXT("The System wrote one ending for everyone. This one is yours.");
		const bool bPad = FSlateApplication::IsInitialized() && FSlateApplication::Get().IsGamepadAttached();

		TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
		auto Add = [&](TSharedRef<SWidget> W, float Top = 0.f) { Col->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, Top, 0, 0)[ W ]; };
		Add(Label(Cut, "Bold", 16, bCanon || bDead ? Dim : Gold));
		Add(Label(FString(E.Title).ToUpper(), "Bold", 54, FLinearColor::White), 4.f);
		Add(Label(E.Line, "Italic", 18, Soft), 6.f);
		Add(Label(Verdict, "Regular", 14, Dim), 18.f);
		if (N)
		{
			Add(Label(FString::Printf(TEXT("Level %d     Rank %d / %d     %s     %d min"), N->Level, N->Rank(), N->CohortSize(), *N->OriginTitle(), FMath::RoundToInt(N->PlayTime / 60.f)), "Regular", 16, FLinearColor::White), 30.f);
			Add(Label(FString::Printf(TEXT("The System predicted %d of your %d decisions (%d%%)."), N->PredHits, N->PredTotal, FMath::RoundToInt(N->Predictability() * 100.f)), "Regular", 16, FLinearColor::White), 6.f);
		}
		Add(Label(FString::Printf(TEXT("Endings found: %d / %d"), EndingsFound(), Endings().Num()), "Bold", 16, Gold), 6.f);
		Add(Label(TEXT("CLASSROOM LEVELLING"), "Bold", 13, Gold), 40.f);
		Add(Label(TEXT("A solo student project for the Epic AI MegaJam 2026. Directed by one human; built with AI agents."), "Regular", 12, Dim), 4.f);
		Add(Label(TEXT("Code: Claude Code.  Writing: Grok via Cursor CLI.  Props: Hyper3D Rodin.  Voices: Piper TTS.  Music and sound: procedural synth.  Engine: Unreal Engine 5.7."), "Regular", 12, Dim), 2.f);
		Add(Label(FString::Printf(TEXT("[%s]  New run          [%s]  Title screen"), bPad ? TEXT("A") : TEXT("Enter"), bPad ? TEXT("B") : TEXT("Esc")), "Regular", 15, FLinearColor::White), 40.f);

		ChildSlot
		[ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.93f)).HAlign(HAlign_Center).VAlign(VAlign_Center)[ Col ] ];
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent& Ev) override
	{
		const FKey K = Ev.GetKey();
		// Copy first: either action destroys this widget.
		if (K == EKeys::Enter || K == EKeys::Gamepad_FaceButton_Bottom) { const TFunction<void()> F = OnNewRun; if (F) F(); }
		else if (K == EKeys::Escape || K == EKeys::Gamepad_FaceButton_Right) { const TFunction<void()> F = OnTitle; if (F) F(); }
		return FReply::Handled();
	}

private:
	TFunction<void()> OnNewRun, OnTitle;
};
