// Title / pause menu: continue, new game, save/load slots, settings (Graphics, Display, Audio, Controls, Gameplay).
// Pure Slate. Every row is a focusable button: mouse click / Enter / A = next value, Left/Right or D-pad = adjust.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"

class SButton;

struct FMenuActions
{
	TFunction<void()> NewGame, Resume, Title, Quit, ApplyPrefs;
	TFunction<void(const FString&)> Load, Save;
};

class SMainMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMainMenu) {}
		SLATE_ARGUMENT(bool, bPause)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const FMenuActions& InActions);
	void FocusFirst();
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& G, const FKeyEvent& E) override;

	static const TArray<FString>& SlotNames();
	static FString SlotLabel(const FString& Slot);

private:
	enum class EPage : uint8 { Main, Slots, Settings };
	void ShowMain();
	void ShowSlots(bool bSaving);
	void ShowSettings(int32 Tab);
	void Header(const FString& Title);
	void Note(const FString& Text);
	void Item(const TFunction<FString()>& Label, const TFunction<void(int32)>& OnAdjust, bool bEnabled = true); // Dir: +1 click/right, -1 left
	void Refocus();

	TSharedPtr<SVerticalBox> List;
	TArray<TSharedPtr<SButton>> Buttons;
	TMap<SWidget*, TFunction<void(int32)>> Adjusters;
	FMenuActions Actions;
	bool bPause = false;
	EPage Page = EPage::Main;
	int32 SettingsTab = 0;
	int32 FocusIndex = 0;
};
