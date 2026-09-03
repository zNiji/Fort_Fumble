// slime enemy - follows path waypoints, stops to shoot tower or cannons
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyUnit.generated.h"

class USkeletalMeshComponent;
class USphereComponent;
class UAnimSequence;
class ACentralTower;
class ADefenderUnit;
class AEnemyProjectile;

UCLASS()
class FORT_FUMBLE_API AEnemyUnit : public AActor
{
	GENERATED_BODY()

public:
	AEnemyUnit();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void InitializeOnPath(const TArray<FVector>& InWaypoints);

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void ApplyDamage(float Amount);

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsAlive() const { return Health > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	float GetHealth() const { return Health; }

	UPROPERTY(EditAnywhere, Category = "Enemy")
	float MaxHealth = 70.f;

	UPROPERTY(EditAnywhere, Category = "Enemy")
	float MoveSpeed = 220.f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Combat")
	float AttackDamage = 17.f;

	// fire range in 2D - tuned for cell size 110 pads beside paths
	UPROPERTY(EditAnywhere, Category = "Enemy|Combat")
	float AttackRange = 750.f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Combat")
	float AttackCooldown = 1.05f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Combat")
	float ProjectileSpeed = 900.f;

	// how far out they'll notice defenders - doesn't stop walk until EngageStopFactor
	UPROPERTY(EditAnywhere, Category = "Enemy|Combat")
	float DefenderAggroRange = 750.f;

	// fraction of attack range where they actually stop moving (still shoot at full range)
	UPROPERTY(EditAnywhere, Category = "Enemy|Combat", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float EngageStopFactor = 0.9f;

	// extra reach on the portal when standing at path end / plaza
	UPROPERTY(EditAnywhere, Category = "Enemy|Combat")
	float TowerAttackRange = 850.f;

	// scale after auto-fit to TargetHeight
	UPROPERTY(EditAnywhere, Category = "Enemy|Visual", meta = (ClampMin = "0.05"))
	float MeshScale = 1.f;

	// how tall the slime should look on the path (uu)
	UPROPERTY(EditAnywhere, Category = "Enemy|Visual", meta = (ClampMin = "20"))
	float TargetHeight = 90.f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Visual")
	float TurnInterpSpeed = 8.f;

	// mesh rel yaw +90 plus this offset so the face points along movement
	UPROPERTY(EditAnywhere, Category = "Enemy|Visual")
	float MeshYawOffset = 180.f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Mesh;

private:
	void MoveAlongPath(float DeltaTime);
	// defenders first, else tower - returns true when in combat (may pause walk)
	bool TryAttackEnemyTargets(float DeltaTime);
	void FireProjectileAt(AActor* Target);
	void UpdateFacing(const FVector& WorldDirection, float DeltaTime);
	void UpdateLocomotionAnim(bool bMoving);
	ADefenderUnit* FindNearbyDefender(float Range) const;
	ACentralTower* FindTower() const;
	void RefreshDamageVisual();

	TArray<FVector> Waypoints;
	int32 WaypointIndex = 0;
	float Health = 70.f;
	float AttackTimer = 0.f;
	float BaseMeshScale = 1.f;
	bool bInitialized = false;
	bool bUsingMonsterMesh = false;
	bool bCombatEngaged = false;

	UPROPERTY()
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY()
	TObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY()
	TObjectPtr<UAnimSequence> CurrentAnim;
};
