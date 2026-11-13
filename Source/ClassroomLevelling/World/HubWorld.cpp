#include "HubWorld.h"
#include "NPCStandIn.h"
#include "UI/GamePrefs.h"
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

// Real colours (menus show them); gameplay desaturates to the B/W look via post-process. Ordered light -> dark
// so the grayscale reading matches the original tone design.
static const float Tones[] = { 0.92f, 0.55f, 0.22f, 0.05f };
static const FLinearColor CubePal[] = { FLinearColor(0.60f, 0.54f, 0.44f), FLinearColor(0.32f, 0.55f, 0.30f), FLinearColor(0.62f, 0.24f, 0.16f), FLinearColor(0.06f, 0.08f, 0.16f) }; // cream, grass, brick, navy
static const FLinearColor CylPal[] = { FLinearColor(0.95f, 0.93f, 0.88f), FLinearColor(0.35f, 0.62f, 0.85f), FLinearColor(0.70f, 0.30f, 0.20f), FLinearColor(0.20f, 0.12f, 0.06f) };  // white, sky, clay, bark
static const FLinearColor SphPal[] = { FLinearColor(0.20f, 0.52f, 0.22f), FLinearColor(0.85f, 0.75f, 0.30f), FLinearColor(0.55f, 0.20f, 0.35f), FLinearColor(0.05f, 0.05f, 0.05f) }; // leaves, gold, plum

AHubWorld::AHubWorld()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubM(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylM(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphM(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	// The engine cube defaults to the prototype grid material; use the plain tintable one on everything.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Plain(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	auto Make = [&](TArray<TObjectPtr<UInstancedStaticMeshComponent>>& Arr, UStaticMesh* M, const TCHAR* Base)
	{
		for (int32 t = 0; t < UE_ARRAY_COUNT(Tones); ++t)
		{
			UInstancedStaticMeshComponent* C = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("%s%d"), Base, t));
			C->SetStaticMesh(M);
			if (Plain.Succeeded()) C->SetMaterial(0, Plain.Object);
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

	const FLinearColor* Pals[] = { CubePal, CylPal, SphPal };
	int32 k = 0;
	for (auto* Arr : { &Cubes, &Cyls, &Spheres })
	{
		for (int32 t = 0; t < Arr->Num(); ++t)
			if (UMaterialInstanceDynamic* M = (*Arr)[t]->CreateDynamicMaterialInstance(0))
				M->SetVectorParameterValue(TEXT("Color"), Pals[k][t]);
		++k;
	}

	Build();

	// Black & white look: one unbound post-process volume (desaturate + contrast + vignette). Accent colour comes later.
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (APostProcessVolume* PP = GetWorld()->SpawnActor<APostProcessVolume>(SP))
	{
		Post = PP;
		PP->bUnbound = true;
		FPostProcessSettings& S = PP->Settings;
		S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(1, 1, 1, 1); // starts in colour (title menu)
		S.bOverride_ColorContrast = true; S.ColorContrast = FVector4(1, 1, 1, 1);
		S.bOverride_VignetteIntensity = true; S.VignetteIntensity = 0.3f;
		S.bOverride_MotionBlurAmount = true; S.MotionBlurAmount = 0.f; // cleaner cuts + cheaper
		S.bOverride_AutoExposureBias = true; S.AutoExposureBias = 0.6f;
		// Print-like finish: grain, soft bloom and a slight toe so the B/W reads as film stock, not a grey filter.
		S.bOverride_FilmGrainIntensity = true; S.FilmGrainIntensity = 0.f;
		S.bOverride_BloomIntensity = true; S.BloomIntensity = 0.45f;
		S.bOverride_FilmToe = true; S.FilmToe = 0.62f;
		S.bOverride_FilmSlope = true; S.FilmSlope = 0.92f;
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

bool AHubWorld::Prop(FName Id, const FVector& Loc, float Yaw, float Height, float CullDist)
{
	TObjectPtr<UInstancedStaticMeshComponent>* Found = Props.Find(Id);
	if (!Found)
	{
		const FString N = Id.ToString();
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Generated/%s/%s/StaticMeshes/%s.%s"), *N, *N, *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
		UInstancedStaticMeshComponent* C = nullptr;
		if (M)
		{
			C = NewObject<UInstancedStaticMeshComponent>(this);
			C->SetStaticMesh(M);
			C->SetupAttachment(RootComponent);
			C->SetMobility(EComponentMobility::Static);
			C->SetCullDistances(CullDist * 0.6f, CullDist);
			C->SetCollisionProfileName(TEXT("BlockAll"));
			C->RegisterComponent();
		}
		Found = &Props.Add(Id, C);
	}
	UInstancedStaticMeshComponent* C = *Found;
	if (!C) return false;
	const FBox B = C->GetStaticMesh()->GetBoundingBox();
	const float S = Height / FMath::Max(B.GetSize().Z, 1.f);
	const FVector Base(B.GetCenter().X, B.GetCenter().Y, B.Min.Z); // model footprint centre goes on Loc
	const FRotator R(0, Yaw, 0);
	C->AddInstance(FTransform(R, Loc - R.RotateVector(Base * S), FVector(S)), true);
	return true;
}

// ---------------- layout ----------------
// X = north. Gate (south) -> courtyard -> main building (corridor + rooms). Cafeteria east, rooftop tower west.
void AHubWorld::Build()
{
	FRandomStream R(1337); // deterministic campus
	Box(1, FVector(2000, 0, -10), FVector(40000, 40000, 20)); // ground

	// ---- Outside: street, sidewalk, bus stop ("station"), city skyline ----
	Box(3, FVector(-3000, 0, 1), FVector(1400, 40000, 2));                       // road
	for (int32 i = -20; i <= 20; ++i) Box(0, FVector(-3000, i * 1000.f, 3), FVector(30, 400, 2)); // lane marks
	Box(0, FVector(-1600, 0, 8), FVector(1200, 40000, 16));                      // sidewalk
	if (!Prop("bus_stop", FVector(-2000, 1500, 16), 90, 300)) { Box(2, FVector(-2050, 1500, 260), FVector(400, 900, 20)); Box(2, FVector(-1900, 1500, 130), FVector(20, 900, 260)); Box(1, FVector(-2050, 1500, 50), FVector(120, 700, 20)); }
	 Cyl(3, FVector(-2200, 2100, 0), 20, 300); Box(0, FVector(-2200, 2100, 300), FVector(10, 160, 80)); // bench + sign
	AddArea("station", FVector(-1700, 1500, 16), FVector2D(1400, 2000));
	for (int32 i = 0; i < 70; ++i) // skyline beyond the road and around the campus
	{
		const float Hh = R.FRandRange(1200.f, 7000.f);
		FVector P;
		switch (i % 4)
		{
		case 0: case 1: P = FVector(R.FRandRange(-12000.f, -5000.f), R.FRandRange(-18000.f, 18000.f), 0); break;
		case 2: P = FVector(R.FRandRange(-3000.f, 19000.f), (R.FRand() < 0.5f ? -1.f : 1.f) * R.FRandRange(14500.f, 19000.f), 0); break;
		default: P = FVector(R.FRandRange(17500.f, 22000.f), R.FRandRange(-16000.f, 16000.f), 0);
		}
		Box(R.FRand() < 0.6f ? 3 : 2, P + FVector(0, 0, Hh * 0.5f), FVector(R.FRandRange(900.f, 2200.f), R.FRandRange(900.f, 2200.f), Hh));
	}

	// ---- Perimeter wall + gate ----
	const float WallH = 300.f;
	Box(2, FVector(0, -6300, WallH * 0.5f), FVector(60, 11500, WallH)); Box(2, FVector(0, 6300, WallH * 0.5f), FVector(60, 11500, WallH));
	Box(2, FVector(7500, -12050, WallH * 0.5f), FVector(15000, 60, WallH)); Box(2, FVector(7500, 12050, WallH * 0.5f), FVector(15000, 60, WallH));
	Box(2, FVector(15000, 0, WallH * 0.5f), FVector(60, 24160, WallH));
	Cyl(3, FVector(0, -450, 0), 120, 500); Cyl(3, FVector(0, 450, 0), 120, 500);
	Box(3, FVector(0, 0, 540), FVector(140, 1100, 80));
	Box(0, FVector(0, 0, 640), FVector(40, 900, 120)); // crest plate
	Prop("school_gate", FVector(-150, 0, 0), 90, 520, 20000);
	AddArea("gate", FVector(300, 0, 0), FVector2D(1400, 1400));

	// ---- Paths ----
	Box(0, FVector(2300, 0, 2), FVector(4600, 500, 4));         // gate -> main building
	Box(0, FVector(4600, 0, 2), FVector(400, 22000, 4));        // east-west spine in front of the main building
	Box(0, FVector(6500, 8000, 2), FVector(13000, 400, 4));     // east avenue (store, library, gym)
	Box(0, FVector(6500, -8500, 2), FVector(13000, 400, 4));    // west avenue (garden, dorm, field)
	Box(0, FVector(9500, 0, 2), FVector(400, 22000, 4));        // back path behind the main building
	for (int32 i = 0; i < 14; ++i) // trees + lamps along the avenues
	{
		const float X = 800 + i * 900.f;
		for (const float Y : { 7600.f, 8400.f, -8100.f, -8900.f })
		{
			if ((i + (int32)Y) % 2) { if (!Prop("tree_stylised", FVector(X, Y + FMath::Sign(Y) * 150.f, 0), i * 37.f, 520, 16000)) { Cyl(3, FVector(X, Y + FMath::Sign(Y) * 150.f, 0), 40, 300); Spheres[0]->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Y + FMath::Sign(Y) * 150.f, 390), FVector(2.2f)), true); } }
			else if (!Prop("street_lamp", FVector(X, Y, 0), Y > 0 ? 0 : 180, 450, 14000)) { Cyl(3, FVector(X, Y, 0), 16, 420); Spheres[0]->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Y, 430), FVector(0.35f)), true); }
		}
	}

	// ---- Courtyard: chessboard plaza under the fountain (the B/W nod) ----
	for (int32 r = 0; r < 8; ++r) for (int32 c = 0; c < 8; ++c)
		Box((r + c) % 2 ? 3 : 0, FVector(2600 - 700 + r * 200.f, -700 + c * 200.f, 3), FVector(200, 200, 6));
	if (!Prop("fountain", FVector(2600, 0, 6), 0, 260, 15000)) { Cyl(0, FVector(2600, 0, 0), 600, 60); Cyl(1, FVector(2600, 0, 60), 120, 160); }
	for (int32 i = 0; i < 8; ++i)
	{
		const float X = 1300 + (i / 2) * 800, Y = (i % 2 ? 1 : -1) * 1300;
		if (!Prop("tree_stylised", FVector(X, Y, 0), i * 53.f, 600, 16000)) { Cyl(3, FVector(X, Y, 0), 50, 320); Spheres[0]->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Y, 420), FVector(2.6f)), true); }
		if (!Prop("bench", FVector(X, Y * 0.62f, 0), Y > 0 ? 180 : 0, 90)) { Box(2, FVector(X, Y * 0.62f, 45), FVector(260, 70, 20)); Box(3, FVector(X, Y * 0.62f, 20), FVector(200, 50, 40)); }
	}
	AddArea("courtyard", FVector(2600, 900, 0), FVector2D(3000, 2600));

	// ---- Main building: corridor + hall, classroom, office ----
	const float H = 420.f;
	Box(2, FVector(5300, 0, 5), FVector(700, 7600, 10));
	Box(0, FVector(4950, -2400, H * 0.5f), FVector(30, 2800, H)); Box(0, FVector(4950, 2400, H * 0.5f), FVector(30, 2800, H));
	Box(3, FVector(4950, 0, H - 40), FVector(30, 2000, 80));
	for (int32 i = -5; i <= 5; ++i) if (FMath::Abs(i) > 1) if (!Prop("locker_row", FVector(5010, i * 640.f, 10), 90, 200)) Box(1, FVector(5000, i * 640.f, 100), FVector(50, 300, 200)); // lockers
	for (int32 i = -3; i <= 3; i += 2) Prop("potted_plant", FVector(5550, i * 900.f, 10), 0, 120);
	Prop("trophy_case", FVector(5550, 3300, 10), 180, 220);
	for (int32 i = -3; i <= 3; ++i) Box(3, FVector(4935, i * 1000.f + 500.f, 260), FVector(10, 500, 140)); // facade windows
	AddArea("corridor", FVector(5300, -1600, 0), FVector2D(600, 3000));

	Room(FVector(6900, -2700, 0), FVector2D(2600, 2200), H + 100, 0); // hall
	Box(3, FVector(7900, -2700, 50), FVector(500, 1600, 100)); if (!Prop("podium", FVector(7800, -2700, 100), -90, 130)) Box(1, FVector(7750, -2700, 160), FVector(80, 160, 120));
	for (int32 r = 0; r < 5; ++r) for (int32 c = 0; c < 8; ++c) Box(2, FVector(6100 + r * 175.f, -3500 + c * 220.f, 45), FVector(80, 80, 90));
	AddArea("hall", FVector(7350, -2700, 0), FVector2D(1600, 1600));

	Room(FVector(6600, 0, 0), FVector2D(2000, 1800), H, 0); // classroom
	if (!Prop("chalkboard", FVector(7540, 0, 10), -90, 260)) Box(3, FVector(7580, 0, 220), FVector(10, 1200, 260));
	for (int32 r = 0; r < 4; ++r) for (int32 c = 0; c < 5; ++c)
	{
		const FVector D(5850 + r * 240.f, -700 + c * 350.f, 10);
		if (!Prop("desk", D, 0, 76)) { Box(1, D + FVector(0, 0, 65), FVector(110, 160, 8)); Box(2, D + FVector(0, 0, 27), FVector(100, 140, 74)); }
		if (!Prop("chair", D + FVector(-90, 0, 0), 0, 90)) Box(3, D + FVector(-110, 0, 35), FVector(60, 60, 90));
	}
	AddArea("classroom", FVector(7150, 0, 0), FVector2D(1400, 1400));

	Room(FVector(6600, 2600, 0), FVector2D(2000, 1800), H, 0); // office
	Box(3, FVector(7000, 2600, 40), FVector(200, 600, 80)); for (int32 i = 0; i < 4; ++i) if (!Prop("bookshelf", FVector(7520, 1900 + i * 450.f, 10), -90, 260)) Box(1, FVector(7500, 1900 + i * 450.f, 150), FVector(120, 400, 300));
	AddArea("office", FVector(6400, 2600, 0), FVector2D(1400, 1400));

	// ---- Cafeteria pavilion ----
	Box(2, FVector(2600, 4200, 5), FVector(2600, 2200, 10));
	for (int32 i = 0; i < 4; ++i) Cyl(0, FVector(1400 + (i % 2) * 2400.f, 3200 + (i / 2) * 2000.f, 0), 60, 450);
	Box(0, FVector(2600, 4200, 470), FVector(2800, 2400, 40));
	for (int32 r = 0; r < 3; ++r) for (int32 c = 0; c < 2; ++c) { const FVector T(1900 + r * 700.f, 3700 + c * 1000.f, 10); if (!Prop("cafeteria_table", T, 0, 80)) { Box(0, T + FVector(0, 0, 65), FVector(500, 160, 10)); Box(3, T + FVector(0, 0, 25), FVector(460, 40, 70)); } }
	if (!Prop("vending_machine", FVector(3700, 5150, 10), 180, 200)) Box(3, FVector(3700, 5150, 110), FVector(120, 90, 220));
	AddArea("cafeteria", FVector(2600, 4200, 0), FVector2D(2000, 1600));

	// ---- Rooftop tower ----
	const FVector Top(2600, -4400, 600);
	Box(1, Top - FVector(0, 0, 300), FVector(1800, 1800, 600));
	for (int32 i = 0; i < 24; ++i) { const float Hh = (i + 1) * 25.f; Box(1, FVector(2600, -2540 - i * 40.f, Hh * 0.5f), FVector(400, 40, Hh)); }
	Box(0, Top + FVector(-900, 0, 60), FVector(20, 1800, 120)); Box(0, Top + FVector(900, 0, 60), FVector(20, 1800, 120)); Box(0, Top + FVector(0, -900, 60), FVector(1800, 20, 120));
	Cyl(3, Top + FVector(500, -500, 0), 200, 250);
	AddArea("rooftop", Top, FVector2D(1400, 1400));

	// ---- Convenience store (east, by the gate) ----
	Room(FVector(1600, 9600, 0), FVector2D(1600, 1400), 360, 2);
	for (int32 i = 0; i < 3; ++i) Box(1, FVector(1300 + i * 300.f, 9800, 90), FVector(80, 700, 180)); // shelves
	Box(3, FVector(2200, 9200, 55), FVector(200, 500, 110)); Box(0, FVector(1600, 8890, 330), FVector(800, 20, 60)); // counter + sign
	AddArea("store", FVector(1600, 9400, 0), FVector2D(1200, 1000));

	// ---- Library (east) ----
	Room(FVector(5600, 9800, 0), FVector2D(2800, 2600), 520, 2);
	for (int32 r = 0; r < 6; ++r) for (int32 k = -2; k <= 2; ++k) if (!Prop("bookshelf", FVector(4700 + r * 380.f, 10400 + k * 240.f, 0), 0, 240)) { if (k == 0) Box(3, FVector(4700 + r * 380.f, 10400, 130), FVector(60, 1200, 260)); } // stacks
	for (int32 t = 0; t < 3; ++t) { Box(0, FVector(4800 + t * 700.f, 9100, 75), FVector(400, 200, 10)); Box(2, FVector(4800 + t * 700.f, 9100, 37), FVector(360, 160, 74)); }
	AddArea("library", FVector(5600, 9200, 0), FVector2D(2000, 1200));

	// ---- Gym (north-east) ----
	Room(FVector(11500, 6000, 0), FVector2D(4200, 3400), 700, 0);
	Box(0, FVector(11500, 6000, 6), FVector(3000, 1800, 2)); // court lines
	for (int32 s = -1; s <= 1; s += 2) if (!Prop("basketball_hoop", FVector(11500 + s * 1650.f, 6000, 0), s > 0 ? 180 : 0, 400)) { Cyl(3, FVector(11500 + s * 1650.f, 6000, 0), 20, 380); Box(0, FVector(11500 + s * 1600.f, 6000, 380), FVector(10, 180, 120)); }
	for (int32 r = 0; r < 4; ++r) Box(2, FVector(11500, 7450 + r * 90.f, 30 + r * 60.f), FVector(3600, 90, 60 + r * 120.f)); // bleachers
	AddArea("gym", FVector(10400, 6000, 0), FVector2D(1600, 2000));

	// ---- Sports field + track (north-west) ----
	Box(2, FVector(11500, -6000, 2), FVector(6400, 4400, 4));   // track
	Box(1, FVector(11500, -6000, 4), FVector(5400, 3400, 4));   // infield
	Box(0, FVector(11500, -6000, 6), FVector(20, 3400, 2));     // halfway line
	for (int32 s = -1; s <= 1; s += 2) { Box(0, FVector(11500 + s * 2600.f, -6000, 120), FVector(40, 700, 240)); }
	for (int32 r = 0; r < 3; ++r) Box(2, FVector(11500, -8450 - r * 90.f, 30 + r * 60.f), FVector(4000, 90, 60 + r * 120.f));
	AddArea("field", FVector(10000, -6000, 4), FVector2D(2000, 2000));

	// ---- Dorm (west) ----
	Room(FVector(6000, -10000, 0), FVector2D(3200, 2600), 420, 3);
	for (int32 i = 0; i < 6; ++i) { const float X = 4800 + i * 480.f; if (!Prop("dorm_bed", FVector(X, -10700, 10), 90, 90)) { Box(0, FVector(X, -10700, 30), FVector(200, 400, 60)); Box(3, FVector(X, -10900, 70), FVector(200, 40, 140)); } }
	AddArea("dorm", FVector(6000, -9800, 0), FVector2D(2400, 1400));

	// ---- Garden (south-west) ----
	Cyl(3, FVector(2200, -9300, 0), 1200, 8); // pond
	for (int32 i = 0; i < 10; ++i) { const float X = 900 + (i % 5) * 600.f, Y = -10600 + (i / 5) * 2600.f; Box(2, FVector(X, Y, 60), FVector(400, 120, 120)); } // hedges
	for (int32 i = 0; i < 6; ++i) Cyl(1, FVector(3500 + (i % 2) * 400.f, -9800 + (i / 2) * 400.f, 0), 30, 280); // gazebo posts
	Box(0, FVector(3700, -9400, 290), FVector(700, 1100, 30));
	AddArea("garden", FVector(2400, -8900, 0), FVector2D(2400, 1600));

	// Small props fade out with distance (low-spec).
	for (auto* Arr : { &Cyls, &Spheres }) for (UInstancedStaticMeshComponent* C : *Arr) C->SetCullDistances(6000, 12000);
}

FName AHubWorld::AreaAt(const FVector& P) const
{
	for (const FHubArea& A : Areas)
		if (FMath::Abs(P.X - A.Center.X) < A.Size.X && FMath::Abs(P.Y - A.Center.Y) < A.Size.Y) return A.Name;
	return NAME_None;
}

FVector AHubWorld::PlayerSpawn() const { return FVector(-600, 0, 120); }

const FHubArea* AHubWorld::Area(FName Name) const { return Areas.FindByPredicate([&](const FHubArea& A) { return A.Name == Name; }); }

ANPCStandIn* AHubWorld::StandIn(FName Id) const { const TObjectPtr<ANPCStandIn>* P = Actors.Find(Id); return P ? P->Get() : nullptr; }

void AHubWorld::SpawnCast()
{
	UNarrativeSubsystem* N = GetGameInstance() ? GetGameInstance()->GetSubsystem<UNarrativeSubsystem>() : nullptr;
	if (!N) return;
	static const FName StudentAreas[] = { "courtyard", "corridor", "cafeteria", "library", "field", "gym", "garden", "store", "classroom", "gate" };
	static const FName StaffAreas[] = { "office", "hall", "library", "gym" };
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	auto Spawn = [&](FName Id, const FString& Name, bool bStaff, bool bFemale, float Height, FName AreaName)
	{
		const FHubArea* A = Area(AreaName);
		if (!A) return;
		const FVector P = A->Center + FVector(FMath::FRandRange(-0.35f, 0.35f) * A->Size.X, FMath::FRandRange(-0.35f, 0.35f) * A->Size.Y, 110);
		if (ANPCStandIn* S = GetWorld()->SpawnActor<ANPCStandIn>(P, FRotator(0, FMath::FRandRange(0.f, 360.f), 0), SP))
		{
			S->Init(Id, Name, bStaff, bFemale, Height, A->Center, A->Size);
			if (!Id.IsNone()) Actors.Add(Id, S);
		}
	};
	int32 i = 0;
	for (const auto& KV : N->NPCs)
	{
		const bool bStaff = KV.Value.Kind != TEXT("student");
		const float Phys = FMath::Clamp(KV.Value.Stats.General(EGeneralStat::Physique), 0.f, 100.f);
		Spawn(KV.Key, KV.Value.Name.ToString(), bStaff, KV.Value.Gender == TEXT("f"), (KV.Value.Gender == TEXT("f") ? 165.f : 175.f) + Phys * 0.12f,
			bStaff ? StaffAreas[i % UE_ARRAY_COUNT(StaffAreas)] : StudentAreas[i % UE_ARRAY_COUNT(StudentAreas)]);
		++i;
	}
	// Unnamed background students so the campus feels alive.
	for (int32 k = 0; k < 36; ++k)
		Spawn(NAME_None, FString(), false, FMath::RandBool(), FMath::FRandRange(160.f, 188.f), StudentAreas[k % UE_ARRAY_COUNT(StudentAreas)]);
}

void AHubWorld::RefreshMarkers()
{
	const UNarrativeSubsystem* N = GetGameInstance() ? GetGameInstance()->GetSubsystem<UNarrativeSubsystem>() : nullptr;
	for (FHubArea& A : Areas)
	{
		A.bHasStory = N && N->Flags.Contains(TEXT("ceremony_done")) && N->FindStorylet(NAME_None, A.Name) != nullptr;
		A.Marker->SetVisibility(A.bHasStory && !ANPCStandIn::bHideTags && FGamePrefs::Get().bMarkers);
	}
}

void AHubWorld::Tick(float Dt)
{
	Super::Tick(Dt);
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	// Colour <-> B/W fade (ticks while paused so the pause menu regains colour).
	const float RealDt = FApp::GetDeltaTime();
	if (Post && !FMath::IsNearlyEqual(Gray, GrayTarget))
	{
		Gray = FMath::FInterpConstantTo(Gray, GrayTarget, RealDt, 1.f / FMath::Max(GrayTime, 0.05f));
		const float G = FMath::SmoothStep(0.f, 1.f, Gray);
		FPostProcessSettings& S = Post->Settings;
		S.ColorSaturation = FVector4(1, 1, 1, 1) * (1.f - G) + FVector4(0, 0, 0, 1) * G; S.ColorSaturation.W = 1.f;
		const float C = FMath::Lerp(1.f, 1.15f, G); S.ColorContrast = FVector4(C, C, C, 1);
		S.VignetteIntensity = FMath::Lerp(0.3f, 0.55f, G);
		S.FilmGrainIntensity = 0.35f * G;
	}
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
	// Don't re-trigger the area you're standing in after a scene: re-arm once you walk away.
	if (const FHubArea* L = Area(Rearm)) if (FVector::Dist2D(Pawn->GetActorLocation(), L->Center) > 700.f) Rearm = NAME_None;
	for (const FHubArea& A : Areas)
		if (A.bHasStory && A.Name != Rearm && FVector::Dist2D(Pawn->GetActorLocation(), A.Center) < 300.f && FMath::Abs(Pawn->GetActorLocation().Z - A.Center.Z) < 300.f)
		{
			Story->StartAt(A.Name);
			break;
		}
}

// ---------------- cutscenes ----------------
void AHubWorld::Stage(FName Location, const TArray<FName>& NPCIds, APawn* Player)
{
	const FHubArea* A = Area(Location);
	if (!Player) return;
	if (!A) // in-place conversation (talk): freeze the speakers where they are and face each other
	{
		ANPCStandIn::bHideTags = true;
		for (const FName& Id : NPCIds) if (ANPCStandIn* S = StandIn(Id))
		{
			S->SetStaged(true); S->FaceTowards(Player->GetActorLocation());
			const FRotator Face = ((S->GetActorLocation() - Player->GetActorLocation()) * FVector(1, 1, 0)).Rotation();
			Player->SetActorRotation(Face);
			if (AController* C = Player->GetController()) C->SetControlRotation(Face);
		}
		PanLook = Player->GetActorLocation() + FVector(0, 0, 60);
		return;
	}
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
		S->SetActorLocationAndRotation(FVector(Pos.X, Pos.Y, Center.Z + 95.f), ((PPos - Pos) * FVector(1, 1, 0)).Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
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
	if (PC && PC->GetPawn()) Rearm = AreaAt(PC->GetPawn()->GetActorLocation());
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

ANPCStandIn* AHubWorld::NearestNamed(const FVector& P, float MaxDist) const
{
	ANPCStandIn* Best = nullptr; float BestD = MaxDist * MaxDist;
	for (const auto& KV : Actors)
		if (KV.Value) { const float D = FVector::DistSquared(KV.Value->GetActorLocation(), P); if (D < BestD) { BestD = D; Best = KV.Value; } }
	return Best;
}

void AHubWorld::SetGrayscale(bool bGray, float Seconds) { GrayTarget = bGray ? 1.f : 0.f; GrayTime = Seconds; }
