// PART 1 — First defender type: placeable, auto-attacks, has health, can be attacked.
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

	/** Pivot→ground distance after mesh scale (for spawning on pad surface). */
	UFUNCTION(BlueprintPure, Category = "Defender")
	float GetPivotToGroundOffset() const { return PivotToGroundOffset; }

	UPROPERTY(EditAnywhere, Category = "Defender")
	float MaxHealth = 90.f;

	/** Must reach enemies that stop at ~AttackRange*0.9 (~675uu for 750 fire range). */
	UPROPERTY(EditAnywhere, Category = "Defender|Combat")
	float AttackRange = 750.f;

	UPROPERTY(EditAnywhere, Category = "Defender|Combat")
	float AttackDamage = 18.f;

	UPROPERTY(EditAnywhere, Category = "Defender|Combat")
	float AttackCooldown = 0.65f;

	/**
	 * Extra yaw on top of look-at. Prefer 0 after Mesh RelativeRotation aligns the barrel
	 * with actor forward (+X). Kept tunable if a specific import still needs a nudge.
	 */
	UPROPERTY(EditAnywhere, Category = "Defender|Aim")
	float AimYawOffset = 0.f;

	/** How quickly the cannon turns toward a combat target (visual only). */
	UPROPERTY(EditAnywhere, Category = "Defender|Aim", meta = (ClampMin = "0.1"))
	float AimInterpSpeed = 6.f;

	/** Soft pitch clamp while tracking (degrees). */
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
