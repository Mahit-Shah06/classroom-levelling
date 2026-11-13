#include "GameAudio.h"
#include "UI/GamePrefs.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UGameAudio* UGameAudio::Get(const UObject* Ctx)
{
	const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	return W && W->GetGameInstance() ? W->GetGameInstance()->GetSubsystem<UGameAudio>() : nullptr;
}

bool UGameAudio::DecodeWav(const TArray<uint8>& B, int32& Rate, int32& Ch, int32& Off, int32& Len)
{
	Rate = 22050; Ch = 1; Off = -1; Len = 0;
	if (B.Num() < 44 || FMemory::Memcmp(B.GetData(), "RIFF", 4) != 0) return false;
	for (int32 i = 12; i + 8 <= B.Num();)
	{
		int32 L; FMemory::Memcpy(&L, &B[i + 4], 4);
		if (FMemory::Memcmp(&B[i], "fmt ", 4) == 0) { int16 C; FMemory::Memcpy(&C, &B[i + 10], 2); Ch = C; FMemory::Memcpy(&Rate, &B[i + 12], 4); }
		else if (FMemory::Memcmp(&B[i], "data", 4) == 0) { Off = i + 8; Len = FMath::Min(L, B.Num() - Off) & ~1; return true; }
		if (L < 0) return false;
		i += 8 + L + (L & 1);
	}
	return false;
}

TSharedPtr<TArray<uint8>> UGameAudio::Pcm(FName Name, int32& Rate, int32& Channels)
{
	if (const TSharedPtr<TArray<uint8>>* C = Cache.Find(Name)) { Rate = Formats[Name].X; Channels = Formats[Name].Y; return *C; }
	TArray<uint8> Bytes;
	const FString Path = FPaths::ProjectContentDir() / TEXT("Audio") / (Name.ToString() + TEXT(".wav"));
	int32 Off, Len;
	TSharedPtr<TArray<uint8>> Out;
	if (FFileHelper::LoadFileToArray(Bytes, *Path) && DecodeWav(Bytes, Rate, Channels, Off, Len))
		Out = MakeShared<TArray<uint8>>(Bytes.GetData() + Off, Len);
	Cache.Add(Name, Out); // cache misses too (missing file = silent)
	Formats.Add(Name, FIntPoint(Rate, Channels));
	return Out;
}

static USoundWaveProcedural* MakeWave(const TSharedPtr<TArray<uint8>>& Data, int32 Rate, int32 Channels, bool bLoop)
{
	USoundWaveProcedural* W = NewObject<USoundWaveProcedural>();
	W->SetSampleRate(Rate);
	W->NumChannels = Channels;
	W->Duration = bLoop ? INDEFINITELY_LOOPING_DURATION : Data->Num() / float(Rate * Channels * 2);
	W->bLooping = false;
	W->QueueAudio(Data->GetData(), Data->Num());
	if (bLoop) // seamless loop: refill from the audio thread whenever the buffer runs low
		W->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateLambda([Data](USoundWaveProcedural* Wave, int32) { Wave->QueueAudio(Data->GetData(), Data->Num()); });
	return W;
}

void UGameAudio::PlaySfx(UObject* Ctx, FName Name, float Scale)
{
	int32 Rate, Ch;
	TSharedPtr<TArray<uint8>> D = Pcm(Name, Rate, Ch);
	if (!D.IsValid() || !Ctx || !Ctx->GetWorld()) return;
	UGameplayStatics::PlaySound2D(Ctx, MakeWave(D, Rate, Ch, false), FGamePrefs::Get().Vol(FGamePrefs::Get().SfxVol) * Scale);
}

UAudioComponent* UGameAudio::PlayLoop(UObject* Ctx, FName Name, float Vol)
{
	int32 Rate, Ch;
	TSharedPtr<TArray<uint8>> D = Pcm(Name, Rate, Ch);
	if (!D.IsValid() || !Ctx || !Ctx->GetWorld()) return nullptr;
	UAudioComponent* C = UGameplayStatics::CreateSound2D(Ctx, MakeWave(D, Rate, Ch, true), 1.f, 1.f, 0.f, nullptr, true, false);
	if (C) { C->SetVolumeMultiplier(FMath::Max(Vol, 0.001f)); C->FadeIn(1.5f, 1.f); }
	return C;
}

void UGameAudio::PlayMusic(UObject* Ctx, FName Name)
{
	if (Name == MusicName && Music && Music->IsPlaying()) return;
	if (Music) Music->FadeOut(1.5f, 0.f);
	MusicName = Name;
	Music = Name.IsNone() ? nullptr : PlayLoop(Ctx, Name, FGamePrefs::Get().Vol(FGamePrefs::Get().MusicVol));
}

void UGameAudio::PlayAmbient(UObject* Ctx, FName Name)
{
	if (Name == AmbientName && Ambient && Ambient->IsPlaying()) return;
	if (Ambient) Ambient->FadeOut(2.f, 0.f);
	AmbientName = Name;
	Ambient = Name.IsNone() ? nullptr : PlayLoop(Ctx, Name, FGamePrefs::Get().Vol(FGamePrefs::Get().AmbientVol));
}

void UGameAudio::ApplyVolumes()
{
	const FGamePrefs& P = FGamePrefs::Get();
	if (Music) Music->SetVolumeMultiplier(FMath::Max(P.Vol(P.MusicVol), 0.001f));
	if (Ambient) Ambient->SetVolumeMultiplier(FMath::Max(P.Vol(P.AmbientVol), 0.001f));
}
