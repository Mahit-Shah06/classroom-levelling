// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClassroomLevellingGameMode.h"
#include "World/HubWorld.h"
#include "Engine/World.h"

AClassroomLevellingGameMode::AClassroomLevellingGameMode()
{
	// stub
}

void AClassroomLevellingGameMode::StartPlay()
{
	if (!AHubWorld::Get(this)) GetWorld()->SpawnActor<AHubWorld>();
	Super::StartPlay();
}
