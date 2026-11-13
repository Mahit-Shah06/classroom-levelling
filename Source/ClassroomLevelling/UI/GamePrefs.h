// Player prefs (non-graphics) persisted in GameUserSettings.ini. Graphics go through UGameUserSettings.
#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"

struct FGamePrefs
{
	float TextSpeed = 40.f;   // chars/sec
	float Volume = 1.f;
	bool bVoice = true;
	bool bHesitation = true;  // accessibility: off = no timer pressure
	bool bBenchmarked = false;

	static FGamePrefs& Get() { static FGamePrefs P; static bool bLoaded = false; if (!bLoaded) { P.Load(); bLoaded = true; } return P; }

	void Load()
	{
		static const TCHAR* S = TEXT("ClassroomLevelling.Prefs");
		GConfig->GetFloat(S, TEXT("TextSpeed"), TextSpeed, GGameUserSettingsIni);
		GConfig->GetFloat(S, TEXT("Volume"), Volume, GGameUserSettingsIni);
		GConfig->GetBool(S, TEXT("Voice"), bVoice, GGameUserSettingsIni);
		GConfig->GetBool(S, TEXT("Hesitation"), bHesitation, GGameUserSettingsIni);
		GConfig->GetBool(S, TEXT("Benchmarked"), bBenchmarked, GGameUserSettingsIni);
	}
	void Save() const
	{
		static const TCHAR* S = TEXT("ClassroomLevelling.Prefs");
		GConfig->SetFloat(S, TEXT("TextSpeed"), TextSpeed, GGameUserSettingsIni);
		GConfig->SetFloat(S, TEXT("Volume"), Volume, GGameUserSettingsIni);
		GConfig->SetBool(S, TEXT("Voice"), bVoice, GGameUserSettingsIni);
		GConfig->SetBool(S, TEXT("Hesitation"), bHesitation, GGameUserSettingsIni);
		GConfig->SetBool(S, TEXT("Benchmarked"), bBenchmarked, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
};
