// Cheap NPC stand-in (basic shapes + name tag). Idles/wanders inside its area; mouth flaps while talking.
// Swapped for generated models later; keeps the same API.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPCStandIn.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class CLASSROOMLEVELLING_API ANPCStandIn : public AActor
{
	GENERATED_BODY()

public:
	ANPCStandIn();
	void Init(FName InId, const FString& Name, bool bStaff, float Height, const FVector& AreaCenter, const FVector2D& AreaSize);
	void SetTalking(bool b) { bTalking = b; }
	void SetStaged(bool b) { bStaged = b; }
	FVector HeadLocation() const;
	FName Id;
	static inline bool bHideTags = false; // during cutscenes

protected:
	virtual void Tick(float Dt) override;

private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Mouth;
	UPROPERTY() TObjectPtr<UTextRenderComponent> Tag;
	FVector Home, Target; FVector2D Range;
	float Wait = 0.f, Talk = 0.f, HeadZ = 150.f;
	bool bTalking = false, bStaged = false;
};
