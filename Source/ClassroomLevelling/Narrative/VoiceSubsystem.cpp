#include "VoiceSubsystem.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogPiper, Log, All);

void UVoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CacheDir = FPaths::ProjectSavedDir() / TEXT("VoiceCache");
	IFileManager::Get().MakeDirectory(*CacheDir, true);

	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("ThirdParty/Piper"));
#if PLATFORM_WINDOWS
	const FString Exe = Dir / TEXT("piper/piper.exe");
#else
	const FString Exe = Dir / TEXT("piper/piper");
#endif
	const FString Model = Dir / TEXT("voice.onnx");
	if (!FPaths::FileExists(Exe) || !FPaths::FileExists(Model))
	{
		UE_LOG(LogPiper, Warning, TEXT("Piper not found (%s). Voices off. Run tools/setup_piper.sh"), *Exe);
		return;
	}
	FPlatformProcess::CreatePipe(InRead, InWrite, true);   // write end is ours
	FPlatformProcess::CreatePipe(OutRead, OutWrite, false);
	const FString Args = FString::Printf(TEXT("-m \"%s\" --json-input -q"), *Model);
	Proc = FPlatformProcess::CreateProc(*Exe, *Args, false, true, true, nullptr, -1, *(Dir / TEXT("piper")), OutWrite, InRead);
	if (!Proc.IsValid()) { UE_LOG(LogPiper, Warning, TEXT("Failed to start piper")); return; }
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVoiceSubsystem::Tick), 0.05f);
	UE_LOG(LogPiper, Log, TEXT("Piper started"));
}

void UVoiceSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	if (Proc.IsValid()) { FPlatformProcess::TerminateProc(Proc, true); FPlatformProcess::CloseProc(Proc); }
	FPlatformProcess::ClosePipe(InRead, InWrite);
	FPlatformProcess::ClosePipe(OutRead, OutWrite);
	InRead = InWrite = OutRead = OutWrite = nullptr;
	Super::Deinitialize();
}

FString UVoiceSubsystem::Request(const FString& Text, int32 SpeakerId)
{
	const FString Key = FMD5::HashAnsiString(*FString::Printf(TEXT("%d|%s"), SpeakerId, *Text));
	const FString Path = CacheDir / (Key + TEXT(".wav"));
	if (Ready.Contains(Path) || Pending.Contains(Path)) return Path;
	if (FPaths::FileExists(Path)) { Ready.Add(Path); return Path; }
	if (!Proc.IsValid()) return Path;

	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("text"), Text);
	J->SetNumberField(TEXT("speaker_id"), SpeakerId);
	J->SetStringField(TEXT("output_file"), FPaths::ConvertRelativePathToFull(Path));
	FString Line;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
	FJsonSerializer::Serialize(J, W);
	FPlatformProcess::WritePipe(InWrite, Line); // appends newline
	Pending.Add(Path);
	return Path;
}

bool UVoiceSubsystem::Tick(float)
{
	// piper prints each finished output path on its own line.
	OutBuffer += FPlatformProcess::ReadPipe(OutRead);
	int32 Nl;
	while (OutBuffer.FindChar(TEXT('\n'), Nl))
	{
		const FString Done = OutBuffer.Left(Nl).TrimStartAndEnd();
		OutBuffer.MidInline(Nl + 1);
		for (auto It = Pending.CreateIterator(); It; ++It)
			if (FPaths::ConvertRelativePathToFull(*It) == Done) { Ready.Add(*It); It.RemoveCurrent(); break; }
	}
	return true;
}

UAudioComponent* UVoiceSubsystem::Play(UObject* WorldContext, const FString& Path)
{
	if (!bEnabled || !Ready.Contains(Path)) return nullptr;
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() < 44) return nullptr;

	// Minimal RIFF parse: find fmt + data chunks (piper: 16-bit PCM mono).
	int32 Rate = 22050, Channels = 1, DataOff = -1, DataLen = 0;
	for (int32 i = 12; i + 8 <= Bytes.Num();)
	{
		const int32 Len = *(int32*)&Bytes[i + 4];
		if (FMemory::Memcmp(&Bytes[i], "fmt ", 4) == 0) { Channels = *(int16*)&Bytes[i + 10]; Rate = *(int32*)&Bytes[i + 12]; }
		else if (FMemory::Memcmp(&Bytes[i], "data", 4) == 0) { DataOff = i + 8; DataLen = FMath::Min(Len, Bytes.Num() - DataOff); break; }
		i += 8 + Len + (Len & 1);
	}
	if (DataOff < 0) return nullptr;

	USoundWaveProcedural* W = NewObject<USoundWaveProcedural>();
	W->SetSampleRate(Rate);
	W->NumChannels = Channels;
	W->Duration = DataLen / float(Rate * Channels * 2);
	W->SoundGroup = SOUNDGROUP_Voice;
	W->bLooping = false;
	W->QueueAudio(&Bytes[DataOff], DataLen);
	return UGameplayStatics::SpawnSound2D(WorldContext, W, Volume);
}
