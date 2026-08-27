// PART 1 — First enemy type: spawns on path starts, walks to tower, fires projectiles at tower/defenders.
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

	/** Ranged fire distance (2D). Sized for CellSize 110 pads flanking paths. */
	UPROPERTY(EditAnywhere, Category = "Enemy|Combat")
	float AttackRange = 750.f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Combat")
	float AttackCooldown = 1.05f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Combat")
	float ProjectileSpeed = 900.f;

	/**
	 * Target-selection radius for defenders (≈ AttackRange). Does NOT freeze movement;
	 * pathing only pauses once inside AttackRange * EngageStopFactor.
	 */
	UPROPERTY(EditAnywhere, Category = "Enemy|Combat")
	float DefenderAggroRange = 750.f;

	/**
	 * Fraction of AttackRange at which pathing freezes (walk closer before stopping).
	 * Fire still uses full AttackRange so stopped units are clearly in mutual reach.
	 */
	UPROPERTY(EditAnywhere, Category = "Enemy|Combat", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float EngageStopFactor = 0.9f;

	/** Extra reach vs the portal when standing at path end / plaza. */
	UPROPERTY(EditAnywhere, Category = "Enemy|Combat")
	float TowerAttackRange = 850.f;

	/** Uniform visual scale after auto-fit to TargetHeight. */
	UPROPERTY(EditAnywhere, Category = "Enemy|Visual", meta = (ClampMin = "0.05"))
	float MeshScale = 1.f;

	/** Desired on-path height in uu (small-medium TD unit). */
	UPROPERTY(EditAnywhere, Category = "Enemy|Visual", meta = (ClampMin = "20"))
	float TargetHeight = 90.f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Visual")
	float TurnInterpSpeed = 8.f;

	/**
	 * Extra yaw baked into actor look-at. Mesh RelYaw +90 maps pack +Y→actor +X;
	 * +180 flips the mesh so the slime face points forward along movement.
	 */
	UPROPERTY(EditAnywhere, Category = "Enemy|Visual")
	float MeshYawOffset = 180.f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Mesh;

private:
	void MoveAlongPath(float DeltaTime);
	/** Prefer nearby defenders, else the tower; returns true if engaged (pause movement). */
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
