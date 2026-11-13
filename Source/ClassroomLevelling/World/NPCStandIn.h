// Campus NPC: animated mannequin (Manny/Quinn by gender) driven by CharacterMovement so the template
// locomotion anim blueprint just works. Wanders its home area; stops and faces the player when staged.
// Low-spec: update-rate optimisation, anim only ticks when rendered, draw-distance culled.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NPCStandIn.generated.h"

class UTextRenderComponent;

UCLASS()
class CLASSROOMLEVELLING_API ANPCStandIn : public ACharacter
{
	GENERATED_BODY()

public:
	ANPCStandIn();
	// Id None = unnamed background extra.
	void Init(FName InId, const FString& Name, bool bStaff, bool bFemale, float Height, const FVector& AreaCenter, const FVector2D& AreaSize);
	void SetTalking(bool b) { bTalking = b; }
	void SetStaged(bool b);
	void FaceTowards(const FVector& P);
	FVector HeadLocation() const;
	FName Id;
	static inline bool bHideTags = false; // during cutscenes

	// Shared mesh/tint helpers (also used for the player body).
	static void ApplyBody(USkeletalMeshComponent* Mesh, bool bFemale, float Tone);

protected:
	virtual void Tick(float Dt) override;

private:
	UPROPERTY() TObjectPtr<UTextRenderComponent> Tag;
	UPROPERTY() TObjectPtr<UTextRenderComponent> Talk;
	FVector Home, Target; FVector2D Range;
	float Wait = 0.f, TalkT = 0.f, Stuck = 0.f;
	bool bTalking = false, bStaged = false;
};
