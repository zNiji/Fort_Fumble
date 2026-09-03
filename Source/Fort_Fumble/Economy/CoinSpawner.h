// keeps coins scattered on walkable off-path ground and respawns over time
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoinSpawner.generated.h"

class AProceduralTerrainActor;
class ACoinPickup;

UCLASS()
class FORT_FUMBLE_API ACoinSpawner : public AActor
{
	GENERATED_BODY()

public:
	ACoinSpawner();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Coins")
	void Configure(AProceduralTerrainActor* InTerrain);

	UPROPERTY(EditAnywhere, Category = "Coins")
	int32 InitialCoinCount = 16;

	UPROPERTY(EditAnywhere, Category = "Coins")
	int32 MaxActiveCoins = 22;

	UPROPERTY(EditAnywhere, Category = "Coins")
	float RespawnInterval = 3.5f;

	UPROPERTY(EditAnywhere, Category = "Coins")
	TSubclassOf<ACoinPickup> CoinClass;

private:
	void SpawnOneCoin();
	int32 CountActiveCoins() const;

	UPROPERTY()
	TObjectPtr<AProceduralTerrainActor> Terrain;

	float RespawnTimer = 0.f;
};
