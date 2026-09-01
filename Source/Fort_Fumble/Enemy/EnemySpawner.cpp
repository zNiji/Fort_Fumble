// timed spawn, round-robin path picks

#include "Enemy/EnemySpawner.h"
#include "Enemy/EnemyUnit.h"
#include "Terrain/ProceduralTerrainActor.h"

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	EnemyClass = AEnemyUnit::StaticClass();
}

void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();
	SpawnTimer = SpawnInterval * 0.5f;
}

void AEnemySpawner::Configure(AProceduralTerrainActor* InTerrain)
{
	Terrain = InTerrain;
	CachedPaths.Reset();
	if (Terrain)
	{
		CachedPaths = Terrain->GetPaths();
	}
}

void AEnemySpawner::SetSpawningEnabled(bool bEnabled)
{
	bSpawningEnabled = bEnabled;
}

void AEnemySpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bSpawningEnabled || CachedPaths.Num() == 0)
	{
		return;
	}

	SpawnTimer -= DeltaTime;
	if (SpawnTimer <= 0.f)
	{
		SpawnEnemy();
		SpawnTimer = SpawnInterval;
	}
}

// spawn slime at next path start, pass full waypoint list
void AEnemySpawner::SpawnEnemy()
{
	if (!EnemyClass || CachedPaths.Num() == 0)
	{
		return;
	}

	// rotate through path start points so pressure spreads across lanes
	const FPortalPath& Path = CachedPaths[NextPathIndex % CachedPaths.Num()];
	NextPathIndex++;

	if (Path.Waypoints.Num() == 0)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AEnemyUnit* Enemy = GetWorld()->SpawnActor<AEnemyUnit>(EnemyClass, Path.Waypoints[0], FRotator::ZeroRotator, Params);
	if (Enemy)
	{
		Enemy->InitializeOnPath(Path.Waypoints);
	}
}
