#include "NPCStandIn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "UObject/ConstructorHelpers.h"

static UStaticMeshComponent* MakePart(AActor* Owner, const TCHAR* Name, UStaticMesh* Mesh, USceneComponent* Parent)
{
	UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(Parent);
	C->SetCastShadow(true);
	C->bCastDynamicShadow = true;
	return C;
}

ANPCStandIn::ANPCStandIn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.033f;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylM(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphM(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubM(TEXT("/Engine/BasicShapes/Cube.Cube"));

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Body = MakePart(this, TEXT("Body"), CylM.Object, RootComponent);
	Head = MakePart(this, TEXT("Head"), SphM.Object, RootComponent);
	Mouth = MakePart(this, TEXT("Mouth"), CubM.Object, Head);
	Mouth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mouth->SetCastShadow(false);

	Tag = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Tag"));
	Tag->SetupAttachment(RootComponent);
	Tag->SetHorizontalAlignment(EHTA_Center);
	Tag->SetWorldSize(18.f);
	Tag->SetTextRenderColor(FColor(230, 230, 230));
}

void ANPCStandIn::Init(FName InId, const FString& Name, bool bStaff, float Height, const FVector& AreaCenter, const FVector2D& AreaSize)
{
	Id = InId; Home = AreaCenter; Range = AreaSize * 0.4f; Target = GetActorLocation();
	const float BodyH = Height * 0.78f;
	Body->SetRelativeScale3D(FVector(0.5f, 0.5f, BodyH / 100.f));
	Body->SetRelativeLocation(FVector(0, 0, BodyH * 0.5f));
	HeadZ = BodyH + 18.f;
	Head->SetRelativeScale3D(FVector(0.36f));
	Head->SetRelativeLocation(FVector(0, 0, HeadZ));
	Mouth->SetRelativeScale3D(FVector(0.04f, 0.22f, 0.05f));
	Mouth->SetRelativeLocation(FVector(47.f, 0, -18.f));
	Tag->SetRelativeLocation(FVector(0, 0, HeadZ + 40.f));
	Tag->SetText(FText::FromString(Name));

	auto Tint = [](UStaticMeshComponent* C, float V)
	{
		UMaterialInstanceDynamic* M = C->CreateDynamicMaterialInstance(0);
		if (M) M->SetVectorParameterValue(TEXT("Color"), FLinearColor(V, V, V));
	};
	Tint(Body, bStaff ? 0.06f : 0.25f);
	Tint(Head, 0.85f);
	Tint(Mouth, 0.02f);
}

FVector ANPCStandIn::HeadLocation() const { return GetActorLocation() + FVector(0, 0, HeadZ); }

void ANPCStandIn::Tick(float Dt)
{
	Super::Tick(Dt);
	// Mouth flap (driven by talking flag; audio-envelope later).
	Talk = bTalking ? Talk + Dt : 0.f;
	const float Open = bTalking ? 0.05f + 0.14f * FMath::Abs(FMath::Sin(Talk * 14.f + FMath::Sin(Talk * 5.f))) : 0.05f;
	Mouth->SetRelativeScale3D(FVector(0.04f, 0.22f, Open));

	// Name tag faces the camera.
	if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector D = Cam->GetCameraLocation() - Tag->GetComponentLocation();
		Tag->SetWorldRotation(FRotator(0, D.Rotation().Yaw, 0));
		Tag->SetVisibility(D.SizeSquared() < FMath::Square(1500.f) && !bStaged && !bHideTags);
	}

	if (bStaged) return;
	// Idle wander within the home area.
	const FVector P = GetActorLocation();
	FVector To = Target - P; To.Z = 0;
	if (To.Size() < 20.f)
	{
		Wait -= Dt;
		if (Wait <= 0.f)
		{
			Target = Home + FVector(FMath::FRandRange(-Range.X, Range.X), FMath::FRandRange(-Range.Y, Range.Y), 0);
			Target.Z = P.Z;
			Wait = FMath::FRandRange(2.f, 7.f);
		}
		return;
	}
	const FVector Dir = To.GetSafeNormal();
	FHitResult Hit;
	SetActorLocationAndRotation(P + Dir * FMath::Min(70.f * Dt, To.Size()), FMath::RInterpTo(GetActorRotation(), Dir.Rotation(), Dt, 5.f), true, &Hit);
	if (Hit.bBlockingHit) Target = P; // blocked: pick a new spot next tick
}
