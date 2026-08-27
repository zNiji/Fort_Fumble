// PART 1 — Central player tower: health, auto-attacks; game over when depleted.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CentralTower.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class AEnemyUnit;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTowerDestroyed);

UCLASS()
class FORT_FUMBLE_API ACentralTower : public AActor
{
	GENERATED_BODY()

public:
	ACentralTower();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Tower")
	void ApplyDamage(float Amount);

	UFUNCTION(BlueprintPure, Category = "Tower")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Tower")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Tower")
	bool IsAlive() const { return Health > 0.f; }

	UPROPERTY(BlueprintAssignable)
	FOnTowerDestroyed OnTowerDestroyed;

	UPROPERTY(EditAnywhere, Category = "Tower")
	float MaxHealth = 500.f;

	UPROPERTY(EditAnywhere, Category = "Tower|Combat")
	float AttackRange = 900.f;

	UPROPERTY(EditAnywhere, Category = "Tower|Combat")
	float AttackDamage = 22.f;

	UPROPERTY(EditAnywhere, Category = "Tower|Combat")
	float AttackCooldown = 0.75f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> TurretMesh;

	/** Portal material from RPGTinyFantasyForest (optional; assigned in constructor). */
	UPROPERTY(EditDefaultsOnly, Category = "Tower|Visual")
	TObjectPtr<UMaterialInterface> PortalMaterial;

private:
	void TryAttack();
	AEnemyUnit* FindNearestEnemy() const;
	void ApplyVisualColor();

	float Health = 500.f;
	float AttackTimer = 0.f;
	float BaseVisualScale = 1.35f;
};
