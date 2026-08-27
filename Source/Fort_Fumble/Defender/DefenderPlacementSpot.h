// Predetermined defender placement marker (never on a path).
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

	/** Engine cylinder half-height (50) × pad Z scale — actor origin is mesh center. */
	UFUNCTION(BlueprintPure, Category = "Placement")
	static float GetPadHalfHeight() { return 50.f * PadZScale; }

	/** World location of the pad's top surface (where a defender base should rest). */
	UFUNCTION(BlueprintPure, Category = "Placement")
	FVector GetPadSurfaceLocation() const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> MarkerMesh;

	/** Matches MarkerMesh XY scale (Engine cylinder diameter 100 → ~120uu pad). */
	static constexpr float PadXYScale = 1.2f;
	static constexpr float PadZScale = 0.15f;

private:
	void ApplyColor(const FLinearColor& Color);

	bool bOccupied = false;
};
