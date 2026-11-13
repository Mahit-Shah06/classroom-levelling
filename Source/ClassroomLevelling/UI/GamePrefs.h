// Player prefs (non-graphics) persisted in GameUserSettings.ini. Engine graphics settings go through UGameUserSettings.
#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"

// X-macro: type, name, default
#define CL_PREFS(F, B) \
	F(TextSpeed, 40.f) F(MasterVol, 1.f) F(MusicVol, 0.6f) F(AmbientVol, 0.7f) F(SfxVol, 0.8f) F(VoiceVol, 1.f) \
	F(LookSens, 1.f) F(PadSens, 1.f) F(Fov, 90.f) F(Brightness, 2.2f) F(FrameLimit, 0.f) F(TextSize, 1.f) \
	B(bVoice, true) B(bHesitation, true) B(bBenchmarked, false) B(bInvertY, false) B(bVibration, true) \
	B(bMotionBlur, false) B(bShowFps, false) B(bAutoAdvance, false) B(bNameTags, true) B(bMarkers, true) B(bSprintToggle, false)

struct FGamePrefs
{
#define CL_DECL_F(N, D) float N = D;
#define CL_DECL_B(N, D) bool N = D;
	CL_PREFS(CL_DECL_F, CL_DECL_B)
#undef CL_DECL_F
#undef CL_DECL_B

	static FGamePrefs& Get() { static FGamePrefs P; static bool bLoaded = false; if (!bLoaded) { P.Load(); bLoaded = true; } return P; }

	void Load()
	{
		static const TCHAR* S = TEXT("ClassroomLevelling.Prefs");
#define CL_LOAD_F(N, D) GConfig->GetFloat(S, TEXT(#N), N, GGameUserSettingsIni);
#define CL_LOAD_B(N, D) GConfig->GetBool(S, TEXT(#N), N, GGameUserSettingsIni);
		CL_PREFS(CL_LOAD_F, CL_LOAD_B)
#undef CL_LOAD_F
#undef CL_LOAD_B
	}
	void Save() const
	{
		static const TCHAR* S = TEXT("ClassroomLevelling.Prefs");
#define CL_SAVE_F(N, D) GConfig->SetFloat(S, TEXT(#N), N, GGameUserSettingsIni);
#define CL_SAVE_B(N, D) GConfig->SetBool(S, TEXT(#N), N, GGameUserSettingsIni);
		CL_PREFS(CL_SAVE_F, CL_SAVE_B)
#undef CL_SAVE_F
#undef CL_SAVE_B
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	void ResetToDefaults() { const bool bB = bBenchmarked; *this = FGamePrefs(); bBenchmarked = bB; }
	float Vol(float Channel) const { return FMath::Clamp(MasterVol * Channel, 0.f, 1.f); }
};
