// PART 1 — Procedural 3D terrain mesh generated at runtime with randomized seed.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/PortalProtectTypes.h"
#include "ProceduralTerrainActor.generated.h"

class UProceduralMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * Generates a heightfield mesh at BeginPlay using a random seed so every launch differs.
 * Carves at least three pathways that all converge on the map center (tower site).
 * Also derives fixed defender placement slots that are never on a path.
 */
UCLASS()
class FORT_FUMBLE_API AProceduralTerrainActor : public AActor
{
	GENERATED_BODY()

public:
	AProceduralTerrainActor();

	virtual void BeginPlay() override;

	/** Rebuild mesh / paths using Seed (or a fresh random seed if bUseRandomSeed). */
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	void GenerateTerrain();

	UFUNCTION(BlueprintPure, Category = "Terrain")
	const TArray<FPortalPath>& GetPaths() const { return Paths; }

	UFUNCTION(BlueprintPure, Category = "Terrain")
	const TArray<FDefenderSlotData>& GetDefenderSlots() const { return DefenderSlots; }

	UFUNCTION(BlueprintPure, Category = "Terrain")
	FVector GetTowerLocation() const { return TowerWorldLocation; }

	UFUNCTION(BlueprintPure, Category = "Terrain")
	int32 GetSeed() const { return Seed; }

	/** Random walkable world location off enemy paths (for coins / player spawn). */
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	bool TryGetRandomOffPathLocation(FVector& OutLocation, float ZOffset = 40.f) const;

	/** World position for a grid cell using stored heights. */
	UFUNCTION(BlueprintPure, Category = "Terrain")
	FVector GetCellWorldLocation(int32 X, int32 Y, float ZOffset = 0.f) const;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	bool bUseRandomSeed = true;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	int32 Seed = 1;

	/** Odd grid resolution (forced odd at generate). Larger = bigger map. */
	UPROPERTY(EditAnywhere, Category = "Terrain")
	int32 Resolution = 73;

	/** World units between grid cells. Playable span ≈ (Resolution-1)*CellSize. */
	UPROPERTY(EditAnywhere, Category = "Terrain")
	float CellSize = 110.f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	float MaxHeight = 280.f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	int32 NumPaths = 3;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	int32 PathHalfWidth = 1;

	/** Visible blocking wall height (uu). */
	UPROPERTY(EditAnywhere, Category = "Terrain|Border", meta = (ClampMin = "50"))
	float BorderWallHeight = 280.f;

	/** Wall thickness (uu). Placed just outside the mesh rim. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Border", meta = (ClampMin = "20"))
	float BorderWallThickness = 80.f;

	UPROPERTY(EditAnywhere, Category = "Terrain|Dressing")
	int32 NumTrees = 32;

	UPROPERTY(EditAnywhere, Category = "Terrain|Dressing")
	int32 NumRocks = 26;

	UPROPERTY(EditAnywhere, Category = "Terrain|Dressing")
	float DressingMinDistFromPad = 180.f;

	/** Extra cells around PathMask to keep trees/rocks clear of pathways. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Dressing", meta = (ClampMin = "0", ClampMax = "4"))
	int32 DressingPathClearance = 2;

	/** Chebyshev cell radius around map center kept clear (portal plaza). */
	UPROPERTY(EditAnywhere, Category = "Terrain|Dressing", meta = (ClampMin = "3", ClampMax = "24"))
	int32 DressingTowerClearanceCells = 9;

	/** World-unit radius around tower/portal so large meshes cannot spawn under the arch. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Dressing", meta = (ClampMin = "200"))
	float DressingTowerClearanceRadius = 1100.f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	/** Optional override. Leave null for stable green opaque MID (do not use pack foliage MI_Grass*). */
	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Visual")
	TObjectPtr<UMaterialInterface> GrassMaterial;

	/** Optional override. Leave null for stable dirt opaque MID (avoid atlas/WPO pack mats on PMC). */
	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Visual")
	TObjectPtr<UMaterialInterface> PathMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Dressing")
	TObjectPtr<UStaticMesh> TreeMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Dressing")
	TArray<TObjectPtr<UStaticMesh>> RockMeshes;

private:
	float SampleHeight(int32 X, int32 Y) const;
	void CarvePaths();
	void BuildMesh();
	void BuildDefenderSlots();
	void ClearEnvironmentDressing();
	void SpawnEnvironmentDressing();
	void ClearMapBorder();
	void SpawnMapBorder();
	bool IsNearDefenderSlot(const FVector& WorldLoc) const;
	/** True if (X,Y) is a path cell or within Radius Chebyshev cells of one. */
	bool IsOnOrNearPath(int32 X, int32 Y, int32 Radius) const;
	/** Nearest path progress along waypoints: 0 = spawn, 1 = tower. Returns false if none nearby. */
	bool TryGetNearestPathProgress(int32 X, int32 Y, int32& OutPathIndex, float& OutProgress, float& OutDist2D) const;
	FVector GridToWorld(int32 X, int32 Y, float Height) const;
	bool IsInside(int32 X, int32 Y) const;
	int32 Index(int32 X, int32 Y) const { return Y * Resolution + X; }

	TArray<float> Heights;
	TArray<uint8> PathMask; // 1 = path cell (enemies only)
	TArray<FPortalPath> Paths;
	TArray<FDefenderSlotData> DefenderSlots;
	FVector TowerWorldLocation = FVector::ZeroVector;
	int32 CenterX = 0;
	int32 CenterY = 0;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> DressingComponents;

	/** Four edge walls (cubes) that block the player from leaving the map. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BorderWallComponents;
};
