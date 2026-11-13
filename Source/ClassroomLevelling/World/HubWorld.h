// Procedural school hub (blockout, B/W). Everything is instanced static meshes from engine basic shapes:
// a handful of draw calls total, so it runs on low-spec PCs. Also owns NPC stand-ins, storylet markers and the cutscene camera.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HubWorld.generated.h"

class UInstancedStaticMeshComponent;
class ANPCStandIn;
class ACameraActor;
class UTextRenderComponent;

USTRUCT()
struct FHubArea
{
	GENERATED_BODY()
	FName Name;
	FVector Center = FVector::ZeroVector; // floor centre (Z = floor height)
	FVector2D Size = FVector2D(1000.f);
	UPROPERTY() TObjectPtr<UTextRenderComponent> Marker;
	bool bHasStory = false;
};

UCLASS()
class CLASSROOMLEVELLING_API AHubWorld : public AActor
{
	GENERATED_BODY()

public:
	AHubWorld();
	static AHubWorld* Get(const UObject* WorldContext);

	const FHubArea* Area(FName Name) const;
	FVector PlayerSpawn() const;

	// Cutscenes: put speakers in an arc in front of the player at an area, then frame shots.
	void Stage(FName Location, const TArray<FName>& NPCIds, APawn* Player);
	void Shot(FName ShotType, FName SubjectNPC, APawn* Player);
	void EndCutscene(APlayerController* PC);
	void SetTalking(FName NPC, bool bTalking);
	void MenuOrbit(APlayerController* PC, bool bOn);

	ACameraActor* Camera() const { return Cam; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;

private:
	void Build();
	void Box(int32 Tone, const FVector& Center, const FVector& Size);
	void Cyl(int32 Tone, const FVector& Base, float Diameter, float Height);
	void Room(const FVector& Center, const FVector2D& Size, float H, int32 DoorSide); // DoorSide 0..3 = -X,+X,-Y,+Y
	void AddArea(FName Name, const FVector& Center, const FVector2D& Size);
	void SpawnCast();
	void RefreshMarkers();
	ANPCStandIn* StandIn(FName Id) const;

	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Cubes; // per tone
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Cyls;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Spheres;
	UPROPERTY() TArray<FHubArea> Areas;
	UPROPERTY() TMap<FName, TObjectPtr<ANPCStandIn>> Actors;
	UPROPERTY() TObjectPtr<ACameraActor> Cam;

	float MarkerT = 0.f, OrbitT = 0.f, PanT = 0.f;
	bool bOrbit = false, bPan = false;
	FVector PanFrom, PanTo, PanLook;
};
