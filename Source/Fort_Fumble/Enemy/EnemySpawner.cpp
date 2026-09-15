// wave logic - rest, spawn mix, wait until path is clear

#include "Enemy/EnemySpawner.h"
#include "Enemy/EnemyUnit.h"
#include "Terrain/ProceduralTerrainActor.h"
#include "Engine/World.h"

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	EnemyClass = AEnemyUnit::StaticClass();
}

void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();
	// short intro rest so the player can look around before wave 1
	WavePhase = EWavePhase::Resting;
	CurrentWave = 0;
	PhaseTimer = 3.0f;
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

int32 AEnemySpawner::GetEnemiesRemaining() const
{
	int32 Alive = 0;
	for (const TWeakObjectPtr<AEnemyUnit>& Ref : AliveThisWave)
	{
		if (Ref.IsValid() && Ref->IsAlive())
		{
			++Alive;
		}
	}
	return Alive + EnemiesLeftToSpawn;
}

void AEnemySpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bSpawningEnabled || CachedPaths.Num() == 0)
	{
		return;
	}

	CleanupDeadRefs();

	switch (WavePhase)
	{
	case EWavePhase::Resting:
	{
		PhaseTimer -= DeltaTime;
		if (PhaseTimer <= 0.f)
		{
			BeginWave(CurrentWave + 1);
		}
		break;
	}
	case EWavePhase::Spawning:
	{
		PhaseTimer -= DeltaTime;
		if (PhaseTimer <= 0.f)
		{
			SpawnNextFromQueue();
			if (EnemiesLeftToSpawn > 0)
			{
				PhaseTimer = SpawnInterval;
			}
			else
			{
				WavePhase = EWavePhase::WaitingClear;
				ClearWaitTimer = 0.f;
			}
		}
		break;
	}
	case EWavePhase::WaitingClear:
	{
		ClearWaitTimer += DeltaTime;
		const int32 Alive = GetEnemiesRemaining();
		if (Alive <= 0 || ClearWaitTimer >= MaxClearWait)
		{
			WavePhase = EWavePhase::Resting;
			// later waves get a tiny bit less rest so pressure creeps up
			const float Scale = FMath::Clamp(1.f - (CurrentWave - 1) * 0.04f, 0.7f, 1.f);
			PhaseTimer = RestDuration * Scale;
			UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Wave %d cleared - resting %.1fs"),
				CurrentWave, PhaseTimer);
		}
		break;
	}
	default:
		break;
	}
}

void AEnemySpawner::BeginWave(int32 WaveNumber)
{
	CurrentWave = FMath::Max(1, WaveNumber);
	BuildWaveComposition(CurrentWave);
	EnemiesLeftToSpawn = SpawnQueue.Num();
	AliveThisWave.Reset();
	WavePhase = EWavePhase::Spawning;
	PhaseTimer = 0.25f;
	ClearWaitTimer = 0.f;

	UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Starting wave %d with %d enemies"),
		CurrentWave, EnemiesLeftToSpawn);
}

void AEnemySpawner::BuildWaveComposition(int32 WaveNumber)
{
	SpawnQueue.Reset();

	// base count grows with wave, seed keeps mixes repeatable per wave index
	FRandomStream Rng(WaveNumber * 9176 + 42);
	const int32 Count = FMath::Clamp(3 + WaveNumber * 2 + Rng.RandRange(0, 1), 3, 28);

	for (int32 i = 0; i < Count; ++i)
	{
		SpawnQueue.Add(PickTypeForWave(WaveNumber, Rng));
	}

	// shuffle a bit so tanks aren't always last
	for (int32 i = SpawnQueue.Num() - 1; i > 0; --i)
	{
		const int32 j = Rng.RandRange(0, i);
		SpawnQueue.Swap(i, j);
	}
}

EEnemyType AEnemySpawner::PickTypeForWave(int32 WaveNumber, FRandomStream& Rng) const
{
	// wave 1 = only slimes
	if (WaveNumber <= 1)
	{
		return EEnemyType::Slime;
	}

	const int32 Roll = Rng.RandRange(0, 99);

	if (WaveNumber == 2)
	{
		// introduce runners
		return (Roll < 55) ? EEnemyType::Slime : EEnemyType::Runner;
	}

	if (WaveNumber == 3)
	{
		// first tanks show up, still mostly slime/runner
		if (Roll < 45) return EEnemyType::Slime;
		if (Roll < 80) return EEnemyType::Runner;
		return EEnemyType::Tank;
	}

	// later waves: more mixed, slowly more tanks
	const int32 TankChance = FMath::Clamp(12 + (WaveNumber - 3) * 4, 12, 35);
	const int32 RunnerChance = FMath::Clamp(30 + (WaveNumber - 2) * 3, 30, 45);
	if (Roll < TankChance) return EEnemyType::Tank;
	if (Roll < TankChance + RunnerChance) return EEnemyType::Runner;
	return EEnemyType::Slime;
}

void AEnemySpawner::SpawnNextFromQueue()
{
	if (!EnemyClass || CachedPaths.Num() == 0 || SpawnQueue.Num() == 0)
	{
		EnemiesLeftToSpawn = 0;
		return;
	}

	const EEnemyType Type = SpawnQueue[0];
	SpawnQueue.RemoveAt(0);
	EnemiesLeftToSpawn = SpawnQueue.Num();

	// rotate through path start points so pressure spreads across lanes
	const FPortalPath& Path = CachedPaths[NextPathIndex % CachedPaths.Num()];
	NextPathIndex++;

	if (Path.Waypoints.Num() == 0)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AEnemyUnit* Enemy = GetWorld()->SpawnActor<AEnemyUnit>(
		EnemyClass, Path.Waypoints[0], FRotator::ZeroRotator, Params);
	if (Enemy)
	{
		Enemy->InitializeAsType(Type);
		Enemy->InitializeOnPath(Path.Waypoints);
		AliveThisWave.Add(Enemy);
	}
}

void AEnemySpawner::CleanupDeadRefs()
{
	for (int32 i = AliveThisWave.Num() - 1; i >= 0; --i)
	{
		if (!AliveThisWave[i].IsValid() || !AliveThisWave[i]->IsAlive())
		{
			AliveThisWave.RemoveAt(i);
		}
	}
}
