// shared structs for terrain, spawners, game mode - keeps everyone on the same data
#pragma once

#include "CoreMinimal.h"
#include "PortalProtectTypes.generated.h"

// slime = baseline, runner = fast cactus, tank = heavy chest monster
UENUM(BlueprintType)
enum class EEnemyType : uint8
{
	Slime UMETA(DisplayName = "Slime"),
	Runner UMETA(DisplayName = "Runner"),
	Tank UMETA(DisplayName = "Tank")
};

// one enemy route - waypoints from map edge to the tower
USTRUCT(BlueprintType)
struct FPortalPath
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FVector> Waypoints;

	UPROPERTY(BlueprintReadOnly)
	FVector SpawnLocation = FVector::ZeroVector;
};

// where a defender pad goes - terrain picks spots off the paths
USTRUCT(BlueprintType)
struct FDefenderSlotData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	bool bOccupied = false;
};
