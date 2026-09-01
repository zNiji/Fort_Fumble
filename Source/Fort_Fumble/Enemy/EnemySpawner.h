// spawns slime waves on a timer from each path start, round-robin between paths
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/PortalProtectTypes.h"
#include "EnemySpawner.generated.h"

class AEnemyUnit;
class AProceduralTerrainActor;

UCLASS()
class FORT_FUMBLE_API AEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawner();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void Configure(AProceduralTerrainActor* InTerrain);

	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void SetSpawningEnabled(bool bEnabled);

	UPROPERTY(EditAnywhere, Category = "Spawner")
	float SpawnInterval = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Spawner")
	TSubclassOf<AEnemyUnit> EnemyClass;

private:
	void SpawnEnemy();

	UPROPERTY()
	TObjectPtr<AProceduralTerrainActor> Terrain;

	TArray<FPortalPath> CachedPaths;
	float SpawnTimer = 1.5f;
	bool bSpawningEnabled = true;
	int32 NextPathIndex = 0;
};
