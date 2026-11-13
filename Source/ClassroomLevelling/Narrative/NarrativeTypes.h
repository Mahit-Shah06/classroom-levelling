// Data-driven narrative types. Content lives in Content/Data/*.json (AI-authored in dev).
#pragma once

#include "CoreMinimal.h"
#include "NarrativeTypes.generated.h"

// 8 general stats; each is the average of its sub-stats ("Stat.Sub" keys).
UENUM(BlueprintType)
enum class EGeneralStat : uint8
{
	Intellect, Charisma, Physique, Sanity, Cunning, Reputation, Funds, Luck, Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ESecretClass : uint8 { Onyx, Slate, Ash, Ivory };

// Sub-stats keyed "Intellect.Logic", "Charisma.Wit", etc. General = mean of its subs.
USTRUCT(BlueprintType)
struct FStatBlock
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, float> Sub;

	float General(EGeneralStat S) const;
	float Get(FName Key) const; // "Intellect" -> general, "Intellect.Logic" -> sub
	void Add(FName SubKey, float Delta);
	ESecretClass SecretClass() const; // from top-2 generals
};

// A choice carries tags (meaning) and effects. NPCs read the tags, not the text.
USTRUCT(BlueprintType)
struct FNarrativeChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Text;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> Requires;      // condition strings
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, float> Tags;       // e.g. betrayal:1, empathy:0.5
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, float> StatDeltas; // sub-stat keys
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> SetFlags;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Next;                    // next storylet (optional)
};

USTRUCT(BlueprintType)
struct FNPCProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Kind = "student"; // student | teacher | staff
	UPROPERTY(BlueprintReadOnly) FName Role;                                  // rolled per run: rival, ally, gremlin, president, ex1, broker, bg
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Gender = "m";        // m / f (picks Manny/Quinn body)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Voice;               // TTS voice id
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> RoleFit;       // roles this personality suits (empty = any)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FStatBlock Stats;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, float> Values; // tag -> how much they like it (-1..1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Perception = 0.5f;   // 0 = misreads often, 1 = reads truth
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Patience = 8.f;      // seconds before hesitation annoys them
};

// How an NPC took a choice. Misread = they interpreted a different tag than intended.
USTRUCT(BlueprintType)
struct FNPCReaction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName NPC;
	UPROPERTY(BlueprintReadOnly) float Delta = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bMisread = false;
	UPROPERTY(BlueprintReadOnly) FName PerceivedTag;
};

// One line of a cutscene/dialogue. Speaker: @role, @slot, staff role (@head, @homeroom), "narrator" or "system".
USTRUCT(BlueprintType)
struct FDialogueLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Speaker;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Text;  // may contain {slot} tokens
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Shot;  // wide | close | over_shoulder | pan | low (camera framing)
};

// Storylet: fires when its conditions hold. Exams are storylets with Slots filled from the save.
USTRUCT(BlueprintType)
struct FStorylet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Kind;             // scene, exam, quest, system
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> Conditions;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Priority = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOnce = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, FName> Slots; // slot -> selector (best_friend, rival, random_npc...)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Location;          // hub area: gate, hall, classroom, corridor, courtyard, cafeteria, rooftop, office
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDialogueLine> Lines; // cutscene played before the prompt
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName PromptSpeaker;      // who asks the final Text (default narrator)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Text;              // final prompt shown with choices; may contain {slot} tokens
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FNarrativeChoice> Choices;
};
