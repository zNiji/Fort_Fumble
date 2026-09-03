// placeable cannon - auto-aims and shoots slimes in range
// spawned on yellow pads when you spend coins
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DefenderUnit.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class AEnemyUnit;

UCLASS()
class FORT_FUMBLE_API ADefenderUnit : public AActor
{
	GENERATED_BODY()

public:
	ADefenderUnit();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Defender")
	void ApplyDamage(float Amount);

	UFUNCTION(BlueprintPure, Category = "Defender")
	bool IsAlive() const { return Health > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Defender")
	float GetHealth() const { return Health; }

	// how far pivot is above ground after scale - used when spawning on pad
	UFUNCTION(BlueprintPure, Category = "Defender")
	float GetPivotToGroundOffset() const { return PivotToGroundOffset; }

	UPROPERTY(EditAnywhere, Category = "Defender")
	float MaxHealth = 90.f;

	// needs to reach enemies that stop at ~AttackRange * 0.9
	UPROPERTY(EditAnywhere, Category = "Defender|Combat")
	float AttackRange = 750.f;

	UPROPERTY(EditAnywhere, Category = "Defender|Combat")
	float AttackDamage = 18.f;

	UPROPERTY(EditAnywhere, Category = "Defender|Combat")
	float AttackCooldown = 0.65f;

	// extra yaw if the imported mesh still points the wrong way
	UPROPERTY(EditAnywhere, Category = "Defender|Aim")
	float AimYawOffset = 0.f;

	// how fast the barrel visually tracks targets
	UPROPERTY(EditAnywhere, Category = "Defender|Aim", meta = (ClampMin = "0.1"))
	float AimInterpSpeed = 6.f;

	// pitch clamp while tracking (degrees)
	UPROPERTY(EditAnywhere, Category = "Defender|Aim")
	float AimMaxPitch = 18.f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

private:
	void UpdateAim(float DeltaTime);
	void TryAttack();
	AEnemyUnit* FindNearestEnemy() const;
	void RefreshColor();

	float Health = 90.f;
	float AttackTimer = 0.f;
	float BaseMeshScale = 1.f;
	float PivotToGroundOffset = 40.f;
	bool bUsingCannonMesh = false;
};
