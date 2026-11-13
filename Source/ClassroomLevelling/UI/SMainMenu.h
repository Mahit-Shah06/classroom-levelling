// Title / pause menu + settings. Pure Slate, every row is a button so mouse, keyboard and gamepad all work.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"

class SButton;

class SMainMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMainMenu) {}
		SLATE_ARGUMENT(bool, bPause)
		SLATE_EVENT(FSimpleDelegate, OnPlay)   // new game / resume
		SLATE_EVENT(FSimpleDelegate, OnTitle)  // pause -> back to title
		SLATE_EVENT(FSimpleDelegate, OnQuit)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	void FocusFirst();
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& G, const FKeyEvent& E) override;

private:
	void ShowMain();
	void ShowSettings();
	TSharedRef<SWidget> Item(const TFunction<FString()>& Label, const TFunction<void()>& OnClick);

	TSharedPtr<SVerticalBox> List;
	TSharedPtr<SButton> First;
	bool bPause = false, bInSettings = false;
	FSimpleDelegate OnPlay, OnTitle, OnQuit;
};
