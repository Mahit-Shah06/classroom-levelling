#include "NPCStandIn.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "AIController.h"
#include "UI/GamePrefs.h"
#include "UObject/ConstructorHelpers.h"

static const TCHAR* GManny = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
static const TCHAR* GQuinn = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple");

ANPCStandIn::ANPCStandIn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::Spawned;

	GetCapsuleComponent()->InitCapsuleSize(38.f, 92.f);
	UCharacterMovementComponent* M = GetCharacterMovement();
	M->MaxWalkSpeed = 130.f;
	M->bOrientRotationToMovement = true;
	M->RotationRate = FRotator(0, 300, 0);
	M->bUseRVOAvoidance = false;
	bUseControllerRotationYaw = false;

	USkeletalMeshComponent* Mesh = GetMesh();
	Mesh->SetRelativeLocationAndRotation(FVector(0, 0, -92.f), FRotator(0, -90.f, 0));
	Mesh->bEnableUpdateRateOptimizations = true;
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Mesh->SetCachedMaxDrawDistance(7000.f);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FClassFinder<UAnimInstance> Anim(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (Anim.Succeeded()) Mesh->SetAnimInstanceClass(Anim.Class);

	Tag = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Tag"));
	Tag->SetupAttachment(RootComponent);
	Tag->SetRelativeLocation(FVector(0, 0, 118.f));
	Tag->SetHorizontalAlignment(EHTA_Center);
	Tag->SetWorldSize(16.f);
	Tag->SetTextRenderColor(FColor(235, 235, 235));

	Talk = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Talk"));
	Talk->SetupAttachment(RootComponent);
	Talk->SetRelativeLocation(FVector(0, 0, 140.f));
	Talk->SetHorizontalAlignment(EHTA_Center);
	Talk->SetWorldSize(30.f);
	Talk->SetText(FText::FromString(TEXT("...")));
	Talk->SetVisibility(false);
}

void ANPCStandIn::ApplyBody(USkeletalMeshComponent* Mesh, bool bFemale, const FLinearColor& Tint)
{
	if (!Mesh) return;
	if (USkeletalMesh* SK = LoadObject<USkeletalMesh>(nullptr, bFemale ? GQuinn : GManny)) Mesh->SetSkeletalMeshAsset(SK);
	for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i)
		if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(i))
			MID->SetVectorParameterValue(TEXT("Paint Tint"), Tint);
}

void ANPCStandIn::Init(FName InId, const FString& Name, bool bStaff, bool bFemale, float Height, const FVector& AreaCenter, const FVector2D& AreaSize)
{
	Id = InId; Home = AreaCenter; Range = AreaSize * 0.4f; Target = GetActorLocation();
	// Staff: dark suits. Students: mid-grey uniforms. Extras: lighter, varied.
	// Colour reads in menus; luminance keeps the B/W hierarchy (staff darkest, named students mid, extras varied).
	static const FLinearColor Suits[] = { FLinearColor(0.10f, 0.02f, 0.03f), FLinearColor(0.02f, 0.07f, 0.04f), FLinearColor(0.04f, 0.04f, 0.05f) };
	const uint32 H = GetTypeHash(InId);
	FLinearColor Tint = bStaff ? Suits[H % 3] : FLinearColor(0.06f, 0.10f, 0.30f); // navy uniform
	if (InId.IsNone()) Tint = FLinearColor::MakeFromHSV8((uint8)FMath::RandRange(0, 255), 120, (uint8)FMath::RandRange(110, 210));
	ApplyBody(GetMesh(), bFemale, Tint);
	SetActorScale3D(FVector(Height / 180.f));
	Tag->SetText(FText::FromString(Name));
	Tag->SetVisibility(!Name.IsEmpty());
	Wait = FMath::FRandRange(0.f, 4.f);
}

void ANPCStandIn::SetStaged(bool b)
{
	bStaged = b;
	if (b) GetCharacterMovement()->StopMovementImmediately();
}

void ANPCStandIn::FaceTowards(const FVector& P)
{
	FVector D = P - GetActorLocation(); D.Z = 0;
	if (!D.IsNearlyZero()) SetActorRotation(D.Rotation());
}

FVector ANPCStandIn::HeadLocation() const
{
	return GetMesh()->DoesSocketExist(TEXT("head")) ? GetMesh()->GetSocketLocation(TEXT("head")) : GetActorLocation() + FVector(0, 0, 70.f);
}

void ANPCStandIn::Tick(float Dt)
{
	Super::Tick(Dt);
	TalkT = bTalking ? TalkT + Dt : 0.f;
	Talk->SetVisibility(bTalking && !bHideTags);

	if (!Id.IsNone())
		if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			const FVector D = Cam->GetCameraLocation() - Tag->GetComponentLocation();
			Tag->SetWorldRotation(FRotator(0, D.Rotation().Yaw, 0));
			Talk->SetWorldRotation(FRotator(0, D.Rotation().Yaw, 0));
			Tag->SetVisibility(D.SizeSquared() < FMath::Square(1400.f) && !bStaged && !bHideTags && FGamePrefs::Get().bNameTags);
		}

	if (bStaged) return;
	// Idle wander inside the home area (cheap: one movement input per tick, no navmesh).
	FVector To = Target - GetActorLocation(); To.Z = 0;
	if (To.Size() < 40.f || Stuck > 2.5f)
	{
		Wait -= Dt;
		if (Wait <= 0.f)
		{
			Target = Home + FVector(FMath::FRandRange(-Range.X, Range.X), FMath::FRandRange(-Range.Y, Range.Y), 0);
			Wait = FMath::FRandRange(3.f, 9.f);
			Stuck = 0.f;
		}
		return;
	}
	AddMovementInput(To.GetSafeNormal(), 1.f);
	Stuck = GetVelocity().Size2D() < 20.f ? Stuck + Dt : 0.f; // walked into a wall: pick another spot
}
