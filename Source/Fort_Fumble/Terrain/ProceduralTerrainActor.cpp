#include "Terrain/ProceduralTerrainActor.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace PortalTerrainVisual
{
	// Solid, opaque engine material — pack MI_Grass01 parents M_TreeGrass (WPO wind +
	// masked atlas T_BaseColor) which rainbow-checkerboards and undulates on a heightfield.
	static UMaterialInterface* GetStableBaseMaterial()
	{
		return LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	static UMaterialInstanceDynamic* MakeTintedMID(UObject* Outer, const FLinearColor& Color)
	{
		UMaterialInterface* BaseMat = GetStableBaseMaterial();
		if (!BaseMat)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, Outer);
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
		return MID;
	}
}

AProceduralTerrainActor::AProceduralTerrainActor()
{
	PrimaryActorTick.bCanEverTick = false;

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	SetRootComponent(TerrainMesh);
	TerrainMesh->bUseAsyncCooking = true;

	// Do NOT assign pack foliage/atlas materials (MI_Grass01 / MI_DefaultPBR) to the
	// procedural ground — they expect card UVs and drive World Position Offset wind.
	// Stable tinted MIDs are created in BuildMesh from BasicShapeMaterial.

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TreeAsset(
		TEXT("/Game/RPGTinyFantasyForest/Mesh/TreePlant/SM_TreeB.SM_TreeB"));
	if (TreeAsset.Succeeded())
	{
		TreeMesh = TreeAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> RockA(
		TEXT("/Game/RPGTinyFantasyForest/Mesh/Rock/SM_RockA.SM_RockA"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RockF(
		TEXT("/Game/RPGTinyFantasyForest/Mesh/Rock/SM_RockF.SM_RockF"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RockG(
		TEXT("/Game/RPGTinyFantasyForest/Mesh/Rock/SM_RockG.SM_RockG"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CobbleA(
		TEXT("/Game/RPGTinyFantasyForest/Mesh/Rock/SM_CobbleA.SM_CobbleA"));
	if (RockA.Succeeded()) { RockMeshes.Add(RockA.Object); }
	if (RockF.Succeeded()) { RockMeshes.Add(RockF.Object); }
	if (RockG.Succeeded()) { RockMeshes.Add(RockG.Object); }
	if (CobbleA.Succeeded()) { RockMeshes.Add(CobbleA.Object); }
}

void AProceduralTerrainActor::BeginPlay()
{
	Super::BeginPlay();
	GenerateTerrain();
}

void AProceduralTerrainActor::GenerateTerrain()
{
	// Requirement: terrain different every launch via randomized seed.
	if (bUseRandomSeed)
	{
		Seed = FMath::RandRange(1, MAX_int32 / 2);
	}

	Resolution = FMath::Max(17, Resolution);
	if (Resolution % 2 == 0)
	{
		++Resolution; // keep odd so a true center cell exists
	}

	CenterX = Resolution / 2;
	CenterY = Resolution / 2;

	const int32 Count = Resolution * Resolution;
	Heights.SetNum(Count);
	PathMask.SetNumZeroed(Count);
	Paths.Reset();
	DefenderSlots.Reset();
	ClearEnvironmentDressing();
	ClearMapBorder();

	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			Heights[Index(X, Y)] = SampleHeight(X, Y);
		}
	}

	CarvePaths();
	BuildDefenderSlots();
	BuildMesh();

	// Tower site before dressing so plaza clearance can use world distance.
	TowerWorldLocation = GridToWorld(CenterX, CenterY, Heights[Index(CenterX, CenterY)]);
	TowerWorldLocation.Z += 20.f;

	SpawnEnvironmentDressing();
	SpawnMapBorder();

	UE_LOG(LogTemp, Warning, TEXT("[PortalProtect] Terrain generated with seed %d — %d paths, %d defender slots, %d dressing props, border walls on."),
		Seed, Paths.Num(), DefenderSlots.Num(), DressingComponents.Num());
}

float AProceduralTerrainActor::SampleHeight(int32 X, int32 Y) const
{
	// Lightweight value-noise style heightfield (no external assets).
	auto Hash01 = [this](int32 IX, int32 IY) -> float
	{
		uint32 N = static_cast<uint32>(IX * 374761393 + IY * 668265263 + Seed * 1274126177);
		N = (N ^ (N >> 13)) * 1274126177u;
		return static_cast<float>(N & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu);
	};

	const float FX = X * 0.12f;
	const float FY = Y * 0.12f;
	const int32 X0 = FMath::FloorToInt(FX);
	const int32 Y0 = FMath::FloorToInt(FY);
	const float TX = FMath::Frac(FX);
	const float TY = FMath::Frac(FY);
	const float SX = TX * TX * (3.f - 2.f * TX);
	const float SY = TY * TY * (3.f - 2.f * TY);

	const float A = Hash01(X0, Y0);
	const float B = Hash01(X0 + 1, Y0);
	const float C = Hash01(X0, Y0 + 1);
	const float D = Hash01(X0 + 1, Y0 + 1);
	const float Z = FMath::Lerp(FMath::Lerp(A, B, SX), FMath::Lerp(C, D, SX), SY);

	// Soft bowl so the center (tower) sits on flatter ground.
	const float NX = (X - CenterX) / static_cast<float>(CenterX);
	const float NY = (Y - CenterY) / static_cast<float>(CenterY);
	const float Dist = FMath::Sqrt(NX * NX + NY * NY);
	const float Bowl = FMath::Clamp(Dist, 0.f, 1.f);

	return (Z * 0.65f + Bowl * 0.35f) * MaxHeight;
}

bool AProceduralTerrainActor::IsInside(int32 X, int32 Y) const
{
	return X >= 0 && Y >= 0 && X < Resolution && Y < Resolution;
}

FVector AProceduralTerrainActor::GridToWorld(int32 X, int32 Y, float Height) const
{
	const float Half = (Resolution - 1) * CellSize * 0.5f;
	return GetActorLocation() + FVector(X * CellSize - Half, Y * CellSize - Half, Height);
}

FVector AProceduralTerrainActor::GetCellWorldLocation(int32 X, int32 Y, float ZOffset) const
{
	if (!IsInside(X, Y) || Heights.Num() == 0)
	{
		return GetActorLocation();
	}
	return GridToWorld(X, Y, Heights[Index(X, Y)] + ZOffset);
}

bool AProceduralTerrainActor::IsOnOrNearPath(int32 X, int32 Y, int32 Radius) const
{
	if (PathMask.Num() == 0 || !IsInside(X, Y))
	{
		return true;
	}

	const int32 R = FMath::Max(0, Radius);
	for (int32 OY = -R; OY <= R; ++OY)
	{
		for (int32 OX = -R; OX <= R; ++OX)
		{
			const int32 CX = X + OX;
			const int32 CY = Y + OY;
			if (IsInside(CX, CY) && PathMask[Index(CX, CY)] != 0)
			{
				return true;
			}
		}
	}
	return false;
}

bool AProceduralTerrainActor::TryGetNearestPathProgress(int32 X, int32 Y, int32& OutPathIndex, float& OutProgress, float& OutDist2D) const
{
	OutPathIndex = INDEX_NONE;
	OutProgress = 0.f;
	OutDist2D = TNumericLimits<float>::Max();

	if (Paths.Num() == 0 || !IsInside(X, Y))
	{
		return false;
	}

	const FVector CellWorld = GridToWorld(X, Y, 0.f);
	float BestDistSq = TNumericLimits<float>::Max();

	for (int32 PathIdx = 0; PathIdx < Paths.Num(); ++PathIdx)
	{
		const FPortalPath& Path = Paths[PathIdx];
		const int32 NumWP = Path.Waypoints.Num();
		if (NumWP < 2)
		{
			continue;
		}

		for (int32 WI = 0; WI < NumWP; ++WI)
		{
			const float DistSq = FVector::DistSquared2D(CellWorld, Path.Waypoints[WI]);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				OutPathIndex = PathIdx;
				OutProgress = static_cast<float>(WI) / static_cast<float>(NumWP - 1);
			}
		}
	}

	if (OutPathIndex == INDEX_NONE)
	{
		return false;
	}

	OutDist2D = FMath::Sqrt(BestDistSq);
	return OutDist2D <= CellSize * 3.5f;
}

bool AProceduralTerrainActor::TryGetRandomOffPathLocation(FVector& OutLocation, float ZOffset) const
{
	if (Heights.Num() == 0 || PathMask.Num() == 0)
	{
		return false;
	}

	TArray<FIntPoint> Candidates;
	Candidates.Reserve(Resolution * Resolution / 4);
	// Keep coins / player spawn inward of the rim border (cells 0 / Resolution-1).
	for (int32 Y = 3; Y < Resolution - 3; ++Y)
	{
		for (int32 X = 3; X < Resolution - 3; ++X)
		{
			// Keep coins / player spawn clear of the walkable road (not just the exact cell).
			if (IsOnOrNearPath(X, Y, 1))
			{
				continue;
			}
			const int32 DistToCenter = FMath::Abs(X - CenterX) + FMath::Abs(Y - CenterY);
			if (DistToCenter < 4)
			{
				continue; // keep tower plaza clear
			}
			Candidates.Add(FIntPoint(X, Y));
		}
	}

	if (Candidates.Num() == 0)
	{
		return false;
	}

	const FIntPoint& Pick = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	OutLocation = GetCellWorldLocation(Pick.X, Pick.Y, ZOffset);
	return true;
}

bool AProceduralTerrainActor::IsNearDefenderSlot(const FVector& WorldLoc) const
{
	const float MinDistSq = DressingMinDistFromPad * DressingMinDistFromPad;
	for (const FDefenderSlotData& Slot : DefenderSlots)
	{
		if (FVector::DistSquared2D(WorldLoc, Slot.Location) < MinDistSq)
		{
			return true;
		}
	}
	return false;
}

void AProceduralTerrainActor::ClearEnvironmentDressing()
{
	for (UStaticMeshComponent* Comp : DressingComponents)
	{
		if (Comp)
		{
			Comp->DestroyComponent();
		}
	}
	DressingComponents.Reset();
}

void AProceduralTerrainActor::ClearMapBorder()
{
	for (UStaticMeshComponent* Comp : BorderWallComponents)
	{
		if (Comp)
		{
			Comp->DestroyComponent();
		}
	}
	BorderWallComponents.Reset();
}

void AProceduralTerrainActor::SpawnMapBorder()
{
	// Continuous dark stone walls just outside the mesh rim so the FPS pawn bump-stops
	// at the edge. Path starts sit on cells 1..(Resolution-2), so they stay clear.
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh || Heights.Num() == 0)
	{
		return;
	}

	const float Half = (Resolution - 1) * CellSize * 0.5f;
	const float Thickness = FMath::Max(20.f, BorderWallThickness);
	const float WallH = FMath::Max(50.f, BorderWallHeight);
	// Sit just outside the outermost vertices; small overlap so corners seal.
	const float Rim = Half + Thickness * 0.5f;
	const float Length = (Half + Thickness) * 2.f;

	// Sample rim height so walls sit on the ground rather than floating.
	auto RimGroundZ = [this](int32 X, int32 Y) -> float
	{
		X = FMath::Clamp(X, 0, Resolution - 1);
		Y = FMath::Clamp(Y, 0, Resolution - 1);
		return Heights[Index(X, Y)];
	};
	const float GroundZ = FMath::Max(
		FMath::Max(RimGroundZ(0, CenterY), RimGroundZ(Resolution - 1, CenterY)),
		FMath::Max(RimGroundZ(CenterX, 0), RimGroundZ(CenterX, Resolution - 1)));
	const float WallCenterZ = GroundZ + WallH * 0.5f;

	UMaterialInstanceDynamic* WallMID = PortalTerrainVisual::MakeTintedMID(
		this, FLinearColor(0.12f, 0.11f, 0.10f));

	struct FWallSpec
	{
		FVector LocalCenter;
		FVector Scale; // Engine cube is 100uu; Scale = desired_size / 100
	};

	const float S = 0.01f; // 100uu cube → world size via scale
	const TArray<FWallSpec> Specs = {
		{ FVector(Rim, 0.f, WallCenterZ), FVector(Thickness * S, Length * S, WallH * S) },
		{ FVector(-Rim, 0.f, WallCenterZ), FVector(Thickness * S, Length * S, WallH * S) },
		{ FVector(0.f, Rim, WallCenterZ), FVector(Length * S, Thickness * S, WallH * S) },
		{ FVector(0.f, -Rim, WallCenterZ), FVector(Length * S, Thickness * S, WallH * S) },
	};

	for (int32 I = 0; I < Specs.Num(); ++I)
	{
		const FName CompName = *FString::Printf(TEXT("BorderWall_%d"), I);
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, CompName);
		if (!Comp)
		{
			continue;
		}

		Comp->SetupAttachment(GetRootComponent());
		Comp->SetStaticMesh(CubeMesh);
		Comp->SetRelativeLocation(Specs[I].LocalCenter);
		Comp->SetRelativeScale3D(Specs[I].Scale);
		Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Comp->SetCollisionObjectType(ECC_WorldStatic);
		Comp->SetCollisionResponseToAllChannels(ECR_Block);
		Comp->SetGenerateOverlapEvents(false);
		Comp->SetCastShadow(true);
		if (WallMID)
		{
			Comp->SetMaterial(0, WallMID);
		}
		Comp->RegisterComponent();
		BorderWallComponents.Add(Comp);
	}

	// Sparse rock markers along each rim for a clearer "stone border" read.
	if (RockMeshes.Num() == 0)
	{
		return;
	}

	FRandomStream Stream(Seed ^ 0xB0B0B0B0);
	const int32 RocksPerSide = FMath::Clamp(Resolution / 6, 8, 18);
	auto PlaceRock = [this, &Stream, GroundZ](const FVector& LocalXY, float Yaw)
	{
		UStaticMesh* Rock = RockMeshes[Stream.RandRange(0, RockMeshes.Num() - 1)].Get();
		if (!Rock)
		{
			return;
		}
		const FName CompName = *FString::Printf(TEXT("BorderRock_%d"), BorderWallComponents.Num());
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, CompName);
		if (!Comp)
		{
			return;
		}
		Comp->SetupAttachment(GetRootComponent());
		Comp->SetStaticMesh(Rock);
		Comp->SetRelativeLocation(FVector(LocalXY.X, LocalXY.Y, GroundZ + 10.f));
		Comp->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
		const float Scale = Stream.FRandRange(1.1f, 1.8f);
		Comp->SetRelativeScale3D(FVector(Scale, Scale, Scale * Stream.FRandRange(1.2f, 1.8f)));
		// Visual only — cubes already block; avoid double-collision snags on path mouths.
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetGenerateOverlapEvents(false);
		Comp->SetCastShadow(true);
		Comp->RegisterComponent();
		BorderWallComponents.Add(Comp);
	};

	const float RockRim = Half + Thickness * 0.15f;
	for (int32 I = 0; I < RocksPerSide; ++I)
	{
		const float T = (I + 0.5f) / static_cast<float>(RocksPerSide);
		const float Along = FMath::Lerp(-Half, Half, T);
		PlaceRock(FVector(RockRim, Along, 0.f), Stream.FRandRange(0.f, 360.f));
		PlaceRock(FVector(-RockRim, Along, 0.f), Stream.FRandRange(0.f, 360.f));
		PlaceRock(FVector(Along, RockRim, 0.f), Stream.FRandRange(0.f, 360.f));
		PlaceRock(FVector(Along, -RockRim, 0.f), Stream.FRandRange(0.f, 360.f));
	}
}

void AProceduralTerrainActor::SpawnEnvironmentDressing()
{
	if (Heights.Num() == 0 || PathMask.Num() == 0)
	{
		return;
	}

	FRandomStream Stream(Seed ^ 0xD00D1EAF);
	const int32 PathClearance = FMath::Max(1, DressingPathClearance);
	const int32 TowerClearCells = FMath::Max(3, DressingTowerClearanceCells);
	const float TowerClearRadius = FMath::Max(200.f, DressingTowerClearanceRadius);
	const float TowerClearRadiusSq = TowerClearRadius * TowerClearRadius;

	TArray<FIntPoint> Candidates;
	Candidates.Reserve(Resolution * Resolution / 4);
	for (int32 Y = 2; Y < Resolution - 2; ++Y)
	{
		for (int32 X = 2; X < Resolution - 2; ++X)
		{
			// Expand exclusion so large rock/tree meshes cannot overhang the road.
			if (IsOnOrNearPath(X, Y, PathClearance))
			{
				continue;
			}
			// Generous plaza around CentralTower / portal mesh (Chebyshev cells).
			const int32 ChebyshevToTower = FMath::Max(FMath::Abs(X - CenterX), FMath::Abs(Y - CenterY));
			if (ChebyshevToTower < TowerClearCells)
			{
				continue;
			}
			const FVector World = GridToWorld(X, Y, Heights[Index(X, Y)]);
			// World-radius backup so overhanging meshes cannot sit under/inside the portal.
			if (FVector::DistSquared2D(World, TowerWorldLocation) < TowerClearRadiusSq)
			{
				continue;
			}
			if (IsNearDefenderSlot(World))
			{
				continue;
			}
			Candidates.Add(FIntPoint(X, Y));
		}
	}

	for (int32 I = Candidates.Num() - 1; I > 0; --I)
	{
		const int32 J = Stream.RandRange(0, I);
		Candidates.Swap(I, J);
	}

	auto SpawnProp = [this, &Stream](UStaticMesh* Mesh, const FIntPoint& Cell, float ScaleMin, float ScaleMax)
	{
		if (!Mesh)
		{
			return;
		}

		const FName CompName = *FString::Printf(TEXT("Dressing_%d"), DressingComponents.Num());
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, CompName);
		if (!Comp)
		{
			return;
		}

		Comp->SetMobility(EComponentMobility::Movable);
		Comp->SetupAttachment(GetRootComponent());
		Comp->SetStaticMesh(Mesh);

		// FPS pawn bumps into props; enemies move by code along paths (dressing is off-path).
		// Uses mesh simple collision (pack rocks/trees); QueryAndPhysics for runtime components.
		Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Comp->SetCollisionObjectType(ECC_WorldStatic);
		Comp->SetCollisionResponseToAllChannels(ECR_Block);
		Comp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		// Projectiles are WorldDynamic and ignore props in ApplyHitTo; skip overlap spam.
		Comp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
		Comp->SetGenerateOverlapEvents(false);
		Comp->SetCastShadow(true);

		const FVector World = GridToWorld(Cell.X, Cell.Y, Heights[Index(Cell.X, Cell.Y)]);
		const FVector Local = World - GetActorLocation();
		Comp->SetRelativeLocation(Local);
		Comp->SetRelativeRotation(FRotator(0.f, Stream.FRandRange(0.f, 360.f), 0.f));
		const float Scale = Stream.FRandRange(ScaleMin, ScaleMax);
		Comp->SetRelativeScale3D(FVector(Scale));

		Comp->RegisterComponent();
		DressingComponents.Add(Comp);
	};

	int32 Cursor = 0;
	const int32 TreeCount = FMath::Min(NumTrees, Candidates.Num());
	for (int32 I = 0; I < TreeCount; ++I, ++Cursor)
	{
		SpawnProp(TreeMesh, Candidates[Cursor], 0.85f, 1.35f);
	}

	const int32 RockBudget = FMath::Min(NumRocks, Candidates.Num() - Cursor);
	for (int32 I = 0; I < RockBudget; ++I, ++Cursor)
	{
		UStaticMesh* Rock = RockMeshes.Num() > 0
			? RockMeshes[Stream.RandRange(0, RockMeshes.Num() - 1)].Get()
			: nullptr;
		SpawnProp(Rock, Candidates[Cursor], 0.55f, 1.25f);
	}
}

void AProceduralTerrainActor::CarvePaths()
{
	// Requirement: at least THREE distinct pathways leading to the central tower.
	FRandomStream Stream(Seed ^ 0xA5A5A5A5);
	const int32 PathCount = FMath::Max(3, NumPaths);

	for (int32 P = 0; P < PathCount; ++P)
	{
		const float BaseAngle = (2.f * PI) * P / PathCount;
		const float Jitter = Stream.FRandRange(-0.35f, 0.35f);
		const float Angle = BaseAngle + Jitter;

		// Start on the perimeter, end at center.
		const float Radius = (Resolution - 3) * 0.5f;
		int32 X = CenterX + FMath::RoundToInt(FMath::Cos(Angle) * Radius);
		int32 Y = CenterY + FMath::RoundToInt(FMath::Sin(Angle) * Radius);
		X = FMath::Clamp(X, 1, Resolution - 2);
		Y = FMath::Clamp(Y, 1, Resolution - 2);

		FPortalPath Path;
		Path.SpawnLocation = GridToWorld(X, Y, 0.f);

		TArray<FIntPoint> Cells;
		int32 Guard = Resolution * Resolution;
		while (Guard-- > 0)
		{
			Cells.Add(FIntPoint(X, Y));

			if (X == CenterX && Y == CenterY)
			{
				break;
			}

			// Biased walk toward center with seeded wiggle for unique routes each launch.
			const int32 DX = FMath::Clamp(CenterX - X, -1, 1);
			const int32 DY = FMath::Clamp(CenterY - Y, -1, 1);
			int32 StepX = DX;
			int32 StepY = DY;
			if (Stream.FRand() < 0.35f)
			{
				if (Stream.FRand() < 0.5f) { StepX = Stream.RandRange(-1, 1); }
				else { StepY = Stream.RandRange(-1, 1); }
			}

			int32 NX = FMath::Clamp(X + StepX, 1, Resolution - 2);
			int32 NY = FMath::Clamp(Y + StepY, 1, Resolution - 2);
			if (NX == X && NY == Y)
			{
				NX = FMath::Clamp(X + DX, 1, Resolution - 2);
				NY = FMath::Clamp(Y + DY, 1, Resolution - 2);
			}
			X = NX;
			Y = NY;
		}

		// Widen path and flatten height so enemies have a clear road.
		const float PathHeight = MaxHeight * 0.18f;
		for (const FIntPoint& Cell : Cells)
		{
			for (int32 OY = -PathHalfWidth; OY <= PathHalfWidth; ++OY)
			{
				for (int32 OX = -PathHalfWidth; OX <= PathHalfWidth; ++OX)
				{
					const int32 CX = Cell.X + OX;
					const int32 CY = Cell.Y + OY;
					if (!IsInside(CX, CY))
					{
						continue;
					}
					PathMask[Index(CX, CY)] = 1;
					Heights[Index(CX, CY)] = PathHeight;
				}
			}
		}

		// Ordered waypoints for enemy movement (downsample for smoother travel).
		for (int32 I = 0; I < Cells.Num(); ++I)
		{
			if (I == 0 || I == Cells.Num() - 1 || (I % 2) == 0)
			{
				const FIntPoint& C = Cells[I];
				Path.Waypoints.Add(GridToWorld(C.X, C.Y, Heights[Index(C.X, C.Y)] + 40.f));
			}
		}
		if (Path.Waypoints.Num() > 0)
		{
			Path.SpawnLocation = Path.Waypoints[0];
		}
		Paths.Add(Path);
	}

	// Flatten a small plaza under the tower.
	for (int32 OY = -2; OY <= 2; ++OY)
	{
		for (int32 OX = -2; OX <= 2; ++OX)
		{
			const int32 CX = CenterX + OX;
			const int32 CY = CenterY + OY;
			if (IsInside(CX, CY))
			{
				Heights[Index(CX, CY)] = MaxHeight * 0.2f;
			}
		}
	}
}

void AProceduralTerrainActor::BuildDefenderSlots()
{
	// At least 3 pads per pathway: off-path flanks, mid-to-late along each route, spaced apart.
	FRandomStream Stream(Seed ^ 0x5C5C5C5C);
	constexpr int32 MinPadsPerPath = 3;
	const int32 DesiredSlots = FMath::Max(MinPadsPerPath * Paths.Num(), MinPadsPerPath * 3);
	const int32 MinDistCells = 4;
	const float MinDistWorld = MinDistCells * CellSize;
	const float MinDistWorldSq = MinDistWorld * MinDistWorld;
	// ADefenderPlacementSpot uses Engine cylinder (origin center, half-height 50) × Z scale 0.15.
	const float PadHalfHeight = 50.f * 0.15f;

	// Spread three pads along each path: early-mid, mid, late (still off-path / not on road).
	const float BandMin[MinPadsPerPath] = { 0.34f, 0.50f, 0.66f };
	const float BandMax[MinPadsPerPath] = { 0.50f, 0.66f, 0.86f };

	struct FPadCandidate
	{
		FIntPoint Cell = FIntPoint::ZeroValue;
		float Angle = 0.f;
		float Progress = 0.f;
		int32 PathIndex = INDEX_NONE;
		float Score = 0.f;
	};

	TArray<FPadCandidate> Candidates;
	Candidates.Reserve(256);

	for (int32 Y = 2; Y < Resolution - 2; ++Y)
	{
		for (int32 X = 2; X < Resolution - 2; ++X)
		{
			if (PathMask[Index(X, Y)] != 0)
			{
				continue;
			}

			int32 NearestPathManhattan = MAX_int32;
			for (int32 OY = -2; OY <= 2; ++OY)
			{
				for (int32 OX = -2; OX <= 2; ++OX)
				{
					if (OX == 0 && OY == 0)
					{
						continue;
					}
					const int32 CX = X + OX;
					const int32 CY = Y + OY;
					if (IsInside(CX, CY) && PathMask[Index(CX, CY)] != 0)
					{
						NearestPathManhattan = FMath::Min(NearestPathManhattan, FMath::Abs(OX) + FMath::Abs(OY));
					}
				}
			}
			if (NearestPathManhattan > 2)
			{
				continue;
			}

			const int32 DistToCenter = FMath::Abs(X - CenterX) + FMath::Abs(Y - CenterY);
			if (DistToCenter < 6 || DistToCenter > FMath::RoundToInt(Resolution * 0.42f))
			{
				continue;
			}

			int32 PathIndex = INDEX_NONE;
			float Progress = 0.f;
			float DistToPath = 0.f;
			if (!TryGetNearestPathProgress(X, Y, PathIndex, Progress, DistToPath))
			{
				continue;
			}

			if (Progress < 0.30f || Progress > 0.88f)
			{
				continue;
			}

			const float Angle = FMath::Atan2(static_cast<float>(Y - CenterY), static_cast<float>(X - CenterX));
			const float ProgressScore = 1.f - FMath::Abs(Progress - 0.58f) * 2.2f;
			const float FlankScore = (NearestPathManhattan <= 1) ? 1.f : 0.55f;
			const float RingScore = 1.f - FMath::Abs(DistToCenter / static_cast<float>(FMath::Max(CenterX, 1)) - 0.55f);

			FPadCandidate Cand;
			Cand.Cell = FIntPoint(X, Y);
			Cand.Angle = Angle;
			Cand.Progress = Progress;
			Cand.PathIndex = PathIndex;
			Cand.Score = ProgressScore * 2.4f + FlankScore * 1.2f + RingScore + Stream.FRandRange(0.f, 0.15f);
			Candidates.Add(Cand);
		}
	}

	Candidates.Sort([](const FPadCandidate& A, const FPadCandidate& B)
	{
		return A.Score > B.Score;
	});

	auto WouldViolateSpacing = [&](const FIntPoint& Cell, float DistSqLimit) -> bool
	{
		const FVector World = GridToWorld(Cell.X, Cell.Y, Heights[Index(Cell.X, Cell.Y)] + PadHalfHeight);
		for (const FDefenderSlotData& Existing : DefenderSlots)
		{
			if (FVector::DistSquared2D(World, Existing.Location) < DistSqLimit)
			{
				return true;
			}
		}
		return false;
	};

	auto AddSlotAtCell = [&](const FIntPoint& Cell) -> bool
	{
		FDefenderSlotData Slot;
		Slot.Location = GridToWorld(Cell.X, Cell.Y, Heights[Index(Cell.X, Cell.Y)] + PadHalfHeight);
		Slot.bOccupied = false;
		DefenderSlots.Add(Slot);
		return true;
	};

	auto TryAddCandidate = [&](const FPadCandidate& Cand, float DistSqLimit) -> bool
	{
		if (DefenderSlots.Num() >= DesiredSlots || WouldViolateSpacing(Cand.Cell, DistSqLimit))
		{
			return false;
		}
		return AddSlotAtCell(Cand.Cell);
	};

	// 1) Guarantee ≥ MinPadsPerPath flanks per pathway, spread across progress bands.
	for (int32 PathIdx = 0; PathIdx < Paths.Num(); ++PathIdx)
	{
		for (int32 Band = 0; Band < MinPadsPerPath; ++Band)
		{
			const FPadCandidate* Best = nullptr;
			for (const FPadCandidate& Cand : Candidates)
			{
				if (Cand.PathIndex != PathIdx)
				{
					continue;
				}
				if (Cand.Progress < BandMin[Band] || Cand.Progress > BandMax[Band])
				{
					continue;
				}
				if (WouldViolateSpacing(Cand.Cell, MinDistWorldSq))
				{
					continue;
				}
				Best = &Cand;
				break; // Candidates already score-sorted
			}

			// Soften spacing, then ignore band, rather than leave a path under-padded.
			if (!Best)
			{
				for (const FPadCandidate& Cand : Candidates)
				{
					if (Cand.PathIndex != PathIdx)
					{
						continue;
					}
					if (WouldViolateSpacing(Cand.Cell, MinDistWorldSq * 0.45f))
					{
						continue;
					}
					Best = &Cand;
					break;
				}
			}

			if (Best)
			{
				TryAddCandidate(*Best, MinDistWorldSq * 0.35f);
			}
		}
	}

	// 2) Fill remaining toward DesiredSlots with angular spread.
	if (DefenderSlots.Num() < DesiredSlots)
	{
		const float AngleJitter = Stream.FRandRange(0.f, 2.f * PI);
		for (int32 SlotI = 0; SlotI < DesiredSlots && DefenderSlots.Num() < DesiredSlots; ++SlotI)
		{
			const float TargetAngle = AngleJitter + (2.f * PI) * SlotI / static_cast<float>(DesiredSlots);
			const FPadCandidate* Best = nullptr;
			float BestMetric = -TNumericLimits<float>::Max();

			for (const FPadCandidate& Cand : Candidates)
			{
				if (WouldViolateSpacing(Cand.Cell, MinDistWorldSq))
				{
					continue;
				}

				const float AngleDelta = FMath::Abs(FMath::FindDeltaAngleRadians(Cand.Angle, TargetAngle));
				const float AngleScore = 1.f - (AngleDelta / PI);
				const float Metric = AngleScore * 2.5f + Cand.Score;
				if (Metric > BestMetric)
				{
					BestMetric = Metric;
					Best = &Cand;
				}
			}

			if (Best)
			{
				TryAddCandidate(*Best, MinDistWorldSq);
			}
		}
	}

	// 3) Fallback: top remaining scores.
	for (const FPadCandidate& Cand : Candidates)
	{
		if (DefenderSlots.Num() >= DesiredSlots)
		{
			break;
		}
		TryAddCandidate(Cand, MinDistWorldSq * 0.5f);
	}

	UE_LOG(LogTemp, Log,
		TEXT("[PortalProtect] Defender pads: %d slots for %d paths (min %d/path)."),
		DefenderSlots.Num(), Paths.Num(), MinPadsPerPath);
}

void AProceduralTerrainActor::BuildMesh()
{
	TArray<FVector> GrassVerts;
	TArray<int32> GrassTris;
	TArray<FVector> GrassNormals;
	TArray<FVector2D> GrassUV;
	TArray<FLinearColor> GrassColors;
	TArray<FProcMeshTangent> GrassTangents;

	TArray<FVector> PathVerts;
	TArray<int32> PathTris;
	TArray<FVector> PathNormals;
	TArray<FVector2D> PathUV;
	TArray<FLinearColor> PathColors;
	TArray<FProcMeshTangent> PathTangents;

	auto AddQuad = [](TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector>& Normals,
		TArray<FVector2D>& UV, TArray<FLinearColor>& Colors, TArray<FProcMeshTangent>& Tangents,
		const FVector& V0, const FVector& V1, const FVector& V2, const FVector& V3,
		const FVector2D& UV0, const FVector2D& UV1, const FVector2D& UV2, const FVector2D& UV3,
		const FLinearColor& Color)
	{
		const int32 Base = Verts.Num();
		Verts.Add(V0);
		Verts.Add(V1);
		Verts.Add(V2);
		Verts.Add(V3);

		// Flat normal from the actual quad so hills shade correctly (no WPO needed).
		FVector N = FVector::CrossProduct(V2 - V0, V1 - V0).GetSafeNormal();
		if (N.IsNearlyZero() || N.Z < 0.f)
		{
			N = FVector::UpVector;
		}
		const FVector Tangent = (V1 - V0).GetSafeNormal();
		const FProcMeshTangent MeshTangent(Tangent.IsNearlyZero() ? FVector::ForwardVector : Tangent, false);

		for (int32 I = 0; I < 4; ++I)
		{
			Normals.Add(N);
			Colors.Add(Color);
			Tangents.Add(MeshTangent);
		}
		UV.Add(UV0);
		UV.Add(UV1);
		UV.Add(UV2);
		UV.Add(UV3);

		Tris.Add(Base + 0);
		Tris.Add(Base + 2);
		Tris.Add(Base + 1);
		Tris.Add(Base + 0);
		Tris.Add(Base + 3);
		Tris.Add(Base + 2);
	};

	// Distinct stable ground colors (opaque BasicShapeMaterial MIDs — no atlas / WPO).
	const FLinearColor GrassColor(0.22f, 0.58f, 0.24f);
	const FLinearColor PathColor(0.42f, 0.30f, 0.16f);
	// World-space UV tiling (~1 repeat per 4 cells) for any future textured override.
	const float UVTilesPerCell = 0.25f;

	for (int32 Y = 0; Y < Resolution - 1; ++Y)
	{
		for (int32 X = 0; X < Resolution - 1; ++X)
		{
			const FVector V0 = GridToWorld(X, Y, Heights[Index(X, Y)]) - GetActorLocation();
			const FVector V1 = GridToWorld(X + 1, Y, Heights[Index(X + 1, Y)]) - GetActorLocation();
			const FVector V2 = GridToWorld(X + 1, Y + 1, Heights[Index(X + 1, Y + 1)]) - GetActorLocation();
			const FVector V3 = GridToWorld(X, Y + 1, Heights[Index(X, Y + 1)]) - GetActorLocation();

			const FVector2D UV0(X * UVTilesPerCell, Y * UVTilesPerCell);
			const FVector2D UV1((X + 1) * UVTilesPerCell, Y * UVTilesPerCell);
			const FVector2D UV2((X + 1) * UVTilesPerCell, (Y + 1) * UVTilesPerCell);
			const FVector2D UV3(X * UVTilesPerCell, (Y + 1) * UVTilesPerCell);

			const bool bPath = PathMask[Index(X, Y)] || PathMask[Index(X + 1, Y)]
				|| PathMask[Index(X, Y + 1)] || PathMask[Index(X + 1, Y + 1)];

			if (bPath)
			{
				AddQuad(PathVerts, PathTris, PathNormals, PathUV, PathColors, PathTangents,
					V0, V1, V2, V3, UV0, UV1, UV2, UV3, PathColor);
			}
			else
			{
				AddQuad(GrassVerts, GrassTris, GrassNormals, GrassUV, GrassColors, GrassTangents,
					V0, V1, V2, V3, UV0, UV1, UV2, UV3, GrassColor);
			}
		}
	}

	TerrainMesh->ClearAllMeshSections();
	TerrainMesh->CreateMeshSection_LinearColor(0, GrassVerts, GrassTris, GrassNormals, GrassUV, GrassColors, GrassTangents, true);
	TerrainMesh->CreateMeshSection_LinearColor(1, PathVerts, PathTris, PathNormals, PathUV, PathColors, PathTangents, true);
	TerrainMesh->ContainsPhysicsTriMeshData(true);

	// Prefer explicit stable overrides; otherwise tint BasicShapeMaterial (never foliage WPO mats).
	if (GrassMaterial)
	{
		TerrainMesh->SetMaterial(0, GrassMaterial);
	}
	else if (UMaterialInstanceDynamic* GrassMID = PortalTerrainVisual::MakeTintedMID(this, GrassColor))
	{
		TerrainMesh->SetMaterial(0, GrassMID);
	}

	if (PathMaterial)
	{
		TerrainMesh->SetMaterial(1, PathMaterial);
	}
	else if (UMaterialInstanceDynamic* PathMID = PortalTerrainVisual::MakeTintedMID(this, PathColor))
	{
		TerrainMesh->SetMaterial(1, PathMID);
	}
}
