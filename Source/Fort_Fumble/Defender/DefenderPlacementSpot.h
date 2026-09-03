// yellow disc where you can place a cannon - never on an enemy path
// highlights green when you aim at it, hides when occupied
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DefenderPlacementSpot.generated.h"

class UStaticMeshComponent;

UCLASS()
class FORT_FUMBLE_API ADefenderPlacementSpot : public AActor
{
	GENERATED_BODY()

public:
	ADefenderPlacementSpot();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "Placement")
	bool IsOccupied() const { return bOccupied; }

	UFUNCTION(BlueprintCallable, Category = "Placement")
	void SetOccupied(bool bInOccupied);

	UFUNCTION(BlueprintCallable, Category = "Placement")
	void SetHighlighted(bool bHighlight);

	// cylinder half-height (50) * pad z scale - actor origin is mesh center
	UFUNCTION(BlueprintPure, Category = "Placement")
	static float GetPadHalfHeight() { return 50.f * PadZScale; }

	// top of the disc - game mode adds cannon pivot offset on spawn
	UFUNCTION(BlueprintPure, Category = "Placement")
	FVector GetPadSurfaceLocation() const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> MarkerMesh;

	// ~120uu pad from engine cylinder diameter 100 * 1.2 xy scale
	static constexpr float PadXYScale = 1.2f;
	static constexpr float PadZScale = 0.15f;

private:
	void ApplyColor(const FLinearColor& Color);

	bool bOccupied = false;
};
