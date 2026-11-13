#include "HubWorld.h"
#include "NPCStandIn.h"
#include "Narrative/NarrativeSubsystem.h"
#include "Narrative/StoryPlayerComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

static const float Tones[] = { 0.92f, 0.55f, 0.22f, 0.05f }; // white, grey, dark, black

AHubWorld::AHubWorld()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubM(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylM(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphM(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	auto Make = [&](TArray<TObjectPtr<UInstancedStaticMeshComponent>>& Arr, UStaticMesh* M, const TCHAR* Base)
	{
		for (int32 t = 0; t < UE_ARRAY_COUNT(Tones); ++t)
		{
			UInstancedStaticMeshComponent* C = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("%s%d"), Base, t));
			C->SetStaticMesh(M);
			C->SetupAttachment(RootComponent);
			C->SetMobility(EComponentMobility::Static);
			Arr.Add(C);
		}
	};
	Make(Cubes, CubM.Object, TEXT("Cube"));
	Make(Cyls, CylM.Object, TEXT("Cyl"));
	Make(Spheres, SphM.Object, TEXT("Sph"));
}

AHubWorld* AHubWorld::Get(const UObject* Ctx)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	if (!W) return nullptr;
	for (TActorIterator<AHubWorld> It(W); It; ++It) return *It;
	return nullptr;
}

void AHubWorld::BeginPlay()
{
	Super::BeginPlay();
	// Clear the template level's geometry; keep its sky/lights.
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It) It->Destroy();
	for (TActorIterator<ATextRenderActor> It(GetWorld()); It; ++It) It->Destroy();

	for (auto* Arr : { &Cubes, &Cyls, &Spheres })
		for (int32 t = 0; t < Arr->Num(); ++t)
			if (UMaterialInstanceDynamic* M = (*Arr)[t]->CreateDynamicMaterialInstance(0))
				M->SetVectorParameterValue(TEXT("Color"), FLinearColor(Tones[t], Tones[t], Tones[t]));

	Build();

	// Black & white look: one unbound post-process volume (desaturate + contrast + vignette). Accent colour comes later.
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (APostProcessVolume* PP = GetWorld()->SpawnActor<APostProcessVolume>(SP))
	{
		PP->bUnbound = true;
		FPostProcessSettings& S = PP->Settings;
		S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(0, 0, 0, 1);
		S.bOverride_ColorContrast = true; S.ColorContrast = FVector4(1.15f, 1.15f, 1.15f, 1);
		S.bOverride_VignetteIntensity = true; S.VignetteIntensity = 0.55f;
		S.bOverride_MotionBlurAmount = true; S.MotionBlurAmount = 0.f; // cleaner cuts + cheaper
		S.bOverride_AutoExposureBias = true; S.AutoExposureBias = 1.0f;
	}
	Cam = GetWorld()->SpawnActor<ACameraActor>(SP);
	if (Cam) { Cam->GetCameraComponent()->SetFieldOfView(55.f); Cam->GetCameraComponent()->bConstrainAspectRatio = false; }

	SpawnCast();
	RefreshMarkers();
}

// ---------------- geometry helpers ----------------
void AHubWorld::Box(int32 Tone, const FVector& C, const FVector& S)
{
	Cubes[Tone]->AddInstance(FTransform(FRotator::ZeroRotator, C, S / 100.f), true);
}

void AHubWorld::Cyl(int32 Tone, const FVector& Base, float D, float H)
{
	Cyls[Tone]->AddInstance(FTransform(FRotator::ZeroRotator, Base + FVector(0, 0, H * 0.5f), FVector(D / 100.f, D / 100.f, H / 100.f)), true);
}

void AHubWorld::Room(const FVector& C, const FVector2D& S, float H, int32 DoorSide)
{
	const float T = 30.f, Door = 360.f;
	Box(2, C + FVector(0, 0, 5), FVector(S.X, S.Y, 10)); // floor
	for (int32 Side = 0; Side < 4; ++Side)
	{
		const bool bX = Side < 2; // wall perpendicular to X
		const float Sign = (Side % 2) ? 1.f : -1.f;
		const float Len = bX ? S.Y : S.X;
		const FVector Mid = C + (bX ? FVector(Sign * S.X * 0.5f, 0, 0) : FVector(0, Sign * S.Y * 0.5f, 0)) + FVector(0, 0, H * 0.5f);
		const FVector Along = bX ? FVector(0, 1, 0) : FVector(1, 0, 0);
		auto Seg = [&](float From, float To) // along-wall range
		{
			const float L = To - From;
			if (L <= 1.f) return;
			Box(0, Mid + Along * (From + L * 0.5f), bX ? FVector(T, L, H) : FVector(L, T, H));
		};
		if (Side == DoorSide) { Seg(-Len * 0.5f, -Door * 0.5f); Seg(Door * 0.5f, Len * 0.5f); Box(0, Mid + FVector(0, 0, H * 0.5f - 40), bX ? FVector(T, Door, 80) : FVector(Door, T, 80)); }
		else Seg(-Len * 0.5f, Len * 0.5f);
	}
	Box(3, C + FVector(-S.X * 0.5f, 0, H + 5), FVector(T + 10, S.Y + T, 10)); // dark trim on the door wall
}

void AHubWorld::AddArea(FName Name, const FVector& Center, const FVector2D& Size)
{
	FHubArea A; A.Name = Name; A.Center = Center; A.Size = Size;
	UTextRenderComponent* M = NewObject<UTextRenderComponent>(this);
	M->SetupAttachment(RootComponent);
	M->RegisterComponent();
	M->SetWorldLocation(Center + FVector(0, 0, 260));
	M->SetText(FText::FromString(TEXT("!")));
	M->SetWorldSize(140.f);
	M->SetHorizontalAlignment(EHTA_Center);
	M->SetVerticalAlignment(EVRTA_TextCenter);
	M->SetTextRenderColor(FColor::White);
	M->SetVisibility(false);
	A.Marker = M;
	Areas.Add(A);
}

// ---------------- layout ----------------
// X = north. Gate (south) -> courtyard -> main building (corridor + rooms). Cafeteria east, rooftop tower west.
void AHubWorld::Build()
{
	Box(1, FVector(4000, 0, -10), FVector(16000, 16000, 20)); // ground

	// Gate: pillars + arch + fence line
	Cyl(3, FVector(0, -450, 0), 120, 500); Cyl(3, FVector(0, 450, 0), 120, 500);
	Box(3, FVector(0, 0, 540), FVector(140, 1100, 80));
	for (int32 i = -1; i <= 1; i += 2) Box(2, FVector(0, i * 3800, 120), FVector(40, 6600, 240));
	AddArea("gate", FVector(300, 0, 0), FVector2D(1400, 1400));

	// Courtyard: paths, trees, benches, fountain
	Box(0, FVector(2500, 0, 2), FVector(3600, 500, 4));
	Cyl(0, FVector(2600, 0, 0), 600, 60); Cyl(1, FVector(2600, 0, 60), 120, 160);
	for (int32 i = 0; i < 8; ++i)
	{
		const float X = 1300 + (i / 2) * 800, Y = (i % 2 ? 1 : -1) * 1300;
		Cyl(3, FVector(X, Y, 0), 50, 320); Spheres[0]->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Y, 420), FVector(2.6f)), true);
		Box(2, FVector(X, Y * 0.62f, 45), FVector(260, 70, 20)); Box(3, FVector(X, Y * 0.62f, 20), FVector(200, 50, 40));
	}
	AddArea("courtyard", FVector(2600, 900, 0), FVector2D(3000, 2600));

	// Main building: corridor along Y with rooms north of it.
	const float H = 420.f;
	Box(2, FVector(5300, 0, 5), FVector(700, 7600, 10));
	Box(0, FVector(4950, -2400, H * 0.5f), FVector(30, 2800, H)); Box(0, FVector(4950, 2400, H * 0.5f), FVector(30, 2800, H)); // facade w/ entrance gap
	Box(3, FVector(4950, 0, H - 40), FVector(30, 2000, 80));
	for (int32 i = -5; i <= 5; ++i) if (FMath::Abs(i) > 1) Box(1, FVector(5000, i * 640.f, 100), FVector(50, 300, 200)); // lockers
	AddArea("corridor", FVector(5300, -1600, 0), FVector2D(600, 3000));

	Room(FVector(6900, -2700, 0), FVector2D(2600, 2200), H + 100, 0); // hall
	Box(3, FVector(7900, -2700, 50), FVector(500, 1600, 100)); Box(1, FVector(7750, -2700, 160), FVector(80, 160, 120)); // stage + podium
	for (int32 r = 0; r < 5; ++r) for (int32 c = 0; c < 8; ++c) Box(2, FVector(6100 + r * 175.f, -3500 + c * 220.f, 45), FVector(80, 80, 90)); // seats
	AddArea("hall", FVector(7350, -2700, 0), FVector2D(1600, 1600));

	Room(FVector(6600, 0, 0), FVector2D(2000, 1800), H, 0); // classroom
	Box(3, FVector(7580, 0, 220), FVector(10, 1200, 260)); // chalkboard
	for (int32 r = 0; r < 4; ++r) for (int32 c = 0; c < 5; ++c)
	{
		const FVector D(5850 + r * 240.f, -700 + c * 350.f, 0);
		Box(1, D + FVector(0, 0, 75), FVector(110, 160, 8)); Box(2, D + FVector(0, 0, 37), FVector(100, 140, 74)); Box(3, D + FVector(-110, 0, 45), FVector(60, 60, 90));
	}
	AddArea("classroom", FVector(7150, 0, 0), FVector2D(1400, 1400));

	Room(FVector(6600, 2600, 0), FVector2D(2000, 1800), H, 0); // office
	Box(3, FVector(7000, 2600, 40), FVector(200, 600, 80)); for (int32 i = 0; i < 4; ++i) Box(1, FVector(7500, 1900 + i * 450.f, 150), FVector(120, 400, 300));
	AddArea("office", FVector(6400, 2600, 0), FVector2D(1400, 1400));

	// Cafeteria pavilion (east): roof on pillars, tables
	Box(2, FVector(2600, 4200, 5), FVector(2600, 2200, 10));
	for (int32 i = 0; i < 4; ++i) Cyl(0, FVector(1400 + (i % 2) * 2400.f, 3200 + (i / 2) * 2000.f, 0), 60, 450);
	Box(0, FVector(2600, 4200, 470), FVector(2800, 2400, 40));
	for (int32 r = 0; r < 3; ++r) for (int32 c = 0; c < 2; ++c) { const FVector T(1900 + r * 700.f, 3700 + c * 1000.f, 0); Box(0, T + FVector(0, 0, 75), FVector(500, 160, 10)); Box(3, T + FVector(0, 0, 35), FVector(460, 40, 70)); }
	Box(3, FVector(3700, 5150, 110), FVector(120, 90, 220)); // vending machine
	AddArea("cafeteria", FVector(2600, 4200, 0), FVector2D(2000, 1600));

	// Rooftop (west tower): ramp up to a fenced platform
	const FVector Top(2600, -4400, 600);
	Box(1, Top - FVector(0, 0, 300), FVector(1800, 1800, 600));
	for (int32 i = 0; i < 24; ++i) { const float Hh = (i + 1) * 25.f; Box(1, FVector(2600, -2540 - i * 40.f, Hh * 0.5f), FVector(400, 40, Hh)); } // stairs (25cm steps)
	Box(0, Top + FVector(-900, 0, 60), FVector(20, 1800, 120)); Box(0, Top + FVector(900, 0, 60), FVector(20, 1800, 120)); Box(0, Top + FVector(0, -900, 60), FVector(1800, 20, 120)); // railings
	Cyl(3, Top + FVector(500, -500, 0), 200, 250); // water tank
	AddArea("rooftop", Top, FVector2D(1400, 1400));
}

FVector AHubWorld::PlayerSpawn() const { return FVector(-600, 0, 120); }

const FHubArea* AHubWorld::Area(FName Name) const { return Areas.FindByPredicate([&](const FHubArea& A) { return A.Name == Name; }); }

ANPCStandIn* AHubWorld::StandIn(FName Id) const { const TObjectPtr<ANPCStandIn>* P = Actors.Find(Id); return P ? P->Get() : nullptr; }

void AHubWorld::SpawnCast()
{
	UNarrativeSubsystem* N = GetGameInstance() ? GetGameInstance()->GetSubsystem<UNarrativeSubsystem>() : nullptr;
	if (!N) return;
	static const FName StudentAreas[] = { "courtyard", "corridor", "cafeteria", "courtyard", "classroom", "gate" };
	int32 i = 0;
	for (const auto& KV : N->NPCs)
	{
		const bool bStaff = KV.Value.Kind != TEXT("student");
		const FHubArea* A = Area(bStaff ? FName(i % 2 ? "office" : "hall") : StudentAreas[i % UE_ARRAY_COUNT(StudentAreas)]);
		if (!A) continue;
		const FVector P = A->Center + FVector(FMath::FRandRange(-A->Size.X, A->Size.X) * 0.35f, FMath::FRandRange(-A->Size.Y, A->Size.Y) * 0.35f, 0);
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (ANPCStandIn* S = GetWorld()->SpawnActor<ANPCStandIn>(P, FRotator(0, FMath::FRandRange(0.f, 360.f), 0), SP))
		{
			const float Phys = KV.Value.Stats.General(EGeneralStat::Physique);
			S->Init(KV.Key, KV.Value.Name.ToString(), bStaff, 165.f + FMath::Clamp(Phys, 0.f, 100.f) * 0.25f, A->Center, A->Size);
			Actors.Add(KV.Key, S);
		}
		++i;
	}
}

void AHubWorld::RefreshMarkers()
{
	const UNarrativeSubsystem* N = GetGameInstance() ? GetGameInstance()->GetSubsystem<UNarrativeSubsystem>() : nullptr;
	for (FHubArea& A : Areas)
	{
		A.bHasStory = N && N->Flags.Contains(TEXT("ceremony_done")) && N->FindStorylet(NAME_None, A.Name) != nullptr;
		A.Marker->SetVisibility(A.bHasStory && !ANPCStandIn::bHideTags);
	}
}

void AHubWorld::Tick(float Dt)
{
	Super::Tick(Dt);
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (bOrbit && Cam)
	{
		if (PC && PC->GetViewTarget() != Cam) PC->SetViewTarget(Cam); // possession resets the view target

		OrbitT += Dt * 0.05f;
		const FVector C(3000, 0, 150);
		const FVector P = C + FVector(FMath::Cos(OrbitT) * 3200.f, FMath::Sin(OrbitT) * 3200.f, 900.f);
		Cam->SetActorLocationAndRotation(P, (C - P).Rotation());
	}
	if (bPan && Cam)
	{
		PanT = FMath::Min(PanT + Dt * 0.12f, 1.f);
		const FVector P = FMath::Lerp(PanFrom, PanTo, FMath::SmoothStep(0.f, 1.f, PanT));
		Cam->SetActorLocationAndRotation(P, (PanLook - P).Rotation());
	}

	MarkerT += Dt;
	for (FHubArea& A : Areas) if (A.bHasStory) A.Marker->SetWorldRotation(FRotator(0, MarkerT * 90.f, 0));
	if (MarkerT < 0.25f) return; // area checks at 4 Hz
	MarkerT = 0.f;
	RefreshMarkers();
	UStoryPlayerComponent* Story = PC ? PC->FindComponentByClass<UStoryPlayerComponent>() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Story || !Pawn || Story->IsBusy()) return;
	for (const FHubArea& A : Areas)
		if (A.bHasStory && FVector::Dist2D(Pawn->GetActorLocation(), A.Center) < 300.f && FMath::Abs(Pawn->GetActorLocation().Z - A.Center.Z) < 300.f)
		{
			Story->StartAt(A.Name);
			break;
		}
}

// ---------------- cutscenes ----------------
void AHubWorld::Stage(FName Location, const TArray<FName>& NPCIds, APawn* Player)
{
	const FHubArea* A = Area(Location);
	if (!A || !Player) return;
	for (auto& KV : Actors) KV.Value->SetStaged(false);
	ANPCStandIn::bHideTags = true;
	const FVector Center = A->Center;
	const FVector PPos = Center - FVector(250, 0, 0) + FVector(0, 0, 100);
	Player->SetActorLocationAndRotation(PPos, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
	if (AController* C = Player->GetController()) C->SetControlRotation(FRotator::ZeroRotator);
	for (int32 i = 0; i < NPCIds.Num(); ++i)
	{
		ANPCStandIn* S = StandIn(NPCIds[i]);
		if (!S) continue;
		const float Ang = (NPCIds.Num() == 1) ? 0.f : FMath::Lerp(-55.f, 55.f, i / float(NPCIds.Num() - 1));
		const FVector Pos = Center + FRotator(0, Ang, 0).RotateVector(FVector(420, 0, 0)) - FVector(250, 0, 0);
		S->SetStaged(true);
		S->SetActorLocationAndRotation(FVector(Pos.X, Pos.Y, Center.Z), ((PPos - Pos) * FVector(1, 1, 0)).Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
	PanLook = Center + FVector(0, 0, 150);
}

void AHubWorld::Shot(FName Type, FName Subject, APawn* Player)
{
	if (!Cam || !Player) return;
	bPan = false;
	const ANPCStandIn* S = StandIn(Subject);
	const FVector PHead = Player->GetActorLocation() + FVector(0, 0, 60);
	const FVector Look = S ? S->HeadLocation() : PanLook;
	const FVector Fwd = S ? S->GetActorForwardVector() : FVector(-1, 0, 0);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, (Look - PHead).GetSafeNormal());
	FVector P;
	if (Type == TEXT("close") && S) P = Look + Fwd * 170.f + FVector(0, 0, 5) + Right * 30.f;
	else if (Type == TEXT("over_shoulder")) P = PHead - (Look - PHead).GetSafeNormal() * 230.f + Right * 95.f + FVector(0, 0, 45);
	else if (Type == TEXT("low") && S) P = S->GetActorLocation() + Fwd * 220.f + FVector(0, 0, 40);
	else if (Type == TEXT("pan"))
	{
		PanFrom = PanLook + FVector(-700, -500, 250); PanTo = PanLook + FVector(-700, 500, 250); PanT = 0.f; bPan = true; P = PanFrom;
	}
	else P = PanLook + FVector(-900, 350, 450); // wide
	Cam->SetActorLocationAndRotation(P, (Look - P).Rotation());
	if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
		if (PC->GetViewTarget() != Cam) PC->SetViewTargetWithBlend(Cam, 0.5f, VTBlend_EaseInOut, 2.f);
}

void AHubWorld::EndCutscene(APlayerController* PC)
{
	bPan = false;
	ANPCStandIn::bHideTags = false;
	for (auto& KV : Actors) { KV.Value->SetStaged(false); KV.Value->SetTalking(false); }
	if (PC && PC->GetPawn()) PC->SetViewTargetWithBlend(PC->GetPawn(), 0.6f, VTBlend_EaseInOut, 2.f);
}

void AHubWorld::SetTalking(FName NPC, bool b) { if (ANPCStandIn* S = StandIn(NPC)) S->SetTalking(b); }

void AHubWorld::MenuOrbit(APlayerController* PC, bool bOn)
{
	bOrbit = bOn;
	if (!PC || !Cam) return;
	if (bOn) PC->SetViewTarget(Cam);
	else if (PC->GetPawn()) PC->SetViewTargetWithBlend(PC->GetPawn(), 1.0f, VTBlend_EaseInOut, 2.f);
}
