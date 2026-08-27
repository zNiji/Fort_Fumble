// PART 1 — Simple enemy projectile: flies toward a target, damages tower/defenders on hit.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class FORT_FUMBLE_API AEnemyProjectile : public AActor
{
	GENERATED_BODY()

public:
	AEnemyProjectile();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Aim at TargetActor (or Direction if null); Damage applied via ApplyDamage on hit. */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void InitProjectile(AActor* InTarget, float InDamage, float InSpeed, AActor* InInstigatorActor);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, Category = "Projectile")
	float Lifetime = 4.f;

	UPROPERTY(EditAnywhere, Category = "Projectile")
	float HomingStrength = 6.f;

	/** Soft hit radius vs intended target (cannons are large / elevated vs path height). */
	UPROPERTY(EditAnywhere, Category = "Projectile")
	float HitProximityRadius = 130.f;

protected:
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	void ApplyHitTo(AActor* Other);
	void DestroyProjectile();

	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY()
	TWeakObjectPtr<AActor> InstigatorActor;

	FVector Velocity = FVector::ZeroVector;
	float Damage = 16.f;
	float Speed = 900.f;
	float Age = 0.f;
	bool bConsumed = false;
};
