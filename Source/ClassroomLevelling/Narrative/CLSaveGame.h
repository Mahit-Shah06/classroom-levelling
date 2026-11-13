// Save slot: full narrative state + where the player stands. Slots "slot0..2" + "auto".
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "NarrativeTypes.h"
#include "CLSaveGame.generated.h"

UCLASS()
class CLASSROOMLEVELLING_API UCLSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() int32 Version = 1;
	UPROPERTY() FDateTime Saved;
	UPROPERTY() float PlayTime = 0.f;
	UPROPERTY() FName Area;
	UPROPERTY() FVector PlayerLocation = FVector::ZeroVector;
	UPROPERTY() FRotator PlayerRotation = FRotator::ZeroRotator;
	UPROPERTY() FName PlayerGender;
	UPROPERTY() int32 Seed = 0;
	UPROPERTY() FStatBlock Player;
	UPROPERTY() TMap<FName, float> Relationship;
	UPROPERTY() TSet<FName> Flags;
	UPROPERTY() TMap<FName, float> TagHistory;
	UPROPERTY() TMap<FName, FName> RoleBindings;
	UPROPERTY() TSet<FName> Played;
	UPROPERTY() FName Origin;
	UPROPERTY() FName Homeroom;
	UPROPERTY() int32 Level = 1;
	UPROPERTY() float Xp = 0.f;
	UPROPERTY() int32 PredHits = 0;
	UPROPERTY() int32 PredTotal = 0;
	UPROPERTY() int32 ChoicesMade = 0;
	UPROPERTY() int32 Rushed = 0;
};
