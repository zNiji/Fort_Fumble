// Portal Protect — shared gameplay types (PART 1)
#pragma once

#include "CoreMinimal.h"
#include "PortalProtectTypes.generated.h"

/** One enemy walkway from a map edge to the central tower. Requirement: >= 3 pathways. */
USTRUCT(BlueprintType)
struct FPortalPath
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FVector> Waypoints;

	UPROPERTY(BlueprintReadOnly)
	FVector SpawnLocation = FVector::ZeroVector;
};

/** Predetermined defender build site derived from terrain (never on a path). */
USTRUCT(BlueprintType)
struct FDefenderSlotData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	bool bOccupied = false;
};
