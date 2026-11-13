// Runtime TTS: one persistent piper process (ThirdParty/Piper), JSON lines over stdin, WAVs cached in Saved/VoiceCache.
// Synthesis is ~0.06x real-time on CPU, so whole scenes are queued ahead while the player reads.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "VoiceSubsystem.generated.h"

class UAudioComponent;

UCLASS()
class CLASSROOMLEVELLING_API UVoiceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Queue synthesis; returns the cache path (key). Cheap if already cached.
	FString Request(const FString& Text, int32 SpeakerId);
	bool IsReady(const FString& Path) const { return Ready.Contains(Path); }
	// Plays a ready line (2D). Returns null if not ready / voice disabled.
	UAudioComponent* Play(UObject* WorldContext, const FString& Path);
	bool IsAvailable() const { return Proc.IsValid(); }

	float Volume = 1.f;
	bool bEnabled = true;

private:
	bool Tick(float Dt);
	FProcHandle Proc;
	void* InRead = nullptr; void* InWrite = nullptr;   // our stdin -> piper
	void* OutRead = nullptr; void* OutWrite = nullptr; // piper stdout -> us
	FString CacheDir, OutBuffer;
	TSet<FString> Ready, Pending;
	FTSTicker::FDelegateHandle TickHandle;
};
