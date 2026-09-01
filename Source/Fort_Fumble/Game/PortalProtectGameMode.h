// game mode - boots the match, owns coins, placement budget, win/lose
// spawns terrain, tower, spawners, pads, drops player on the map
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PortalProtectGameMode.generated.h"

class AProceduralTerrainActor;
class ACentralTower;
class AEnemySpawner;
class ADefenderPlacementSpot;
class ACoinSpawner;

UCLASS()
class FORT_FUMBLE_API APortalProtectGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APortalProtectGameMode();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "PortalProtect")
	bool TryPlaceDefenderAtSpot(ADefenderPlacementSpot* Spot);

	UFUNCTION(BlueprintPure, Category = "PortalProtect")
	bool IsGameOver() const { return bGameOver; }

	UFUNCTION(BlueprintPure, Category = "PortalProtect")
	int32 GetDefendersRemaining() const { return DefendersRemaining; }

	UFUNCTION(BlueprintPure, Category = "PortalProtect")
	int32 GetCoinBalance() const { return CoinBalance; }

	UFUNCTION(BlueprintCallable, Category = "PortalProtect")
	void AddCoins(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "PortalProtect")
	int32 GetDefenderCost() const { return DefenderCost; }

	UFUNCTION(BlueprintPure, Category = "PortalProtect")
	FString GetStatusMessage() const { return StatusMessage; }

	UFUNCTION(BlueprintPure, Category = "PortalProtect")
	ACentralTower* GetTower() const { return Tower; }

	UFUNCTION(BlueprintPure, Category = "PortalProtect")
	int32 GetTerrainSeed() const;

	UFUNCTION(BlueprintCallable, Category = "PortalProtect")
	void NotifyTowerDestroyed();

	UFUNCTION(BlueprintCallable, Category = "PortalProtect")
	void PlacePlayerOnTerrain();

	// how many cannons you can still place - terrain spawns at least 3 pads per path
	UPROPERTY(EditAnywhere, Category = "PortalProtect")
	int32 StartingDefenders = 12;

	UPROPERTY(EditAnywhere, Category = "PortalProtect|Economy")
	int32 StartingCoins = 25;

	UPROPERTY(EditAnywhere, Category = "PortalProtect|Economy")
	int32 DefenderCost = 15;

protected:
	void SpawnWorld();
	void SetStatusMessage(const FString& Message, float Duration = 2.5f);
	void ClearStatusMessage();

	UPROPERTY()
	TObjectPtr<AProceduralTerrainActor> Terrain;

	UPROPERTY()
	TObjectPtr<ACentralTower> Tower;

	UPROPERTY()
	TObjectPtr<AEnemySpawner> Spawner;

	UPROPERTY()
	TObjectPtr<ACoinSpawner> CoinSpawner;

	UPROPERTY()
	TArray<TObjectPtr<ADefenderPlacementSpot>> PlacementSpots;

	FTimerHandle PlayerPlaceTimer;
	FTimerHandle StatusMessageTimer;
	int32 PlayerPlaceAttempts = 0;
	int32 DefendersRemaining = 12;
	int32 CoinBalance = 25;
	bool bGameOver = false;
	FString StatusMessage;
};
