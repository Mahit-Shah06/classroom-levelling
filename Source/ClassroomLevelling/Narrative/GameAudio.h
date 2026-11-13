// Music / ambience / UI / footstep audio from raw WAVs in Content/Audio (synthesized by tools/gen_audio.py).
// No binary sound assets: WAVs are decoded once and fed to procedural waves; loops re-queue on underflow.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameAudio.generated.h"

class UAudioComponent;
class USoundWaveProcedural;

UCLASS()
class CLASSROOMLEVELLING_API UGameAudio : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UGameAudio* Get(const UObject* Ctx);
	static bool DecodeWav(const TArray<uint8>& Bytes, int32& OutRate, int32& OutChannels, int32& OutOffset, int32& OutLen);

	void PlaySfx(UObject* Ctx, FName Name, float VolumeScale = 1.f); // ui_move, ui_select, notify, footstep_1...
	void PlayMusic(UObject* Ctx, FName Name);                          // looped, cross-fades; None = stop
	void PlayAmbient(UObject* Ctx, FName Name);
	void ApplyVolumes();                                               // after settings change

private:
	TSharedPtr<TArray<uint8>> Pcm(FName Name, int32& Rate, int32& Channels);
	UAudioComponent* PlayLoop(UObject* Ctx, FName Name, float Vol);

	TMap<FName, TSharedPtr<TArray<uint8>>> Cache;
	TMap<FName, FIntPoint> Formats; // rate, channels
	UPROPERTY() TObjectPtr<UAudioComponent> Music;
	UPROPERTY() TObjectPtr<UAudioComponent> Ambient;
	FName MusicName, AmbientName;
};
